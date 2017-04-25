#define CATCH_CONFIG_MAIN

#include <vector>

#include "bredis/Protocol.hpp"
#include "catch.hpp"

namespace r = bredis;

TEST_CASE("simple string", "[protocol]") {
    std::string ok = "+OK\r\n";
    r::positive_parse_result_t r(
        boost::get<r::positive_parse_result_t>(r::Protocol::parse(ok)));
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == ok.size());
    REQUIRE(boost::get<r::string_holder_t>(r.result) == "OK");
};

TEST_CASE("empty string", "[protocol]") {
    std::string ok = "";
    auto parse_result = r::Protocol::parse(ok);
    r::no_enoght_data_t *r = boost::get<r::no_enoght_data_t>(&parse_result);
    REQUIRE(r != nullptr);
};

TEST_CASE("non-finished ", "[protocol]") {
    std::string ok = "+OK";
    auto parse_result = r::Protocol::parse(ok);
    r::no_enoght_data_t *r = boost::get<r::no_enoght_data_t>(&parse_result);
    REQUIRE(r != nullptr);
};

TEST_CASE("wrong start marker", "[protocol]") {
    std::string ok = "!OK";
    auto parse_result = r::Protocol::parse(ok);
    r::protocol_error_t *r = boost::get<r::protocol_error_t>(&parse_result);
    REQUIRE(r->what == "wrong introduction");
};

TEST_CASE("simple number", "[protocol]") {
    std::string ok = ":55\r\n";
    auto parse_result = r::Protocol::parse(ok);
    r::positive_parse_result_t r(
        boost::get<r::positive_parse_result_t>(parse_result));
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == ok.size());
    REQUIRE(boost::get<r::int_result_t>(r.result) == 55);
};

TEST_CASE("large number", "[protocol]") {
    std::string ok = ":9223372036854775801\r\n";
    auto parse_result = r::Protocol::parse(ok);
    r::positive_parse_result_t r(
        boost::get<r::positive_parse_result_t>(parse_result));
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == ok.size());
    REQUIRE(boost::get<r::int_result_t>(r.result) == 9223372036854775801);
};

TEST_CASE("negative number", "[protocol]") {
    std::string ok = ":-922\r\n";
    auto parse_result = r::Protocol::parse(ok);
    r::positive_parse_result_t r(
        boost::get<r::positive_parse_result_t>(parse_result));
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == ok.size());
    REQUIRE(boost::get<r::int_result_t>(r.result) == -922);
};

TEST_CASE("wrong number", "[protocol]") {
    std::string ok = ":-9ab22\r\n";
    auto parse_result = r::Protocol::parse(ok);
    r::protocol_error_t *r = boost::get<r::protocol_error_t>(&parse_result);
    REQUIRE(r);
    REQUIRE(r->what == "bad lexical cast: source type value could not be "
                       "interpreted as target");
};

TEST_CASE("too large number", "[protocol]") {
    std::string ok = ":92233720368547758019223372036854775801\r\n";
    auto parse_result = r::Protocol::parse(ok);
    r::protocol_error_t *r = boost::get<r::protocol_error_t>(&parse_result);
    REQUIRE(r);
    REQUIRE(r->what == "bad lexical cast: source type value could not be "
                       "interpreted as target");
};

TEST_CASE("simple error", "[protocol]") {
    std::string ok = "-Ooops\r\n";
    auto parse_result = r::Protocol::parse(ok);
    r::positive_parse_result_t r(
        boost::get<r::positive_parse_result_t>(parse_result));
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == ok.size());
    REQUIRE(boost::get<r::error_holder_t>(r.result) == "Ooops");
};

TEST_CASE("nil", "[protocol]") {
    std::string ok = "$-1\r\n";
    auto parse_result = r::Protocol::parse(ok);
    r::positive_parse_result_t r(
        boost::get<r::positive_parse_result_t>(parse_result));
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == ok.size());
    REQUIRE(boost::get<r::nil_t>(&r.result) != nullptr);
};

TEST_CASE("malformed bulk string", "[protocol]") {
    std::string ok = "$-5\r\nsome\r\n";
    auto parse_result = r::Protocol::parse(ok);
    r::protocol_error_t *r = boost::get<r::protocol_error_t>(&parse_result);
    REQUIRE(r);
    REQUIRE(r->what == "Value -5 in unacceptable for bulk strings");
};

TEST_CASE("some bulk string", "[protocol]") {
    std::string ok = "$4\r\nsome\r\n";
    auto parse_result = r::Protocol::parse(ok);
    r::positive_parse_result_t r(
        boost::get<r::positive_parse_result_t>(parse_result));
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == ok.size());
    REQUIRE(boost::get<r::string_holder_t>(r.result) == "some");
};

TEST_CASE("empty bulk string", "[protocol]") {
    std::string ok = "$0\r\n\r\n";
    auto parse_result = r::Protocol::parse(ok);
    r::positive_parse_result_t r(
        boost::get<r::positive_parse_result_t>(parse_result));
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == ok.size());
    REQUIRE(boost::get<r::string_holder_t>(r.result) == "");
};

TEST_CASE("patrial bulk string(1)", "[protocol]") {
    std::string ok = "$10\r\nsome\r\n";
    auto parse_result = r::Protocol::parse(ok);
    REQUIRE(boost::get<r::no_enoght_data_t>(&parse_result) != nullptr);
};

TEST_CASE("patrial bulk string(2)", "[protocol]") {
    std::string ok = "$4\r\nsome\r";
    auto parse_result = r::Protocol::parse(ok);
    REQUIRE(boost::get<r::no_enoght_data_t>(&parse_result) != nullptr);
};

TEST_CASE("empty array", "[protocol]") {
    std::string ok = "*0\r\n";
    auto parse_result = r::Protocol::parse(ok);
    r::positive_parse_result_t r(
        boost::get<r::positive_parse_result_t>(parse_result));
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == ok.size());
    auto &array = boost::get<r::array_holder_t>(r.result);
    REQUIRE(array.elements.size() == 0);
};

TEST_CASE("null array", "[protocol]") {
    std::string ok = "*-1\r\n";
    auto parse_result = r::Protocol::parse(ok);
    r::positive_parse_result_t r(
        boost::get<r::positive_parse_result_t>(parse_result));
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == ok.size());
    REQUIRE(boost::get<r::nil_t>(&r.result) != nullptr);
};

TEST_CASE("malformed array", "[protocol]") {
    std::string ok = "*-4\r\nsome\r\n";
    auto parse_result = r::Protocol::parse(ok);
    r::protocol_error_t *r = boost::get<r::protocol_error_t>(&parse_result);
    REQUIRE(r);
    REQUIRE(r->what == "Value -4 in unacceptable for arrays");
};

TEST_CASE("patrial rray", "[protocol]") {
    std::string ok = "*1\r\n";
    auto parse_result = r::Protocol::parse(ok);
    REQUIRE(boost::get<r::no_enoght_data_t>(&parse_result) != nullptr);
};

TEST_CASE("array: string, int, nil", "[protocol]") {
    std::string ok = "*3\r\n$4\r\nsome\r\n:5\r\n$-1\r\n";
    auto parse_result = r::Protocol::parse(ok);
    r::positive_parse_result_t r(
        boost::get<r::positive_parse_result_t>(parse_result));
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == ok.size());
    auto &array = boost::get<r::array_holder_t>(r.result);
    REQUIRE(array.elements.size() == 3);

    REQUIRE(boost::get<r::string_holder_t>(array.elements[0]) == "some");
    REQUIRE(boost::get<r::int_result_t>(array.elements[1]) == 5);
    REQUIRE(boost::get<r::nil_t>(&array.elements[2]) != nullptr);
};

TEST_CASE("array of arrays: [int, int, int,], [str,err] ", "[protocol]") {
    std::string ok = "*2\r\n*3\r\n:1\r\n:2\r\n:3\r\n*2\r\n+Foo\r\n-Bar\r\n";
    auto parse_result = r::Protocol::parse(ok);
    r::positive_parse_result_t r(
        boost::get<r::positive_parse_result_t>(parse_result));
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == ok.size());

    auto &array = boost::get<r::array_holder_t>(r.result);
    REQUIRE(array.elements.size() == 2);

    auto &a1 = boost::get<r::array_holder_t>(array.elements[0]);
    REQUIRE(a1.elements.size() == 3);
    REQUIRE(boost::get<r::int_result_t>(a1.elements[0]) == 1);
    REQUIRE(boost::get<r::int_result_t>(a1.elements[1]) == 2);
    REQUIRE(boost::get<r::int_result_t>(a1.elements[2]) == 3);

    auto &a2 = boost::get<r::array_holder_t>(array.elements[1]);
    REQUIRE(a2.elements.size() == 2);
    REQUIRE(boost::get<r::string_holder_t>(a2.elements[0]) == "Foo");
    REQUIRE(boost::get<r::error_holder_t>(a2.elements[1]) == "Bar");
};

TEST_CASE("right consumption", "[protocol]") {
    std::string ok =
        "*3\r\n$7\r\nmessage\r\n$13\r\nsome-channel1\r\n$10\r\nmessage-a1\r\n";
    ok = ok + ok;
    boost::string_ref buff(ok);
    auto parse_result = r::Protocol::parse(ok);
    r::positive_parse_result_t r(
        boost::get<r::positive_parse_result_t>(parse_result));
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == ok.size() / 2);
}

TEST_CASE("overfilled buffer", "[protocol]") {
    std::string ok = "*3\r\n$7\r\nmessage\r\n$13\r\nsome-channel1\r\n$"
                     "10\r\nmessage-a1\r\n*3\r\n$7\r\nmessage\r\n$13\r\nsome-"
                     "channel1\r\n$10\r\nmessage-a2\r\n*3\r\n$7\r\nmessage\r\n$"
                     "13\r\nsome-channel2\r\n$4\r\nlast\r\n";
    boost::string_ref buff(ok);
    auto parse_result = r::Protocol::parse(buff);
    r::positive_parse_result_t r(
        boost::get<r::positive_parse_result_t>(parse_result));
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == 54);

    auto &a1 = boost::get<r::array_holder_t>(r.result);
    REQUIRE(a1.elements.size() == 3);
    REQUIRE(boost::get<r::string_holder_t>(a1.elements[0]) == "message");
    REQUIRE(boost::get<r::string_holder_t>(a1.elements[1]) == "some-channel1");
    REQUIRE(boost::get<r::string_holder_t>(a1.elements[2]) == "message-a1");

    buff = boost::string_ref(ok.c_str() + 54, ok.size() - 54);
    parse_result = r::Protocol::parse(buff);
    r = boost::get<r::positive_parse_result_t>(parse_result);
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == 54);

    auto &a2 = boost::get<r::array_holder_t>(r.result);
    REQUIRE(a2.elements.size() == 3);
    REQUIRE(boost::get<r::string_holder_t>(a2.elements[0]) == "message");
    REQUIRE(boost::get<r::string_holder_t>(a2.elements[1]) == "some-channel1");
    REQUIRE(boost::get<r::string_holder_t>(a2.elements[2]) == "message-a2");

    buff = boost::string_ref(ok.c_str() + 54 * 2, ok.size() - 54 * 2);
    parse_result = r::Protocol::parse(buff);
    r = boost::get<r::positive_parse_result_t>(parse_result);
    REQUIRE(r.consumed);
    REQUIRE(r.consumed == 47);

    auto &a3 = boost::get<r::array_holder_t>(r.result);
    REQUIRE(a3.elements.size() == 3);
    REQUIRE(boost::get<r::string_holder_t>(a3.elements[0]) == "message");
    REQUIRE(boost::get<r::string_holder_t>(a3.elements[1]) == "some-channel2");
    REQUIRE(boost::get<r::string_holder_t>(a3.elements[2]) == "last");
}

TEST_CASE("serialize", "[protocol]") {
    std::stringstream buff;
    r::single_command_t cmd("LLEN", "fmm.cheap-travles2");
    r::Protocol::serialize(buff, cmd);
    std::string expected("*2\r\n$4\r\nLLEN\r\n$18\r\nfmm.cheap-travles2\r\n");
    REQUIRE(buff.str() == expected);
};
