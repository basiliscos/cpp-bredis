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
    using write_callback_t = std::function<void(const boost::system::error_code &error_code)>;
    using read_callback_t = std::function<void(const boost::system::error_code &error_code, r::redis_result_t &&r, size_t consumed)>;

    std::chrono::nanoseconds sleep_delay(1);

    auto count = 100000;
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

    boost::asio::streambuf rx_buff;
    read_callback_t read_callback = [&](const boost::system::error_code &error_code, r::redis_result_t &&r, size_t consumed) {
        if (error_code) {
            BREDIS_LOG_DEBUG("error: " << error_code.message());
            REQUIRE(!error_code);
        }
        //REQUIRE(!error_code);
        auto &reply_ref = boost::get<r::string_holder_t>(r).str;
        std::string reply_str(reply_ref.cbegin(), reply_ref.cend());
        results.emplace_back(reply_str);
        BREDIS_LOG_DEBUG("callback, size: " << results.size());
        if (results.size() == count) {
            completion_promise.set_value();
        }
        rx_buff.consume(consumed);
        c.async_read(rx_buff, read_callback);
    };

    auto writes_count = 0;
    write_callback_t write_callback = [&](const boost::system::error_code &error_code){
        //REQUIRE(!error_code);
        c.async_write("ping", write_callback);
        if (++writes_count == 1) {
            BREDIS_LOG_DEBUG("pusing initial read callback");
            c.async_read(rx_buff, read_callback);
        }
    };

    c.async_write("ping", write_callback);

    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }
    REQUIRE(results.size() == count);
};
