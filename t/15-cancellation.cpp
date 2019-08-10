#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <future>
#include <vector>

#include "EmptyPort.hpp"
#include "SocketWithLogging.hpp"
#include "TestServer.hpp"
#include "catch.hpp"

#include "bredis/Connection.hpp"

namespace r = bredis;
namespace asio = boost::asio;
namespace sys = boost::system;
namespace ep = empty_port;
namespace ts = test_server;
namespace sys = boost::system;

TEST_CASE("cancel-on-read", "[cancellation]") {
    using socket_t = asio::ip::tcp::socket;
#ifdef BREDIS_DEBUG
    using next_layer_t = r::test::SocketWithLogging<socket_t &>;
#else
    using next_layer_t = socket_t &;
#endif
    using result_t = void;
    using Buffer = boost::asio::streambuf;
    using Iterator =
        boost::asio::buffers_iterator<typename Buffer::const_buffers_type,
                                      char>;
    using Policy = r::parsing_policy::keep_result;
    using ParseResult = r::positive_parse_result_t<Iterator, Policy>;

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

    r::Connection<next_layer_t> c(socket);

    std::string end_marker = "ping\r\n";
    Buffer remote_rx_buff;
    acceptor.async_accept(peer_socket, [&](const sys::error_code &error_code) {
        (void)error_code;
        BREDIS_LOG_DEBUG("async_accept: " << error_code.message() << ", "
                                          << peer_socket.local_endpoint());
        async_read_until(peer_socket, remote_rx_buff, end_marker,
                         [&](const sys::error_code &ec, std::size_t sz) {
                             (void)ec;
                             (void)sz;
                             BREDIS_LOG_DEBUG("async_read: " << sz << ", "
                                                             << ec.message());
                             socket.cancel();
                         });
    });

    std::promise<result_t> completion_promise;
    std::future<result_t> completion_future = completion_promise.get_future();

    Buffer rx_buff, tx_buff;
    c.async_write(
        tx_buff, "ping",
        [&](const sys::error_code &ec, std::size_t bytes_transferred) {
            REQUIRE(!ec);
            tx_buff.consume(bytes_transferred);
            c.async_read(rx_buff,
                         [&](const sys::error_code &ec, ParseResult &&) {
                             REQUIRE(ec);
                             // REQUIRE(error_code.message() == "Operation
                             // canceled");
                             completion_promise.set_value();
                         });
        });

    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }
}
