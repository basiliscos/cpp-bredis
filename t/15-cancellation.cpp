#define CATCH_CONFIG_MAIN

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <future>
#include <vector>

#include "EmptyPort.hpp"
#include "TestServer.hpp"
#include "catch.hpp"

#include "bredis/AsyncConnection.hpp"

namespace r = bredis;
namespace asio = boost::asio;
namespace sys = boost::system;
namespace ep = empty_port;
namespace ts = test_server;

TEST_CASE("cancel-on-read", "[cancellation]") {
    using socket_t = asio::ip::tcp::socket;
    using result_t = void;
    using read_callback_t =
        std::function<void(const boost::system::error_code &error_code,
                           r::redis_result_t &&r, size_t consumed)>;

    std::chrono::milliseconds sleep_delay(1);

    uint16_t port = ep::get_random<ep::Kind::TCP>();
    asio::io_service io_service;
    asio::ip::tcp::endpoint end_point(
        asio::ip::address::from_string("127.0.0.1"), port);
    asio::ip::tcp::acceptor acceptor(io_service, end_point.protocol());

    acceptor.bind(end_point);
    acceptor.listen(5);

    auto peer_socket = socket_t(io_service);
    socket_t socket(io_service, end_point.protocol());
    socket.connect(end_point);

    r::AsyncConnection<socket_t> c(std::move(socket));

    std::string end_marker = "ping\r\n";
    boost::asio::streambuf remote_rx_buff;
    acceptor.async_accept(peer_socket, [&](const sys::error_code &error_code) {
        BREDIS_LOG_DEBUG("async_accept: " << error_code.message() << ", "
                                          << peer_socket.local_endpoint());
        async_read_until(peer_socket, remote_rx_buff, end_marker,
                         [&](const sys::error_code &ec, std::size_t sz) {
                             BREDIS_LOG_DEBUG("async_read: " << sz << ", "
                                                             << ec.message());
                             c.next_layer().cancel();
                         });
    });

    std::promise<result_t> completion_promise;
    std::future<result_t> completion_future = completion_promise.get_future();

    boost::asio::streambuf rx_buff;
    c.async_write("ping", [&](const auto &error_code) {
        REQUIRE(!error_code);
        c.async_read(rx_buff, [&](const auto &error_code, r::redis_result_t &&r,
                                  size_t consumed) {
            REQUIRE(error_code);
            REQUIRE(error_code.message() == "Operation canceled");
            completion_promise.set_value();
        });
    });

    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }
}
