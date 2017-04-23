#define CATCH_CONFIG_MAIN

#include <boost/asio.hpp>
#include <future>
#include <vector>
#include <cstdio>
#include <iostream>
#include "catch.hpp"
#include "TestServer.hpp"
#include "EmptyPort.hpp"

#include "bredis/AsyncConnection.hpp"

namespace r = bredis;
namespace asio = boost::asio;
namespace ts = test_server;
namespace ep = empty_port;

struct tmpfile_holder_t {
    char *filename_;
    tmpfile_holder_t(char *filename) : filename_(filename) { assert(filename); }
    ~tmpfile_holder_t() { std::remove(filename_); }
};

TEST_CASE("ping", "[connection]") {
    using socket_t = asio::local::stream_protocol::socket;
    using result_t = void;
    using read_callback_t =
        std::function<void(const boost::system::error_code &error_code,
                           r::redis_result_t &&r, size_t consumed)>;


    std::chrono::nanoseconds sleep_delay(1);

    uint16_t port = ep::get_random<ep::Kind::TCP>();
    tmpfile_holder_t redis_config(std::tmpnam(nullptr));
    auto redis_socket = std::tmpnam(nullptr);
    {
        std::ofstream redis_out(redis_config.filename_);
        redis_out << "port " << port << "\n";
        redis_out << "unixsocket " << redis_socket << "\n";
    }
    auto server = ts::make_server({"redis-server", redis_config.filename_});
    ep::wait_port<ep::Kind::TCP>(port);

    auto count = 1000;
    r::single_command_t ping_cmd("ping");
    r::command_container_t ping_cmds_container;
    for (auto i = 0; i < count; ++i) {
        ping_cmds_container.push_back(ping_cmd);
    }
    r::command_wrapper_t cmd(ping_cmds_container);

    asio::io_service io_service;

    asio::local::stream_protocol::endpoint end_point(redis_socket);
    socket_t socket(io_service, end_point.protocol());
    socket.connect(end_point);

    std::vector<std::string> results;
    r::AsyncConnection<socket_t> c(std::move(socket));
    std::promise<result_t> completion_promise;
    std::future<result_t> completion_future = completion_promise.get_future();
    boost::asio::streambuf rx_buff;

    read_callback_t read_callback = [&](const auto &error_code, r::redis_result_t &&r, size_t consumed) {
        REQUIRE(!error_code);
        auto &reply_ref = boost::get<r::string_holder_t>(r).str;
        std::string reply_str(reply_ref.cbegin(), reply_ref.cend());
        results.emplace_back(reply_str);
        BREDIS_LOG_DEBUG("callback, size: " << results.size());
        if (results.size() == count) {
            completion_promise.set_value();
        } else {
            c.async_read(rx_buff, read_callback);
        }
    };

    c.async_write(cmd, [&](const auto &error_code){
        REQUIRE(!error_code);
        c.async_read(rx_buff, read_callback);
    });

    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }
    REQUIRE(results.size() == count);
};
