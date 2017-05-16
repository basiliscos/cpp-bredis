#include <boost/asio.hpp>
#include <boost/asio/use_future.hpp>
//#include <boost/asio/spawn.hpp>
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
    using result_t = r::markers::redis_result_t<Iterator>;

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

    /*
    boost::asio::spawn(io_service, [&] (boost::asio::yield_context yield) mutable {
        boost::system::error_code error_code;
        auto future_write = c.async_write("ping", yield[error_code]);
        REQUIRE(!error_code);

    });

    io_service.run();
    */
    auto fut = c.async_write("ping", asio::use_future);

/*
    std::promise<result_t> completion_promise;
    std::future<result_t> completion_future = completion_promise.get_future();

    Buffer rx_buff;

    c.async_write("ping", [&](const auto &error_code) {
        c.async_read(rx_buff,
                     [&](const auto &error_code, auto &&r, size_t consumed) {
                         completion_promise.set_value(r);
                     });
    });

    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }

    auto result = completion_future.get();
    auto extract = boost::apply_visitor(r::extractor<Iterator>(), result);
    auto &reply_str = boost::get<r::extracts::string_t>(extract);
    REQUIRE(reply_str.str == "PONG");
*/
};
