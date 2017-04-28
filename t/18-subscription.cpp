#define CATCH_CONFIG_MAIN

#include <boost/asio.hpp>
#include <future>
#include <vector>

#include "EmptyPort.hpp"
#include "TestServer.hpp"
#include "catch.hpp"
#include "SocketWithLogging.hpp"

#include "bredis/Connection.hpp"

namespace r = bredis;
namespace asio = boost::asio;
namespace ep = empty_port;
namespace ts = test_server;

TEST_CASE("subscription", "[connection]") {
    using socket_t = asio::ip::tcp::socket;
#ifdef BREDIS_DEBUG
    using next_layer_t = r::test::SocketWithLogging<socket_t>;
#else
    using next_layer_t = socket_t;
#endif
    using result_t = r::redis_result_t;

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

    std::vector<r::redis_result_t> results;
    std::vector<std::string> messages;
    std::promise<void> subscription_promise;
    std::future<void> subscription_future = subscription_promise.get_future();
    std::promise<void> completion_promise;
    std::future<void> completion_future = completion_promise.get_future();

    r::Connection<socket_t&> consumer(socket);
    r::command_wrapper_t subscribe_cmd(
        r::single_command_t("subscribe", "some-channel1", "some-channel2"));

    /* check point 1, got 2 subscription confirmations */
    {
        boost::asio::streambuf rx_buff;
        consumer.write(subscribe_cmd);
        auto parse_result = consumer.read(rx_buff);
        auto reply1 = boost::get<r::array_holder_t>(parse_result.result);
        REQUIRE(reply1.elements.size() == 3);
        REQUIRE(boost::get<r::string_holder_t>(reply1.elements[0]) ==
                "subscribe");
        REQUIRE(boost::get<r::string_holder_t>(reply1.elements[1]) ==
                "some-channel1");
        REQUIRE(boost::get<r::int_result_t>(reply1.elements[2]) == 1);
        rx_buff.consume(parse_result.consumed);

        parse_result = consumer.read(rx_buff);
        auto reply2 = boost::get<r::array_holder_t>(parse_result.result);
        REQUIRE(reply2.elements.size() == 3);
        REQUIRE(boost::get<r::string_holder_t>(reply2.elements[0]) ==
                "subscribe");
        REQUIRE(boost::get<r::string_holder_t>(reply2.elements[1]) ==
                "some-channel2");
        REQUIRE(boost::get<r::int_result_t>(reply2.elements[2]) == 2);
        rx_buff.consume(parse_result.consumed);
    }

    /* check point 2: publish messages */
    {
        socket_t socket_2(io_service, end_point.protocol());
        socket_2.connect(end_point);
        r::Connection<socket_t> producer(std::move(socket_2));
        boost::asio::streambuf rx_buff;

        producer.write(r::single_command_t("publish", "some-channel1", "message-a1"));
        auto s_result = producer.read(rx_buff);
        REQUIRE(boost::get<r::int_result_t>(s_result.result) == 1);
        rx_buff.consume(s_result.consumed);

        producer.write(r::single_command_t("publish", "some-channel1", "message-a2"));
        s_result = producer.read(rx_buff);
        REQUIRE(boost::get<r::int_result_t>(s_result.result) == 1);
        rx_buff.consume(s_result.consumed);

        producer.write(r::single_command_t("publish", "some-channel3", "message-c"));
        s_result = producer.read(rx_buff);
        REQUIRE(boost::get<r::int_result_t>(s_result.result) == 0);
        rx_buff.consume(s_result.consumed);

        producer.write(r::single_command_t("publish", "some-channel2", "last"));
        s_result = producer.read(rx_buff);
        REQUIRE(boost::get<r::int_result_t>(s_result.result) == 1);
        rx_buff.consume(s_result.consumed);
    }

    /* check point 3: examine received messages */
    boost::asio::streambuf rx_buff;
    r::Connection<socket_t&> c(socket);

    read_callback_t notification_callback = [&](const boost::system::error_code,
                                     r::redis_result_t &&r, size_t consumed) {
#ifdef BREDIS_DEBUG
        BREDIS_LOG_DEBUG("subscription callback " << boost::apply_visitor(r::result_stringizer(), r));
#endif

        r::array_holder_t array_reply = boost::get<r::array_holder_t>(r);

        r::string_holder_t *type_reply =
            boost::get<r::string_holder_t>(&array_reply.elements[0]);
        r::string_holder_t *string_reply =
            boost::get<r::string_holder_t>(&array_reply.elements[2]);
        BREDIS_LOG_DEBUG("examining for completion. String: "
                         << (string_reply ? string_reply->str : ""));

        if (type_reply && type_reply->str == "message" && string_reply) {
            if (string_reply->str == "last") {
                completion_promise.set_value();
            } else {
                c.async_read(rx_buff, notification_callback);
            }
            std::string channel(
                boost::get<r::string_holder_t>(&array_reply.elements[1])->str);
            std::string payload(string_reply->str);
            messages.emplace_back(channel + ":" + payload);
        }
        rx_buff.consume(consumed);
    };
    c.async_read(rx_buff, notification_callback);

    /* gather enough info */
    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }

};
