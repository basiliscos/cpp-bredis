#include <boost/asio.hpp>
#include <future>

#include "EmptyPort.hpp"
#include "TestServer.hpp"

#include "SocketWithLogging.hpp"
#include "bredis/Connection.hpp"
#include "bredis/Extract.hpp"

#include "catch.hpp"

namespace r = bredis;
namespace asio = boost::asio;
namespace ep = empty_port;
namespace ts = test_server;
namespace sys = boost::system;

TEST_CASE("ping", "[connection]") {
    using socket_t = asio::ip::tcp::socket;

#ifdef BREDIS_DEBUG
    using next_layer_t = r::test::SocketWithLogging<socket_t>;
#else
    using next_layer_t = socket_t;
#endif
    using Buffer = boost::asio::streambuf;
    using Iterator = typename r::to_iterator<Buffer>::iterator_t;
    using Policy = r::parsing_policy::keep_result;
    using result_t = r::positive_parse_result_t<Iterator, Policy>;

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
    std::promise<result_t> completion_promise;
    std::future<result_t> completion_future = completion_promise.get_future();

    Buffer tx_buff, rx_buff;

    c.async_write(
        tx_buff, "ping",
        [&](const sys::error_code &error_code, std::size_t bytes_transferred) {
            REQUIRE(!error_code);
            tx_buff.consume(bytes_transferred);
            c.async_read(rx_buff, [&](const sys::error_code &, result_t &&r) {
                completion_promise.set_value(r);
                rx_buff.consume(r.consumed);
            });
        });

    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }

    auto parse_result = completion_future.get();
    auto extract =
        boost::apply_visitor(r::extractor<Iterator>(), parse_result.result);
    auto &reply_str = boost::get<r::extracts::string_t>(extract);
    REQUIRE(reply_str.str == "PONG");
}
