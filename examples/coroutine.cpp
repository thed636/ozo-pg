#include <ozo/connection_info.h>
#include <ozo/request.h>
#include <ozo/shortcuts.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/as_tuple.hpp>

#include <iostream>

namespace asio = boost::asio;

// OZO operations accept any Boost.Asio completion token, so C++20 coroutines
// work with no OZO-specific support: pass asio::use_awaitable and co_await the
// result. Nothing in the library needs to opt in.
//
// Prefer asio::as_tuple(asio::use_awaitable) over a bare asio::use_awaitable.
// A bare use_awaitable reports failure by throwing, and the exception carries
// only an error_code -- but OZO keeps the useful diagnostics on the connection
// object, which a throw discards. as_tuple yields (error_code, connection)
// instead, so the connection survives a failure and can be interrogated. It is
// the coroutine equivalent of yield[ec] rather than a bare yield.
constexpr auto nothrow_awaitable = asio::as_tuple(asio::use_awaitable);

asio::awaitable<void> print_selected_value(std::string conn_string, asio::io_context& io) {
    using namespace ozo::literals;
    using namespace std::chrono_literals;

    // Request result is always a set of rows; the caller owns the output object.
    ozo::rows_of<int> result;

    const ozo::connection_info conn_info(std::move(conn_string));

    // co_await yields the pair the handler would have received. The connection
    // can be reused as a ConnectionProvider for further requests, exactly as in
    // the callback and stackful-coroutine styles.
    auto [ec, connection] = co_await ozo::request(
        conn_info[io], "SELECT 1"_SQL, 1s, ozo::into(result), nothrow_awaitable);

    if (ec) {
        std::cout << "Request failed with error: " << ec.message();
        // Check for a null connection first to avoid undefined behaviour.
        if (!ozo::is_null_recursive(connection)) {
            if (auto msg = ozo::error_message(connection); !msg.empty()) {
                std::cout << ", error message: " << msg;
            }
            // libpq's message is not always enough, so check OZO's own context too.
            if (auto ctx = ozo::get_error_context(connection); !ctx.empty()) {
                std::cout << ", error context: " << ctx;
            }
        }
        std::cout << std::endl;
        co_return;
    }

    std::cout << "Selected:" << std::endl;
    for (auto value : result) {
        std::cout << std::get<0>(value) << std::endl;
    }
}

int main(int argc, char **argv) {
    std::cout << "OZO coroutine example" << std::endl;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <connection string>\n";
        return 1;
    }

    asio::io_context io;

    // The coroutine does not start until io.run() is called, as with any other
    // Boost.Asio operation.
    asio::co_spawn(io, print_selected_value(argv[1], io), asio::detached);

    io.run();

    return 0;
}
