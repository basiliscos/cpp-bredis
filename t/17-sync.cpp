#include <boost/asio.hpp>
#include <future>

#include "EmptyPort.hpp"
#include "TestServer.hpp"
#include "catch.hpp"

#include "bredis/Connection.hpp"
#include "bredis/MarkerHelpers.hpp"

#include "SocketWithLogging.hpp"

namespace r = bredis;
namespace asio = boost::asio;
namespace ep = empty_port;
namespace ts = test_server;

TEST_CASE("ping", "[connection]") {
    using socket_t = asio::ip::tcp::socket;
#ifdef BREDIS_DEBUG
    using next_layer_t = r::test::SocketWithLogging<socket_t>;
#else
    using next_layer_t = socket_t;
#endif
    using Buffer = boost::asio::streambuf;
    using Iterator = typename r::to_iterator<Buffer>::iterator_t;

    std::chrono::milliseconds sleep_delay(1);

    uint16_t port = ep::get_random<ep::Kind::TCP>();
    auto port_str = boost::lexical_cast<std::string>(port);
    auto server = ts::make_server({"redis-server", "--port", port_str});
    ep::wait_port<ep::Kind::TCP>(port);
    asio::io_service io_service;

    asio::ip::tcp::endpoint end_point(
        asio::ip::address::from_string("127.0.0.1"), port);
    socket_t socket(io_service, end_point.protocol());
    socket.connect(end_point);

    r::Connection<next_layer_t> c(std::move(socket));

    Buffer rx_buff;
    auto equality = r::marker_helpers::equality<Iterator>("PONG");
    c.write("ping");
    auto parse_result = c.read(rx_buff);
    REQUIRE(boost::apply_visitor(equality, parse_result.result));
    rx_buff.consume(parse_result.consumed);

    /* overloads */
    boost::system::error_code ec;
    c.write("ping", ec);
    REQUIRE(!ec);

    parse_result = c.read(rx_buff, ec);
    REQUIRE(!ec);
    REQUIRE(boost::apply_visitor(equality, parse_result.result));
    rx_buff.consume(parse_result.consumed);
}
