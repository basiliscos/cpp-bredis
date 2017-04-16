#define CATCH_CONFIG_MAIN

#include <boost/asio.hpp>
#include <future>
#include <vector>

#include "EmptyPort.hpp"
#include "TestServer.hpp"
#include "catch.hpp"

#include "bredis/Subscription.hpp"
#include "bredis/SyncConnection.hpp"

namespace r = bredis;
namespace asio = boost::asio;
namespace ep = empty_port;
namespace ts = test_server;

TEST_CASE("subscription", "[connection]") {
    using socket_t = asio::ip::tcp::socket;
    using result_t = r::redis_result_t;
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

    r::Subscription<socket_t> subscription(
        std::move(socket), [&](const auto &error_code, r::redis_result_t &&r) {
            BREDIS_LOG_DEBUG("subscription callback");

            results.emplace_back(std::move(r));
            if (results.size() == 2) {
                subscription_promise.set_value();
            }

            r::array_holder_t *array_reply =
                boost::get<r::array_holder_t>(&results[results.size() - 1]);
            BREDIS_LOG_DEBUG(
                "subscription callback, array: "
                << (array_reply ? array_reply->elements.size() : 0));
            if (array_reply && array_reply->elements.size() == 3) {
                r::string_holder_t *type_reply =
                    boost::get<r::string_holder_t>(&array_reply->elements[0]);
                r::string_holder_t *string_reply =
                    boost::get<r::string_holder_t>(&array_reply->elements[2]);
                BREDIS_LOG_DEBUG("examining for completion. String: "
                                 << (string_reply ? string_reply->str : ""));
                if (type_reply && type_reply->str == "message" &&
                    string_reply) {
                    if (string_reply->str == "last") {
                        completion_promise.set_value();
                    }
                    std::string channel(boost::get<r::string_holder_t>(
                                            &array_reply->elements[1])
                                            ->str);
                    std::string payload(string_reply->str);
                    messages.emplace_back(channel + ":" + payload);
                }
            }
        });

    subscription.push_command("subscribe", {"some-channel1", "some-channel2"});

    while (subscription_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }
    /* check point 1, got 2 subscription confirmations */
    {
        auto &reply1 = boost::get<r::array_holder_t>(results[0]);
        REQUIRE(reply1.elements.size() == 3);
        REQUIRE(boost::get<r::string_holder_t>(reply1.elements[0]) ==
                "subscribe");
        REQUIRE(boost::get<r::string_holder_t>(reply1.elements[1]) ==
                "some-channel1");
        REQUIRE(boost::get<r::int_result_t>(reply1.elements[2]) == 1);

        auto &reply2 = boost::get<r::array_holder_t>(results[1]);
        REQUIRE(reply2.elements.size() == 3);
        REQUIRE(boost::get<r::string_holder_t>(reply2.elements[0]) ==
                "subscribe");
        REQUIRE(boost::get<r::string_holder_t>(reply2.elements[1]) ==
                "some-channel2");
        REQUIRE(boost::get<r::int_result_t>(reply2.elements[2]) == 2);
    }

    /* check point 2: publish messages */
    {
        socket_t socket_2(io_service, end_point.protocol());
        socket_2.connect(end_point);
        r::SyncConnection<socket_t> sync_conn(std::move(socket_2));
        boost::asio::streambuf rx_buff;
        auto s_result = sync_conn.command(
            "publish", {"some-channel1", "message-a1"}, rx_buff);
        REQUIRE(boost::get<r::int_result_t>(s_result) == 1);
        s_result = sync_conn.command("publish", {"some-channel1", "message-a2"},
                                     rx_buff);
        REQUIRE(boost::get<r::int_result_t>(s_result) == 1);
        s_result = sync_conn.command("publish", {"some-channel3", "message-c"},
                                     rx_buff);
        REQUIRE(boost::get<r::int_result_t>(s_result) == 0);
        s_result =
            sync_conn.command("publish", {"some-channel2", "last"}, rx_buff);
        REQUIRE(boost::get<r::int_result_t>(s_result) == 1);
    }

    /* gather enough info */
    while (completion_future.wait_for(sleep_delay) !=
           std::future_status::ready) {
        io_service.run_one();
    }

    /* check point 3, let's see what we have received */
    {
        REQUIRE(results.size() == 5);
        REQUIRE(messages.size() == 3);

        REQUIRE(messages[0] == "some-channel1:message-a1");
        REQUIRE(messages[1] == "some-channel1:message-a2");
        REQUIRE(messages[2] == "some-channel2:last");
    }
};
