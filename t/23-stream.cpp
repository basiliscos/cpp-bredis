#include <boost/asio.hpp>
#include <future>

#include "EmptyPort.hpp"
#include "TestServer.hpp"
#include "catch.hpp"

#include "bredis/Connection.hpp"
#include "bredis/MarkerHelpers.hpp"

#include "bredis/Extract.hpp"


#include "SocketWithLogging.hpp"

namespace r = bredis;
namespace asio = boost::asio;
namespace ep = empty_port;
namespace ts = test_server;

TEST_CASE("stream", "[connection]") {
    using socket_t = asio::ip::tcp::socket;
#ifdef BREDIS_DEBUG
    using next_layer_t = r::test::SocketWithLogging<socket_t>;
#else
    using next_layer_t = socket_t;
#endif
    using Buffer = boost::asio::streambuf;
    using Iterator = typename r::to_iterator<Buffer>::iterator_t;
    using Extractor = r::extractor<Iterator>;

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
    c.write(r::single_command_t{ "INFO" });
    auto parse_result0 = c.read(rx_buff);
    auto extract0 = boost::apply_visitor(Extractor(), parse_result0.result);
    auto info = boost::get<r::extracts::string_t>(extract0);
    rx_buff.consume(parse_result0.consumed);
    std::string version_str = "redis_version:5.";
    if (info.str.find("redis_version:5.") == std::string::npos) {
        /* not supported by earlier redis versions */
        return;
    }


    c.write(r::single_command_t{ "XADD", "mystream", "*", "cpu-temp", "23.4", "load", "2.3" });
    auto parse_result1 = c.read(rx_buff);
    auto extract1 = boost::apply_visitor(Extractor(), parse_result1.result);
    auto id1 = boost::get<r::extracts::string_t>(extract1);
    rx_buff.consume(parse_result1.consumed);

    c.write(r::single_command_t{ "XADD", "mystream", "*", "cpu-temp", "23.2", "load", "2.1" });
    auto parse_result2 = c.read(rx_buff);
    auto extract2 = boost::apply_visitor(Extractor(), parse_result2.result);
    auto id2 = boost::get<r::extracts::string_t>(extract2);
    rx_buff.consume(parse_result2.consumed);

    c.write(r::single_command_t{ "XRANGE" , "mystream",  id1.str, id2.str});
    auto parse_result3 = c.read(rx_buff);
    auto extract3 = boost::apply_visitor(Extractor(), parse_result3.result);
    rx_buff.consume(parse_result3.consumed);

    auto& outer_arr = boost::get<r::extracts::array_holder_t>(extract3);
    auto& inner_arr1 = boost::get<r::extracts::array_holder_t>(outer_arr.elements[0]);
    auto& inner_arr2 = boost::get<r::extracts::array_holder_t>(outer_arr.elements[1]);

    REQUIRE(boost::get<r::extracts::string_t>(inner_arr1.elements[0]).str == id1.str);
    auto& arr1 = boost::get<r::extracts::array_holder_t>(inner_arr1.elements[1]);
    REQUIRE(boost::get<r::extracts::string_t>(arr1.elements[0]).str == "cpu-temp");
    REQUIRE(boost::get<r::extracts::string_t>(arr1.elements[1]).str == "23.4");
    REQUIRE(boost::get<r::extracts::string_t>(arr1.elements[2]).str == "load");
    REQUIRE(boost::get<r::extracts::string_t>(arr1.elements[3]).str == "2.3");

    REQUIRE(boost::get<r::extracts::string_t>(inner_arr2.elements[0]).str == id2.str);
    auto& arr2 = boost::get<r::extracts::array_holder_t>(inner_arr2.elements[1]);
    REQUIRE(boost::get<r::extracts::string_t>(arr2.elements[0]).str == "cpu-temp");
    REQUIRE(boost::get<r::extracts::string_t>(arr2.elements[1]).str == "23.2");
    REQUIRE(boost::get<r::extracts::string_t>(arr2.elements[2]).str == "load");
    REQUIRE(boost::get<r::extracts::string_t>(arr2.elements[3]).str == "2.1");
}
