#define CATCH_CONFIG_MAIN

#include <boost/asio.hpp>
#include <future>

#include "EmptyPort.hpp"
#include "TestServer.hpp"
#include "catch.hpp"

#include "bredis/Connection.hpp"

namespace r = bredis;
namespace asio = boost::asio;
namespace ep = empty_port;
namespace ts = test_server;

TEST_CASE("ping", "[connection]") {
    using socket_t = asio::ip::tcp::socket;
    using result_t = r::redis_result_t;
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

    r::Connection<socket_t> c(std::move(socket));

    boost::asio::streambuf rx_buff;
    c.write("ping");
    auto parse_result = c.read(rx_buff);
    auto &reply_str = boost::get<r::string_holder_t>(parse_result.result).str;
    std::string str(reply_str.cbegin(), reply_str.cend());
    REQUIRE(str == "PONG");

    /* overloads */
    boost::system::error_code ec;
    c.write("ping", ec);
    REQUIRE(!ec);

    parse_result = c.read(rx_buff, ec);
    REQUIRE(!ec);
    reply_str = boost::get<r::string_holder_t>(parse_result.result).str;
    str = std::string(reply_str.cbegin(), reply_str.cend());
    REQUIRE(str == "PONG");


};
