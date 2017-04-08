#define CATCH_CONFIG_MAIN

#include <boost/asio.hpp>
#include <future>
#include <boost/lexical_cast.hpp>

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
    using result_t = void;
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

    int order = 0;

    c.push_command("LLEN", {"x"},
                   [&](const auto &error_code, r::redis_result_t &&r) {
                       REQUIRE(boost::get<r::int_result_t>(r) == 0);
                       REQUIRE(order++ == 0);
                   });

    c.push_command("GET", {"x"},
                   [&](const auto &error_code, r::redis_result_t &&r) {
                       REQUIRE(boost::get<r::nil_t>(r) == r::nil_t{});
                       REQUIRE(order++ == 1);
                   });

    c.push_command("SET", {"x", "value"},
                   [&](const auto &error_code, r::redis_result_t &&r) {
                       REQUIRE(boost::get<r::string_holder_t>(r).str == "OK");
                       REQUIRE(order++ == 2);
                   });

    c.push_command(
        "GET", {"x"}, [&](const auto &error_code, r::redis_result_t &&r) {
            REQUIRE(boost::get<r::string_holder_t>(r).str == "value");
            REQUIRE(order++ == 3);
            completion_promise.set_value();
        });

    c.push_command("LLEN", [&](const auto &error_code, r::redis_result_t &&r) {
        REQUIRE(boost::get<r::error_holder_t>(r).str ==
                "ERR wrong number of arguments for 'llen' command");
        REQUIRE(order++ == 4);
    });

    c.push_command("time", [&](const auto &error_code, r::redis_result_t &&r) {
        REQUIRE(order++ == 5);
        auto arr = boost::get<r::array_holder_t>(r);
        REQUIRE(arr.elements.size() == 2);
        REQUIRE(boost::lexical_cast<r::int_result_t>(
                    boost::get<r::string_holder_t>(arr.elements[0]).str) >= 0);
        REQUIRE(boost::lexical_cast<r::int_result_t>(
                    boost::get<r::string_holder_t>(arr.elements[1]).str) >= 0);
    });

    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }
};
