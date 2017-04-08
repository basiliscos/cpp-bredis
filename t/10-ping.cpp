#define CATCH_CONFIG_MAIN

#include <boost/asio.hpp>
#include <future>

#include "catch.hpp"
#include "EmptyPort.hpp"
#include "TestServer.hpp"

#include "bredis/AsyncConnection.hpp"

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

    r::AsyncConnection<socket_t> c(std::move(socket));
    std::promise<result_t> completion_promise;
    std::future<result_t> completion_future = completion_promise.get_future();

    c.push_command("ping", [&completion_promise](const auto &error_code,
                                                 r::redis_result_t &&r) {
        completion_promise.set_value(r);
    });

    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }
    auto &reply_str =
        boost::get<r::string_holder_t>(completion_future.get()).str;
    std::string str(reply_str.cbegin(), reply_str.cend());
    REQUIRE(str == "PONG");
};
