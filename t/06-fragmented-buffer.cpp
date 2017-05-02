#define CATCH_CONFIG_MAIN

#include <boost/asio/buffer.hpp>
#include <vector>

#include "bredis/MarkerHelpers.hpp"
#include "bredis/Protocol.hpp"

#include "catch.hpp"

namespace r = bredis;
namespace asio = boost::asio;

using Buffer = std::vector<asio::const_buffers_1>;
using Iterator = boost::asio::buffers_iterator<Buffer, char>;
using parsed_result_t = r::parse_result_t<Iterator>;

TEST_CASE("right consumption", "[protocol]") {
    std::string full_message =
        "*3\r\n$7\r\nmessage\r\n$13\r\nsome-channel1\r\n$10\r\nmessage-a1\r\n";
    auto from = full_message.cbegin();
    auto to = full_message.cbegin() - 1;
    Buffer buff;
    for (auto i = 0; i < full_message.size(); i++) {
        asio::const_buffers_1 v(full_message.c_str() + i, 1);
        buff.push_back(v);
    }
    auto parsed_result = r::Protocol::parse(buff);
    auto positive_parse_result =
        boost::get<r::positive_parse_result_t<Iterator>>(parsed_result);
    REQUIRE(positive_parse_result.consumed);
    REQUIRE(positive_parse_result.consumed == full_message.size());

    auto *array = boost::get<r::markers::array_holder_t<Iterator>>(
        &positive_parse_result.result);
    REQUIRE(array != nullptr);
    REQUIRE(array->elements.size() == 3);

    REQUIRE(boost::apply_visitor(
        r::marker_helpers::equality<Iterator>("message"), array->elements[0]));
    REQUIRE(boost::apply_visitor(
        r::marker_helpers::equality<Iterator>("some-channel1"),
        array->elements[1]));
    REQUIRE(boost::apply_visitor(
        r::marker_helpers::equality<Iterator>("message-a1"),
        array->elements[2]));
    REQUIRE(boost::get<r::markers::string_t<Iterator>>(&array->elements[0]) !=
            nullptr);
    REQUIRE(boost::get<r::markers::string_t<Iterator>>(&array->elements[1]) !=
            nullptr);
    REQUIRE(boost::get<r::markers::string_t<Iterator>>(&array->elements[2]) !=
            nullptr);
}
