#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <future>

#include "EmptyPort.hpp"
#include "TestServer.hpp"
#include "catch.hpp"

#include "bredis/Connection.hpp"
#include "bredis/Extract.hpp"

#include "SocketWithLogging.hpp"

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
    using Extractor = r::extractor<Iterator>;

    using result_t = void;
    using read_callback_t = std::function<void(
        const boost::system::error_code &error_code, ParseResult &&r)>;

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
    Buffer rx_buff, tx_buff;

    r::command_container_t cmds_container{
        r::single_command_t("LLEN", "x"),
        r::single_command_t("GET", "x"),
        r::single_command_t("SET", "x", "value"),
        r::single_command_t("GET", "x"),
        r::single_command_t("LLEN"),
        r::single_command_t("time"),
    };
    std::vector<read_callback_t> callbacks{
        [&](const boost::system::error_code &error_code, ParseResult &&r) {
            auto extract = boost::apply_visitor(Extractor(), r.result);
            REQUIRE(boost::get<r::extracts::int_t>(extract) == 0);
            REQUIRE(order == 0);
            REQUIRE(!error_code);
        },
        [&](const boost::system::error_code &error_code, ParseResult &&r) {
            auto extract = boost::apply_visitor(Extractor(), r.result);
            REQUIRE(boost::get<r::extracts::nil_t>(&extract));
            REQUIRE(order == 1);
            REQUIRE(!error_code);
        },
        [&](const boost::system::error_code &error_code, ParseResult &&r) {
            auto extract = boost::apply_visitor(Extractor(), r.result);
            REQUIRE(boost::get<r::extracts::string_t>(extract).str == "OK");
            REQUIRE(order == 2);
            REQUIRE(!error_code);
        },
        [&](const boost::system::error_code &error_code, ParseResult &&r) {
            auto extract = boost::apply_visitor(Extractor(), r.result);
            REQUIRE(boost::get<r::extracts::string_t>(extract).str == "value");
            REQUIRE(order == 3);
            REQUIRE(!error_code);
        },
        [&](const boost::system::error_code &error_code, ParseResult &&r) {
            auto extract = boost::apply_visitor(Extractor(), r.result);
            REQUIRE(boost::get<r::extracts::error_t>(extract).str ==
                    "ERR wrong number of arguments for 'llen' command");
            REQUIRE(order == 4);
            REQUIRE(!error_code);
        },
        [&](const boost::system::error_code &error_code, ParseResult &&r) {
            REQUIRE(!error_code);
            REQUIRE(order == 5);
            auto extract = boost::apply_visitor(Extractor(), r.result);
            auto &arr = boost::get<r::extracts::array_holder_t>(extract);
            REQUIRE(arr.elements.size() == 2);
            auto &c1 = boost::get<r::extracts::string_t>(arr.elements[0]).str;
            auto &c2 = boost::get<r::extracts::string_t>(arr.elements[1]).str;

            REQUIRE(boost::lexical_cast<r::extracts::int_t>(c1) >= 0);
            REQUIRE(boost::lexical_cast<r::extracts::int_t>(c2) >= 0);
            completion_promise.set_value();
        }};

    read_callback_t generic_callback =
        [&](const boost::system::error_code &error_code, ParseResult &&r) {
            REQUIRE(!error_code);
            auto &cb = callbacks[order];
            auto consumed = r.consumed;
            cb(error_code, std::move(r));
            rx_buff.consume(consumed);
            ++order;
            c.async_read(rx_buff, generic_callback);
        };

    c.async_write(
        tx_buff, r::command_wrapper_t(cmds_container),
        [&](const sys::error_code &ec, std::size_t bytes_transferred) {
            REQUIRE(!ec);
            tx_buff.consume(bytes_transferred);
            c.async_read(rx_buff, generic_callback);
        });

    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }
}
