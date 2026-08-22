#include <ozo/connection_info.h>
#include <ozo/connection_pool.h>
#include <ozo/request.h>
#include <ozo/shortcuts.h>

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <thread>
#include <vector>

// OZO used to be pinned to boost::asio::io_context::executor_type: the
// connection hardcoded it, and the timer and descriptor factories were
// specialized for it alone. These tests hold the line on the generalisation, in
// particular that a connection can be bound to a strand and driven from several
// threads, which is the case the old design made impossible.

namespace {

namespace asio = boost::asio;

using namespace testing;
using namespace ozo::literals;
using namespace std::chrono_literals;

using strand_type = asio::strand<asio::io_context::executor_type>;

// Runs the io_context on several threads. Every operation below is bound to a
// strand, so this must stay race-free; without a strand the same connection
// touched from four threads would be a data race.
template <typename Coroutine>
void run_on_threads(asio::io_context& io, strand_type strand, Coroutine&& coroutine) {
    asio::spawn(strand, std::forward<Coroutine>(coroutine), asio::detached);

    std::vector<std::thread> threads;
    threads.reserve(4);
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&io] { io.run(); });
    }
    for (auto& thread : threads) {
        thread.join();
    }
}

TEST(executor, connection_info_should_bind_connection_to_a_strand) {
    asio::io_context io;
    auto strand = asio::make_strand(io);
    const ozo::connection_info conn_info(OZO_PG_TEST_CONNINFO);

    ozo::rows_of<std::int32_t> result;
    ozo::error_code ec;
    bool completed = false;

    run_on_threads(io, strand, [&](asio::yield_context yield) {
        const auto conn = ozo::request(conn_info[strand], "SELECT 42"_SQL, 5s,
                                       ozo::into(result), yield[ec]);
        // The connection carries the strand it was bound to, not the bare
        // io_context executor.
        EXPECT_NE(ozo::get_executor(conn).target<strand_type>(), nullptr);
        completed = true;
    });

    EXPECT_FALSE(ec) << ec.message();
    EXPECT_THAT(result, ElementsAre(std::make_tuple(42)));
    EXPECT_TRUE(completed);
}

TEST(executor, connection_pool_should_bind_connection_to_a_strand) {
    asio::io_context io;
    auto strand = asio::make_strand(io);

    ozo::connection_pool_config config;
    config.capacity = 2;
    config.queue_capacity = 4;
    auto pool = ozo::make_connection_pool(ozo::connection_info(OZO_PG_TEST_CONNINFO), config);

    ozo::rows_of<std::int32_t> result;
    ozo::error_code ec;
    bool completed = false;

    run_on_threads(io, strand, [&](asio::yield_context yield) {
        ozo::request(pool[strand], "SELECT 7"_SQL, 5s, ozo::into(result), yield[ec]);
        completed = true;
    });

    EXPECT_FALSE(ec) << ec.message();
    EXPECT_THAT(result, ElementsAre(std::make_tuple(7)));
    EXPECT_TRUE(completed);
}

TEST(executor, should_still_accept_an_io_context) {
    // The io_context overloads remain, so existing code keeps working unchanged.
    asio::io_context io;
    const ozo::connection_info conn_info(OZO_PG_TEST_CONNINFO);

    ozo::rows_of<std::int32_t> result;
    ozo::error_code ec;

    asio::spawn(io, [&](asio::yield_context yield) {
        ozo::request(conn_info[io], "SELECT 1"_SQL, 5s, ozo::into(result), yield[ec]);
    }, asio::detached);
    io.run();

    EXPECT_FALSE(ec) << ec.message();
    EXPECT_THAT(result, ElementsAre(std::make_tuple(1)));
}

} // namespace
