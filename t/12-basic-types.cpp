#define CATCH_CONFIG_MAIN

#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <future>

#include "EmptyPort.hpp"
#include "TestServer.hpp"
#include "catch.hpp"
#include "SocketWithLogging.hpp"

#include "bredis/Connection.hpp"

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

    asio::ip::tcp::endpoint end_point(
        asio::ip::address::from_string("127.0.0.1"), port);
    socket_t socket(io_service, end_point.protocol());
    socket.connect(end_point);

    r::Connection<next_layer_t> c(std::move(socket));
    std::promise<result_t> completion_promise;
    std::future<result_t> completion_future = completion_promise.get_future();

    int order = 0;
    boost::asio::streambuf rx_buff;

    r::command_container_t cmds_container{
        r::single_command_t("LLEN", "x"),
        r::single_command_t("GET", "x"),
        r::single_command_t("SET", "x", "value"),
        r::single_command_t("GET", "x"),
        r::single_command_t("LLEN"),
        r::single_command_t("time"),
    };
    std::vector<read_callback_t> callbacks{
        [&](const boost::system::error_code &error_code, r::redis_result_t &&r,
            size_t consumed) {
            REQUIRE(boost::get<r::int_result_t>(r) == 0);
            REQUIRE(order == 0);
        },
        [&](const boost::system::error_code &error_code, r::redis_result_t &&r,
            size_t consumed) {
            REQUIRE(boost::get<r::nil_t>(r) == r::nil_t{});
            REQUIRE(order == 1);
        },
        [&](const boost::system::error_code &error_code, r::redis_result_t &&r,
            size_t consumed) {
            REQUIRE(boost::get<r::string_holder_t>(r).str == "OK");
            REQUIRE(order == 2);
        },
        [&](const boost::system::error_code &error_code, r::redis_result_t &&r,
            size_t consumed) {
            REQUIRE(boost::get<r::string_holder_t>(r).str == "value");
            REQUIRE(order == 3);
        },
        [&](const boost::system::error_code &error_code, r::redis_result_t &&r,
            size_t consumed) {
            REQUIRE(boost::get<r::error_holder_t>(r).str ==
                    "ERR wrong number of arguments for 'llen' command");
            REQUIRE(order == 4);
        },
        [&](const boost::system::error_code &error_code, r::redis_result_t &&r,
            size_t consumed) {
            REQUIRE(order == 5);
            auto arr = boost::get<r::array_holder_t>(r);
            REQUIRE(arr.elements.size() == 2);
            REQUIRE(boost::lexical_cast<r::int_result_t>(
                        boost::get<r::string_holder_t>(arr.elements[0]).str) >=
                    0);
            REQUIRE(boost::lexical_cast<r::int_result_t>(
                        boost::get<r::string_holder_t>(arr.elements[1]).str) >=
                    0);
            completion_promise.set_value();
        }};

    read_callback_t generic_callback =
        [&](const boost::system::error_code &error_code, r::redis_result_t &&r,
            size_t consumed) {
            REQUIRE(!error_code);
            auto &cb = callbacks[order];
            cb(error_code, std::move(r), consumed);
            rx_buff.consume(consumed);
            ++order;
            c.async_read(rx_buff, generic_callback);
        };

    c.async_write(r::command_wrapper_t(cmds_container),
                  [&](const auto &error_code) {
                      REQUIRE(!error_code);
                      c.async_read(rx_buff, generic_callback);
                  });

    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }
};
