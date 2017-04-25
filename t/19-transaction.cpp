#define CATCH_CONFIG_MAIN

#include <boost/asio.hpp>
#include <future>

#include "EmptyPort.hpp"
#include "TestServer.hpp"
#include "catch.hpp"

#include "bredis/AsyncConnection.hpp"

namespace r = bredis;
namespace asio = boost::asio;
namespace ep = empty_port;
namespace ts = test_server;

TEST_CASE("transaction", "[connection]") {
    using socket_t = asio::ip::tcp::socket;
    using result_t = void;
    using read_callback_t =
        std::function<void(const boost::system::error_code &error_code,
                           r::redis_result_t &&r, size_t consumed)>;

    std::chrono::milliseconds sleep_delay(1);

    uint16_t port = ep::get_random<ep::Kind::TCP>();
    auto port_str = boost::lexical_cast<std::string>(port);
    auto server = ts::make_server({"redis-server", "--port", port_str});
    ep::wait_port<ep::Kind::TCP>(port);
    asio::io_service io_service;

    r::command_container_t tx_commands = {
        r::single_command_t("MULTI"), r::single_command_t("INCR", "foo"),
        r::single_command_t("INCR", "bar"), r::single_command_t("EXEC"),
    };
    r::command_wrapper_t cmd(tx_commands);

    std::promise<result_t> completion_promise;
    std::future<result_t> completion_future = completion_promise.get_future();

    asio::ip::tcp::endpoint end_point(
        asio::ip::address::from_string("127.0.0.1"), port);
    socket_t socket(io_service, end_point.protocol());
    socket.connect(end_point);
    r::AsyncConnection<socket_t> c(std::move(socket));

    boost::asio::streambuf rx_buff;
    read_callback_t read_callback = [&](
        const boost::system::error_code &error_code, r::redis_result_t &&r,
        size_t consumed) {
        REQUIRE(!error_code);
        auto &replies = boost::get<r::array_holder_t>(r);
        REQUIRE(replies.elements.size() == tx_commands.size());

        REQUIRE(boost::get<r::string_holder_t>(&replies.elements[0])->str ==
                "OK");
        REQUIRE(boost::get<r::string_holder_t>(&replies.elements[1])->str ==
                "QUEUED");
        REQUIRE(boost::get<r::string_holder_t>(&replies.elements[2])->str ==
                "QUEUED");

        auto &tx_replies = boost::get<r::array_holder_t>(replies.elements[3]);
        REQUIRE(tx_replies.elements.size() == 2);
        REQUIRE(boost::get<r::int_result_t>(tx_replies.elements[0]) == 1);
        REQUIRE(boost::get<r::int_result_t>(tx_replies.elements[1]) == 1);

        completion_promise.set_value();
        rx_buff.consume(consumed);
    };

    c.async_read(rx_buff, read_callback, tx_commands.size());

    c.async_write(cmd, [&](const auto &error_code) { REQUIRE(!error_code); });

    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }
};
