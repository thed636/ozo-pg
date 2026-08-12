#include <ozo/connection_info.h>
#include <ozo/connection_pool.h>
#include <ozo/execute.h>
#include <ozo/request.h>
#include <ozo/shortcuts.h>
#include <ozo/transaction.h>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// C++20 coroutine support falls out of OZO's use of async_initiate: every
// operation accepts an arbitrary completion token, so asio::use_awaitable works
// without the library knowing about coroutines at all. These tests exist to
// keep that property from regressing, since nothing else in the suite exercises
// a completion token other than a callback or a stackful coroutine.
//
// Note that ASSERT_* cannot be used inside a coroutine: it expands to a bare
// `return`, which is ill-formed in a function returning awaitable. Use EXPECT_*.

namespace {

namespace asio = boost::asio;

using namespace testing;
using namespace ozo::literals;
using namespace std::chrono_literals;

// The completion token to prefer with OZO: it yields (error_code, connection)
// rather than throwing, so a failed operation still hands back the connection
// carrying the diagnostics.
constexpr auto nothrow_awaitable = asio::as_tuple(asio::use_awaitable);

// The coroutines below are free functions taking their arguments by value
// rather than lambdas capturing by reference: a lambda coroutine's closure is
// not kept alive by the coroutine frame, so captures dangle at the first
// suspension point.

// Runs a coroutine to completion on its own io_context and rethrows anything it
// threw, so an unexpected exception fails the test rather than being swallowed.
template <typename MakeCoroutine>
void run_coroutine(MakeCoroutine make) {
    asio::io_context io;
    bool completed = false;
    asio::co_spawn(io, make(std::ref(io), std::ref(completed)),
        [](std::exception_ptr e) {
            if (e) {
                std::rethrow_exception(e);
            }
        });
    io.run();
    // Guards against a coroutine that never actually ran, which would otherwise
    // let a test pass without asserting anything.
    EXPECT_TRUE(completed) << "coroutine did not run to completion";
}

asio::awaitable<void> request_into_rows(std::reference_wrapper<asio::io_context> io,
                                        std::reference_wrapper<bool> completed) {
    const ozo::connection_info conn_info(OZO_PG_TEST_CONNINFO);
    ozo::rows_of<int> result;

    auto [ec, conn] = co_await ozo::request(
        conn_info[io.get()], "SELECT 42"_SQL, 5s, ozo::into(result), nothrow_awaitable);

    EXPECT_FALSE(ec) << ec.message();
    EXPECT_FALSE(ozo::connection_bad(conn));
    EXPECT_THAT(result, ElementsAre(std::make_tuple(42)));
    completed.get() = true;
}

TEST(coroutine, should_perform_request_and_receive_result) {
    run_coroutine(request_into_rows);
}

asio::awaitable<void> reuse_connection(std::reference_wrapper<asio::io_context> io,
                                       std::reference_wrapper<bool> completed) {
    const ozo::connection_info conn_info(OZO_PG_TEST_CONNINFO);
    ozo::rows_of<int> first;
    ozo::rows_of<int> second;

    auto [ec1, conn] = co_await ozo::request(
        conn_info[io.get()], "SELECT 1"_SQL, 5s, ozo::into(first), nothrow_awaitable);
    EXPECT_FALSE(ec1) << ec1.message();

    // The connection handed back by co_await is a ConnectionProvider in its own
    // right, exactly as in the callback and stackful-coroutine styles.
    auto [ec2, same_conn] = co_await ozo::request(
        std::move(conn), "SELECT 2"_SQL, 5s, ozo::into(second), nothrow_awaitable);
    EXPECT_FALSE(ec2) << ec2.message();

    EXPECT_THAT(first, ElementsAre(std::make_tuple(1)));
    EXPECT_THAT(second, ElementsAre(std::make_tuple(2)));
    completed.get() = true;
}

TEST(coroutine, should_reuse_returned_connection_as_provider) {
    run_coroutine(reuse_connection);
}

asio::awaitable<void> preserve_error_context(std::reference_wrapper<asio::io_context> io,
                                             std::reference_wrapper<bool> completed) {
    // A port nothing listens on, so the connection attempt fails.
    const ozo::connection_info conn_info("host=127.0.0.1 port=1 dbname=ozo_no_such_db");
    ozo::rows_of<int> result;

    auto [ec, conn] = co_await ozo::request(
        conn_info[io.get()], "SELECT 1"_SQL, 5s, ozo::into(result), nothrow_awaitable);

    // The point of as_tuple over a bare use_awaitable: the operation failed but
    // the connection survived, so libpq's own diagnostics are still reachable.
    EXPECT_TRUE(ec);
    EXPECT_FALSE(ozo::is_null_recursive(conn));
    EXPECT_THAT(ozo::error_message(conn), Not(IsEmpty()));
    completed.get() = true;
}

TEST(coroutine, should_keep_connection_and_error_message_on_failure) {
    run_coroutine(preserve_error_context);
}

asio::awaitable<void> throw_on_error(std::reference_wrapper<asio::io_context> io,
                                     std::reference_wrapper<bool> completed) {
    const ozo::connection_info conn_info("host=127.0.0.1 port=1 dbname=ozo_no_such_db");
    ozo::rows_of<int> result;
    bool threw = false;

    try {
        // A bare use_awaitable reports failure by throwing.
        co_await ozo::request(conn_info[io.get()], "SELECT 1"_SQL, 5s,
                              ozo::into(result), asio::use_awaitable);
    } catch (const boost::system::system_error& e) {
        threw = true;
        EXPECT_TRUE(e.code());
    }

    EXPECT_TRUE(threw) << "a failed request should throw with a bare use_awaitable";
    completed.get() = true;
}

TEST(coroutine, should_throw_on_error_with_bare_use_awaitable) {
    run_coroutine(throw_on_error);
}

asio::awaitable<void> run_transaction(std::reference_wrapper<asio::io_context> io,
                                      std::reference_wrapper<bool> completed) {
    const ozo::connection_info conn_info(OZO_PG_TEST_CONNINFO);

    auto [begin_ec, transaction] = co_await ozo::begin(conn_info[io.get()], 5s, nothrow_awaitable);
    EXPECT_FALSE(begin_ec) << begin_ec.message();

    ozo::result unused;
    auto [exec_ec, in_transaction] = co_await ozo::request(
        std::move(transaction), "CREATE TEMP TABLE ozo_coroutine_test (value int)"_SQL,
        5s, std::ref(unused), nothrow_awaitable);
    EXPECT_FALSE(exec_ec) << exec_ec.message();

    auto [commit_ec, conn] = co_await ozo::commit(std::move(in_transaction), 5s, nothrow_awaitable);
    EXPECT_FALSE(commit_ec) << commit_ec.message();
    EXPECT_FALSE(ozo::connection_bad(conn));
    completed.get() = true;
}

TEST(coroutine, should_run_transaction) {
    run_coroutine(run_transaction);
}

asio::awaitable<void> request_via_pool(std::reference_wrapper<asio::io_context> io,
                                       std::reference_wrapper<bool> completed) {
    ozo::connection_pool_config config;
    config.capacity = 2;
    config.queue_capacity = 4;

    auto pool = ozo::make_connection_pool(ozo::connection_info(OZO_PG_TEST_CONNINFO), config);
    ozo::rows_of<int> result;

    auto [ec, conn] = co_await ozo::request(
        pool[io.get()], "SELECT 7"_SQL, 5s, ozo::into(result), nothrow_awaitable);

    EXPECT_FALSE(ec) << ec.message();
    EXPECT_THAT(result, ElementsAre(std::make_tuple(7)));
    completed.get() = true;
}

TEST(coroutine, should_request_through_connection_pool) {
    run_coroutine(request_via_pool);
}

asio::awaitable<void> honour_deadline(std::reference_wrapper<asio::io_context> io,
                                      std::reference_wrapper<bool> completed) {
    const ozo::connection_info conn_info(OZO_PG_TEST_CONNINFO);

    // The statement sleeps far longer than the time constraint allows.
    auto [ec, conn] = co_await ozo::execute(
        conn_info[io.get()], "SELECT pg_sleep(10)"_SQL, 1s, nothrow_awaitable);

    EXPECT_EQ(ec, boost::asio::error::timed_out) << ec.message();
    completed.get() = true;
}

TEST(coroutine, should_honour_time_constraint) {
    run_coroutine(honour_deadline);
}

} // namespace
