#include <boost/asio.hpp>
#include <future>
#include <string>
#include <vector>

#include "EmptyPort.hpp"
#include "SocketWithLogging.hpp"
#include "TestServer.hpp"
#include "catch.hpp"

#include "bredis/Connection.hpp"

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
    using Iterator =
        boost::asio::buffers_iterator<typename Buffer::const_buffers_type,
                                      char>;
    using Policy = r::parsing_policy::keep_result;
    using ParseResult = r::positive_parse_result_t<Iterator, Policy>;

    using result_t = void;
    using write_callback_t =
        std::function<void(const boost::system::error_code &error_code,
                           std::size_t bytes_transferred)>;
    using read_callback_t = std::function<void(
        const boost::system::error_code &error_code, ParseResult &&r)>;

    std::chrono::nanoseconds sleep_delay(1);

    size_t count = 1000;
    r::single_command_t ping_cmd("ping");
    r::command_container_t ping_cmds_container(count, ping_cmd);
    r::command_wrapper_t cmd(ping_cmds_container);

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
    read_callback_t read_callback = [&](const sys::error_code &error_code,
                                        ParseResult &&r) {
        if (error_code) {
            BREDIS_LOG_DEBUG("error: " << error_code.message());
            REQUIRE(!error_code);
        }
        REQUIRE(!error_code);
        auto &replies =
            boost::get<r::markers::array_holder_t<Iterator>>(r.result);
        BREDIS_LOG_DEBUG("callback, size: " << replies.elements.size());
        REQUIRE(replies.elements.size() == count);
        completion_promise.set_value();
        rx_buff.consume(r.consumed);
    };

    write_callback_t write_callback = [&](const sys::error_code &error_code,
                                          std::size_t bytes_transferred) {
        (void)bytes_transferred;
        BREDIS_LOG_DEBUG("write_callback");
        if (error_code) {
            BREDIS_LOG_DEBUG("error: " << error_code.message());
            REQUIRE(!error_code);
        }
        REQUIRE(!error_code);
        tx_buff.consume(bytes_transferred);
    };

    c.async_read(rx_buff, read_callback, count);
    c.async_write(tx_buff, cmd, write_callback);

    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }
}
