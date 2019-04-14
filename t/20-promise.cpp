#include <boost/asio.hpp>
#include <boost/asio/use_future.hpp>
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

TEST_CASE("ping", "[connection]") {
    using socket_t = asio::ip::tcp::socket;

#ifdef BREDIS_DEBUG
    using next_layer_t = r::test::SocketWithLogging<socket_t>;
#else
    using next_layer_t = socket_t;
#endif
    using Buffer = boost::asio::streambuf;
    using Iterator = typename r::to_iterator<Buffer>::iterator_t;

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
    Buffer rx_buff, tx_buff;

    auto f_tx_consumed = c.async_write(tx_buff, "ping", asio::use_future);
    auto f_parse_result = c.async_read(rx_buff, asio::use_future, 1);

    std::thread thread([&io_service]() { io_service.run(); });

    REQUIRE(f_tx_consumed.get() > 0);

    auto parse_result = f_parse_result.get();
    auto extract =
        boost::apply_visitor(r::extractor<Iterator>(), parse_result.result);
    auto &reply_str = boost::get<r::extracts::string_t>(extract);
    REQUIRE(reply_str.str == "PONG");

    io_service.stop();
    thread.join();
}
