#include "EmptyPort.hpp"
#include "TestServer.hpp"
#include "catch.hpp"

#include <boost/asio.hpp>
#include <cstdio>
#include <fstream>
#include <future>
#include <iostream>
#include <vector>

#include "bredis/Connection.hpp"
#include "bredis/MarkerHelpers.hpp"

#include "SocketWithLogging.hpp"

namespace r = bredis;
namespace asio = boost::asio;
namespace ts = test_server;
namespace ep = empty_port;
namespace sys = boost::system;

struct tmpfile_holder_t {
    char *filename_;
    tmpfile_holder_t(char *filename) : filename_(filename) { assert(filename); }
    ~tmpfile_holder_t() { std::remove(filename_); }
};

TEST_CASE("ping", "[connection]") {
    using socket_t = asio::local::stream_protocol::socket;
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
    using read_callback_t = std::function<void(
        const boost::system::error_code &error_code, ParseResult &&r)>;

    std::chrono::nanoseconds sleep_delay(1);

    uint16_t port = ep::get_random<ep::Kind::TCP>();
    tmpfile_holder_t redis_config(std::tmpnam(nullptr));
    auto redis_socket = std::tmpnam(nullptr);
    {
        std::ofstream redis_out{redis_config.filename_};
        redis_out << "port " << port << "\n";
        redis_out << "unixsocket " << redis_socket << "\n";
    }
    auto server = ts::make_server({"redis-server", redis_config.filename_});
    ep::wait_port<ep::Kind::TCP>(port);

    std::size_t count = 1000;
    r::single_command_t ping_cmd("ping");
    r::command_container_t ping_cmds_container;
    for (size_t i = 0; i < count; ++i) {
        ping_cmds_container.push_back(ping_cmd);
    }
    r::command_wrapper_t cmd(ping_cmds_container);

    asio::io_service io_service;

    asio::local::stream_protocol::endpoint end_point(redis_socket);
    socket_t socket(io_service, end_point.protocol());
    socket.connect(end_point);

    r::Connection<next_layer_t> c(std::move(socket));
    std::promise<result_t> completion_promise;
    std::future<result_t> completion_future = completion_promise.get_future();
    Buffer rx_buff, tx_buff;

    read_callback_t read_callback = [&](const sys::error_code &ec,
                                        ParseResult &&r) {
        REQUIRE(!ec);
        rx_buff.consume(r.consumed);

        auto str = boost::apply_visitor(
            r::marker_helpers::stringizer<Iterator>(), r.result);
        BREDIS_LOG_DEBUG("result: " << str);

        auto equality = r::marker_helpers::equality<Iterator>("PONG");
        auto &results =
            boost::get<r::markers::array_holder_t<Iterator>>(r.result);
        BREDIS_LOG_DEBUG("callback, size: " << results.elements.size());
        REQUIRE(results.elements.size() == count);

        for (const auto &v : results.elements) {
            REQUIRE(boost::apply_visitor(equality, v));
        }
        completion_promise.set_value();
    };

    c.async_write(
        tx_buff, cmd,
        [&](const sys::error_code &ec, std::size_t bytes_transferred) {
            REQUIRE(!ec);
            tx_buff.consume(bytes_transferred);
            c.async_read(rx_buff, read_callback, count);
        });

    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }
}
