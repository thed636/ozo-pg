#include <ozo/connection_info.h>
#include <ozo/query_builder.h>
#include <ozo/request.h>
#include <ozo/shortcuts.h>

#include <boost/asio/io_context.hpp>

// Deliberately does not talk to a database: the point of the test package is to
// prove that find_package(ozo) resolves, that ozo::ozo links, and that the
// packaged headers are complete enough to instantiate the templates a caller
// actually uses. Building a query and naming the request types exercises the
// header-only machinery without needing a live server.
int main() {
    using namespace ozo::literals;

    boost::asio::io_context io;
    const ozo::connection_info conn_info("");

    const auto query = "SELECT "_SQL + std::int32_t(1);
    ozo::rows_of<std::int32_t> rows;

    // Not executed -- only instantiated, so the test package stays runnable
    // without a server.
    const auto provider = conn_info[io];
    (void)provider;
    (void)ozo::into(rows);
    (void)ozo::get_query_text(query.build());

    return 0;
}
