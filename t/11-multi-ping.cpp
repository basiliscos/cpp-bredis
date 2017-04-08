#define CATCH_CONFIG_MAIN

#include <boost/asio.hpp>
#include <future>
#include <vector>

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
    std::chrono::nanoseconds sleep_delay(1);

    auto count = 1000;
    uint16_t port = ep::get_random<ep::Kind::TCP>();
    auto port_str = boost::lexical_cast<std::string>(port);
    auto server = ts::make_server({"redis-server", "--port", port_str});
    ep::wait_port<ep::Kind::TCP>(port);
    asio::io_service io_service;

    asio::ip::tcp::endpoint end_point(
        asio::ip::address::from_string("127.0.0.1"), port);
    socket_t socket(io_service, end_point.protocol());
    socket.connect(end_point);

    std::vector<std::string> results;
    r::AsyncConnection<socket_t> c(std::move(socket));
    std::promise<result_t> completion_promise;
    std::future<result_t> completion_future = completion_promise.get_future();

    auto callback = [&](const auto &error_code, r::redis_result_t &&r) {
        if (error_code) {
            io_service.stop();
        }
        auto &reply_ref = boost::get<r::string_holder_t>(r).str;
        std::string reply_str(reply_ref.cbegin(), reply_ref.cend());
        results.emplace_back(reply_str);
        BREDIS_LOG_DEBUG("callback, size: " << results.size());
        if (results.size() == count) {
            completion_promise.set_value();
        }
    };
    for (auto i = 0; i < count; ++i) {
        c.push_command("ping", {}, callback);
    }

    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }
    REQUIRE(results.size() == count);
};
