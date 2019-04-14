#include <boost/asio.hpp>
#include <future>
#include <vector>

#include "EmptyPort.hpp"
#include "TestServer.hpp"
#include "catch.hpp"

#include "bredis/Connection.hpp"
#include "bredis/Extract.hpp"
#include "bredis/MarkerHelpers.hpp"

#include "SocketWithLogging.hpp"

namespace r = bredis;
namespace asio = boost::asio;
namespace ep = empty_port;
namespace ts = test_server;

TEST_CASE("subscription", "[connection]") {
    using socket_t = asio::ip::tcp::socket;
#ifdef BREDIS_DEBUG
    using next_layer_t = r::test::SocketWithLogging<socket_t &>;
#else
    using next_layer_t = socket_t &;
#endif
    using Buffer = boost::asio::streambuf;
    using Iterator =
        boost::asio::buffers_iterator<typename Buffer::const_buffers_type,
                                      char>;
    using Policy = r::parsing_policy::keep_result;
    using ParseResult = r::positive_parse_result_t<Iterator, Policy>;
    using Extractor = r::extractor<Iterator>;

    using read_callback_t = std::function<void(
        const boost::system::error_code &error_code, ParseResult &&r)>;

    std::chrono::milliseconds sleep_delay(1);
    uint16_t port = ep::get_random<ep::Kind::TCP>();
    auto port_str = boost::lexical_cast<std::string>(port);
    auto server = ts::make_server({"redis-server", "--port", port_str});
    ep::wait_port<ep::Kind::TCP>(port);
    // uint16_t port = 6379;
    asio::io_service io_service;

    asio::ip::tcp::endpoint end_point(
        asio::ip::address::from_string("127.0.0.1"), port);
    socket_t socket(io_service, end_point.protocol());
    socket.connect(end_point);

    std::promise<void> subscription_promise;
    std::future<void> subscription_future = subscription_promise.get_future();
    std::promise<void> completion_promise;
    std::future<void> completion_future = completion_promise.get_future();

    r::Connection<next_layer_t> consumer(socket);
    r::single_command_t subscribe_cmd{"subscribe", "some-channel1",
                                      "some-channel2"};

    /* check point 1, got 2 subscription confirmations */
    {
        Buffer rx_buff;
        consumer.write(subscribe_cmd);
        auto parse_result = consumer.read(rx_buff);

        r::marker_helpers::check_subscription<Iterator> check_subscription{
            std::move(subscribe_cmd)};
        REQUIRE(boost::apply_visitor(check_subscription, parse_result.result));
        rx_buff.consume(parse_result.consumed);

        parse_result = consumer.read(rx_buff);
        REQUIRE(boost::apply_visitor(check_subscription, parse_result.result));
        rx_buff.consume(parse_result.consumed);
    }

    /* check point 2: publish messages */
    {
        socket_t socket_2(io_service, end_point.protocol());
        socket_2.connect(end_point);
        r::Connection<socket_t> producer(std::move(socket_2));
        Buffer rx_buff;

        producer.write(
            r::single_command_t("publish", "some-channel1", "message-a1"));
        auto s_result = producer.read(rx_buff);
        REQUIRE(boost::apply_visitor(r::marker_helpers::equality<Iterator>("1"),
                                     s_result.result));
        rx_buff.consume(s_result.consumed);

        producer.write(
            r::single_command_t("publish", "some-channel1", "message-a2"));
        s_result = producer.read(rx_buff);
        REQUIRE(boost::apply_visitor(r::marker_helpers::equality<Iterator>("1"),
                                     s_result.result));
        rx_buff.consume(s_result.consumed);

        producer.write(
            r::single_command_t("publish", "some-channel3", "message-c"));
        s_result = producer.read(rx_buff);
        REQUIRE(boost::apply_visitor(r::marker_helpers::equality<Iterator>("0"),
                                     s_result.result));
        rx_buff.consume(s_result.consumed);

        producer.write(r::single_command_t("publish", "some-channel2", "last"));
        s_result = producer.read(rx_buff);
        REQUIRE(boost::apply_visitor(r::marker_helpers::equality<Iterator>("1"),
                                     s_result.result));
        rx_buff.consume(s_result.consumed);
    }

    /* check point 3: examine received messages */
    std::vector<std::string> messages;
    Buffer rx_buff;
    r::Connection<next_layer_t> c(socket);

    read_callback_t notification_callback =
        [&](const boost::system::error_code ec, ParseResult &&r) {
            REQUIRE(!ec);
#ifdef BREDIS_DEBUG
            BREDIS_LOG_DEBUG(
                "subscription callback " << boost::apply_visitor(
                    r::marker_helpers::stringizer<Iterator>(), r.result));
#endif
            REQUIRE(!ec);
            auto extract = boost::apply_visitor(Extractor(), r.result);
            r::extracts::array_holder_t array_reply =
                boost::get<r::extracts::array_holder_t>(extract);
            auto *type_reply =
                boost::get<r::extracts::string_t>(&array_reply.elements[0]);
            auto *string_reply =
                boost::get<r::extracts::string_t>(&array_reply.elements[2]);
            BREDIS_LOG_DEBUG("examining for completion. String: "
                             << (string_reply ? string_reply->str : ""));

            bool retrigger = false;
            if (type_reply && type_reply->str == "message" && string_reply) {
                if (string_reply->str == "last") {
                    completion_promise.set_value();
                } else {
                    BREDIS_LOG_DEBUG("retriggering notification_callback");
                    retrigger = true;
                }
                auto *channel =
                    boost::get<r::extracts::string_t>(&array_reply.elements[1]);
                REQUIRE(channel);
                std::string payload(string_reply->str);
                messages.emplace_back(channel->str + ":" + payload);
            }
            BREDIS_LOG_DEBUG("consuming " << r.consumed << " bytes");
            REQUIRE(r.consumed);
            rx_buff.consume(r.consumed);
            if (retrigger) {
                c.async_read(rx_buff, notification_callback);
            }
        };
    c.async_read(rx_buff, notification_callback);

    /* gather enough info */
    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }

    REQUIRE(messages.size() == 3);
    REQUIRE(messages[0] == "some-channel1:message-a1");
    REQUIRE(messages[1] == "some-channel1:message-a2");
    REQUIRE(messages[2] == "some-channel2:last");
}
