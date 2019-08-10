#include <boost/asio.hpp>
#include <future>

#include "EmptyPort.hpp"
#include "TestServer.hpp"
#include "catch.hpp"

#include "bredis/Connection.hpp"
#include "bredis/MarkerHelpers.hpp"

#include "SocketWithLogging.hpp"

namespace r = bredis;
namespace asio = boost::asio;
namespace ep = empty_port;
namespace ts = test_server;
namespace sys = boost::system;

TEST_CASE("transaction", "[connection]") {
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
    using read_callback_t = std::function<void(
        const boost::system::error_code &error_code, ParseResult &&r)>;

    std::chrono::milliseconds sleep_delay(1);

    uint16_t port = ep::get_random<ep::Kind::TCP>();
    auto port_str = boost::lexical_cast<std::string>(port);
    auto server = ts::make_server({"redis-server", "--port", port_str});
    ep::wait_port<ep::Kind::TCP>(port);
    asio::io_service io_service;

    r::command_container_t tx_commands = {
        r::single_command_t("MULTI"),
        r::single_command_t("INCR", "foo"),
        r::single_command_t("INCR", "bar"),
        r::single_command_t("EXEC"),
    };
    r::command_wrapper_t cmd(tx_commands);

    std::promise<result_t> completion_promise;
    std::future<result_t> completion_future = completion_promise.get_future();

    asio::ip::tcp::endpoint end_point(
        asio::ip::address::from_string("127.0.0.1"), port);
    socket_t socket(io_service, end_point.protocol());
    socket.connect(end_point);
    r::Connection<next_layer_t> c(std::move(socket));

    Buffer rx_buff, tx_buff;
    read_callback_t read_callback = [&](const sys::error_code &ec,
                                        ParseResult &&r) {
        REQUIRE(!ec);

        auto &replies =
            boost::get<r::markers::array_holder_t<Iterator>>(r.result);
        REQUIRE(replies.elements.size() == tx_commands.size());

        auto eq_OK = r::marker_helpers::equality<Iterator>("OK");
        auto eq_QUEUED = r::marker_helpers::equality<Iterator>("QUEUED");
        REQUIRE(replies.elements.size() == 4);
        REQUIRE(boost::apply_visitor(eq_OK, replies.elements[0]));
        REQUIRE(boost::apply_visitor(eq_QUEUED, replies.elements[1]));
        REQUIRE(boost::apply_visitor(eq_QUEUED, replies.elements[2]));

        auto &tx_replies = boost::get<r::markers::array_holder_t<Iterator>>(
            replies.elements[3]);
        REQUIRE(tx_replies.elements.size() == 2);
        REQUIRE(boost::apply_visitor(r::marker_helpers::equality<Iterator>("1"),
                                     tx_replies.elements[0]));
        REQUIRE(boost::apply_visitor(r::marker_helpers::equality<Iterator>("1"),
                                     tx_replies.elements[1]));

        completion_promise.set_value();
        rx_buff.consume(r.consumed);
    };

    c.async_read(rx_buff, read_callback, tx_commands.size());
    c.async_write(
        tx_buff, cmd,
        [&](const sys::error_code &ec, std::size_t bytes_transferred) {
            tx_buff.consume(bytes_transferred);
            REQUIRE(!ec);
        });

    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }
}
