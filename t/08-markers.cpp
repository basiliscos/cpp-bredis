#include <boost/asio/buffer.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <string>
#include <vector>

#include "bredis/MarkerHelpers.hpp"
#include "bredis/Protocol.hpp"
#include "catch.hpp"

namespace r = bredis;
namespace asio = boost::asio;

using Iterator = std::string::iterator;

TEST_CASE("subscription mixed case", "[markers]") {
    std::string cmd_subscribe{"subscribe"};
    std::string cmd_psubscribe{"psubscribe"};
    std::string cmd_channel1 = "channel1";
    std::string cmd_channel2 = "channel2";
    std::string cmd_idx_1 = "1";
    std::string cmd_idx_2 = "2";
    std::string cmd_idx_5 = "5";

    r::markers::redis_result_t<Iterator> reply_1{
        r::markers::array_holder_t<Iterator>{
            {r::markers::string_t<Iterator>{cmd_subscribe.begin(),
                                            cmd_subscribe.end()},
             r::markers::string_t<Iterator>{cmd_channel1.begin(),
                                            cmd_channel1.end()},
             r::markers::int_t<Iterator>{
                 {cmd_idx_1.begin(), cmd_idx_1.end()}}}}};

    r::markers::redis_result_t<Iterator> reply_2{
        r::markers::array_holder_t<Iterator>{
            {r::markers::string_t<Iterator>{cmd_subscribe.begin(),
                                            cmd_subscribe.end()},
             r::markers::string_t<Iterator>{cmd_channel2.begin(),
                                            cmd_channel2.end()},
             r::markers::int_t<Iterator>{
                 {cmd_idx_1.begin(), cmd_idx_1.end()}}}}};

    r::markers::redis_result_t<Iterator> reply_3{
        r::markers::array_holder_t<Iterator>{
            {r::markers::string_t<Iterator>{cmd_subscribe.begin(),
                                            cmd_subscribe.end()},
             r::markers::string_t<Iterator>{cmd_channel2.begin(),
                                            cmd_channel2.end()},
             r::markers::int_t<Iterator>{
                 {cmd_idx_2.begin(), cmd_idx_2.end()}}}}};

    r::markers::redis_result_t<Iterator> reply_wrong_1;
    r::markers::redis_result_t<Iterator> reply_wrong_2{
        r::markers::array_holder_t<Iterator>{
            {r::markers::string_t<Iterator>{cmd_psubscribe.begin(),
                                            cmd_psubscribe.end()},
             r::markers::string_t<Iterator>{cmd_channel2.begin(),
                                            cmd_channel2.end()},
             r::markers::int_t<Iterator>{
                 {cmd_idx_1.begin(), cmd_idx_1.end()}}}}};
    r::markers::redis_result_t<Iterator> reply_wrong_3{
        r::markers::array_holder_t<Iterator>{{
            r::markers::int_t<Iterator>{{cmd_idx_1.begin(), cmd_idx_1.end()}},
            r::markers::int_t<Iterator>{{cmd_idx_1.begin(), cmd_idx_1.end()}},
            r::markers::int_t<Iterator>{{cmd_idx_1.begin(), cmd_idx_1.end()}},
        }}};
    r::markers::redis_result_t<Iterator> reply_wrong_4{
        r::markers::array_holder_t<Iterator>{{
            r::markers::int_t<Iterator>{{cmd_idx_1.begin(), cmd_idx_1.end()}},
            r::markers::int_t<Iterator>{{cmd_idx_1.begin(), cmd_idx_1.end()}},
            r::markers::int_t<Iterator>{{cmd_idx_1.begin(), cmd_idx_1.end()}},
            r::markers::int_t<Iterator>{{cmd_idx_1.begin(), cmd_idx_1.end()}},
        }}};
    r::markers::redis_result_t<Iterator> reply_wrong_5{
        r::markers::array_holder_t<Iterator>{{
            r::markers::string_t<Iterator>{cmd_psubscribe.begin(),
                                           cmd_psubscribe.end()},
            r::markers::string_t<Iterator>{cmd_channel2.begin(),
                                           cmd_channel2.end()},
            r::markers::string_t<Iterator>{cmd_psubscribe.begin(),
                                           cmd_psubscribe.end()},
        }}};
    r::markers::redis_result_t<Iterator> reply_wrong_6{
        r::markers::array_holder_t<Iterator>{{
            r::markers::string_t<Iterator>{cmd_psubscribe.begin(),
                                           cmd_psubscribe.end()},
            r::markers::string_t<Iterator>{cmd_channel2.begin(),
                                           cmd_channel2.end()},
            r::markers::int_t<Iterator>{
                {cmd_channel2.begin(), cmd_channel2.end()}},
        }}};
    r::markers::redis_result_t<Iterator> reply_wrong_7{
        r::markers::array_holder_t<Iterator>{
            {r::markers::string_t<Iterator>{cmd_subscribe.begin(),
                                            cmd_subscribe.end()},
             r::markers::string_t<Iterator>{cmd_channel1.begin(),
                                            cmd_channel1.end()},
             r::markers::int_t<Iterator>{
                 {cmd_idx_5.begin(), cmd_idx_5.end()}}}}};

    r::single_command_t subscribe_1{"subscribe", cmd_channel1};
    r::single_command_t subscribe_2{"SUBSCRIBE", cmd_channel2};
    r::single_command_t subscribe_3{"SUBSCRIBE", cmd_channel1, cmd_channel2};

    r::marker_helpers::check_subscription<Iterator> check_subscription_1{
        std::move(subscribe_1)};
    REQUIRE(boost::apply_visitor(check_subscription_1, reply_1));
    REQUIRE(!boost::apply_visitor(check_subscription_1, reply_2));
    REQUIRE(!boost::apply_visitor(check_subscription_1, reply_wrong_1));
    REQUIRE(!boost::apply_visitor(check_subscription_1, reply_wrong_2));
    REQUIRE(!boost::apply_visitor(check_subscription_1, reply_wrong_3));
    REQUIRE(!boost::apply_visitor(check_subscription_1, reply_wrong_4));
    REQUIRE(!boost::apply_visitor(check_subscription_1, reply_wrong_5));
    REQUIRE(!boost::apply_visitor(check_subscription_1, reply_wrong_6));
    REQUIRE(!boost::apply_visitor(check_subscription_1, reply_wrong_7));

    r::marker_helpers::check_subscription<Iterator> check_subscription_2{
        std::move(subscribe_2)};
    REQUIRE(boost::apply_visitor(check_subscription_2, reply_2));
    REQUIRE(!boost::apply_visitor(check_subscription_2, reply_1));

    r::marker_helpers::check_subscription<Iterator> check_subscription_3{
        std::move(subscribe_3)};
    REQUIRE(boost::apply_visitor(check_subscription_3, reply_1));
    REQUIRE(boost::apply_visitor(check_subscription_3, reply_3));
    REQUIRE(!boost::apply_visitor(check_subscription_3, reply_2));
}
