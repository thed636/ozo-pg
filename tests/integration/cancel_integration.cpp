#include <ozo/connection_info.h>
#include <ozo/cancel.h>
#include <ozo/execute.h>
#include <ozo/request.h>
#include <ozo/shortcuts.h>

#include <boost/asio/spawn.hpp>
#include <boost/asio/detached.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define ASSERT_REQUEST_OK(ec, conn)\
    ASSERT_FALSE(ec) << ec.message() \
        << "|" << ozo::error_message(conn) \
        << "|" << ozo::get_error_context(conn) << std::endl

namespace {

namespace hana = boost::hana;

using namespace testing;

// Terminates a backend left running by a test.
//
// PostgreSQL does not notice that a client has gone away until it next tries to
// write to the socket, and pg_sleep() never does, so a statement abandoned by
// the client keeps running server-side until it completes on its own. A test
// that deliberately fails to cancel such a statement therefore has to clean up
// after itself, or it leaks one backend per run for the duration of the sleep.
void terminate_backend(std::int32_t pid) {
    using namespace ozo::literals;
    using namespace std::chrono_literals;

    ASSERT_NE(pid, 0) << "no backend pid was captured, cannot clean up";

    ozo::io_context io;
    boost::asio::spawn(io, [&io, pid](auto yield) {
        ozo::error_code ec;
        ozo::rows_of<bool> terminated;
        const auto conn = ozo::request(ozo::connection_info(OZO_PG_TEST_CONNINFO)[io],
            "SELECT pg_terminate_backend("_SQL + pid + ")"_SQL, 5s,
            ozo::into(terminated), yield[ec]);
        ASSERT_REQUEST_OK(ec, conn);
        EXPECT_THAT(terminated, ElementsAre(std::make_tuple(true)));
    }, boost::asio::detached);
    io.run();
}

TEST(cancel, should_cancel_operation) {
    using namespace ozo::literals;
    using namespace std::chrono_literals;
    using namespace hana::literals;

    ozo::io_context io;
    boost::asio::steady_timer timer(io);

    boost::asio::spawn(io, [&io, &timer](auto yield){
        const ozo::connection_info conn_info(OZO_PG_TEST_CONNINFO);
        ozo::error_code ec;
        auto conn = ozo::get_connection(conn_info[io], yield[ec]);
        EXPECT_FALSE(ec);
        boost::asio::spawn(yield, [&io, &timer, handle = get_cancel_handle(conn)](auto yield) mutable {
            timer.expires_after(1s);
            ozo::error_code ec;
            timer.async_wait(yield[ec]);
            if (!ec) {
                // Guard is needed since cancel will be served with external
                // system executor, so we need to preserve our io_context from
                // stop until all the operation processed properly
                auto guard = boost::asio::make_work_guard(io);
                ozo::cancel(std::move(handle), io, 5s, yield[ec]);
            }
        }, boost::asio::detached);
        ozo::execute(conn, "SELECT pg_sleep(1000000)"_SQL, yield[ec]);
        EXPECT_EQ(ec, ozo::sqlstate::query_canceled);
    }, boost::asio::detached);

    io.run();
}

TEST(cancel, should_stop_cancel_operation_on_zero_timeout) {
    using namespace ozo::literals;
    using namespace std::chrono_literals;
    using namespace hana::literals;

    ozo::io_context io;
    ozo::io_context dummy_io;
    boost::asio::steady_timer timer(io);
    std::int32_t backend_pid = 0;

    boost::asio::spawn(io, [&io, &timer, &dummy_io, &backend_pid](auto yield){
        const ozo::connection_info conn_info(OZO_PG_TEST_CONNINFO);
        ozo::error_code ec;
        auto conn = ozo::get_connection(conn_info[io], yield[ec]);
        EXPECT_FALSE(ec);

        // The cancellation below is expected to time out, so nothing will stop
        // the statement this connection is about to run. Record which backend
        // executes it so the test can terminate it once io.run() returns.
        ozo::rows_of<std::int32_t> pid_row;
        ozo::request(conn, "SELECT pg_backend_pid()"_SQL, 5s, ozo::into(pid_row), yield[ec]);
        EXPECT_FALSE(ec);
        EXPECT_THAT(pid_row, SizeIs(1));
        if (!pid_row.empty()) {
            backend_pid = std::get<0>(pid_row.front());
        }

        boost::asio::spawn(yield, [&io, &timer, handle = get_cancel_handle(conn, dummy_io.get_executor())](auto yield) mutable {
            timer.expires_after(1s);
            ozo::error_code ec;
            timer.async_wait(yield[ec]);
            if (!ec) {
                // Guard is needed since cancel will be served with external
                // system executor, so we need to preserve our io_context from
                // stop until all the operation processed properly
                auto guard = boost::asio::make_work_guard(io);
                ozo::cancel(std::move(handle), io, 0s, yield[ec]);
                EXPECT_EQ(ec, boost::asio::error::timed_out);
            }
        }, boost::asio::detached);
        ozo::execute(conn, "SELECT pg_sleep(1000000)"_SQL, 2s, yield[ec]);
        EXPECT_EQ(ec, boost::asio::error::timed_out);
    }, boost::asio::detached);

    io.run();

    // The statement is still running on the server: the client gave up on it
    // and the cancellation deliberately never arrived.
    terminate_backend(backend_pid);
}

} // namespace
