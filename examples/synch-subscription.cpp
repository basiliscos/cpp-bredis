//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <bredis/Connection.hpp>
#include <bredis/Extract.hpp>
#include <bredis/MarkerHelpers.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "picojson.h"

namespace r = bredis;
namespace asio = boost::asio;

template <typename Iterator>
struct eq_array : public boost::static_visitor<bool> {
    std::vector<std::string> values_;

    template <typename... Args>
    eq_array(Args &&... args) : values_{std::forward<Args>(args)...} {}

    template <typename T> bool operator()(const T &value) const {
        return false;
    }

    bool operator()(const r::markers::array_holder_t<Iterator> &value) const {
        bool r = values_.size() <= value.elements.size();
        if (r) {
            for (std::size_t i = 0; i < values_.size() && r; i++) {
                const auto &exemplar = values_[i];
                const auto &item = value.elements[i];
                r = r &&
                    boost::apply_visitor(
                        r::marker_helpers::equality<Iterator>(exemplar), item);
            }
        }
        return r;
    }
};

using optional_string_t = boost::optional<std::string>;

struct payload_extractor : public boost::static_visitor<optional_string_t> {
    template <typename T> optional_string_t operator()(const T &value) const {
        return optional_string_t{};
    }

    optional_string_t
    operator()(const r::extracts::array_holder_t value) const {
        if (value.elements.size() == 3) {
            const auto &payload = value.elements[2];
            const auto *message = boost::get<r::extracts::string_t>(&payload);
            if (message) {
                return optional_string_t{message->str};
            }
        }
        return optional_string_t{};
    }
};

int main(int argc, char **argv) {
    using socket_t = asio::ip::tcp::socket;
    using next_layer_t = socket_t;
    using Buffer = boost::asio::streambuf;
    using Iterator = typename r::to_iterator<Buffer>::iterator_t;

    asio::io_service io_service;
    asio::ip::tcp::endpoint end_point(
        asio::ip::address::from_string("127.0.0.1"), 6379);
    socket_t socket(io_service, end_point.protocol());
    socket.connect(end_point);

    r::Connection<next_layer_t> c(std::move(socket));
    boost::system::error_code error_code;

    // subscribe
    try {
        r::single_command_t subscribe_cmd{"SUBSCRIBE", "channel-one"};
        c.write(subscribe_cmd);
        std::cout << "subscribed\n";

        Buffer rx_buff;
        auto parse_result = c.read(rx_buff);
        eq_array<Iterator> confirm_message{
            "subscribe", "channel-one",
        };
        bool subscription_confirmed =
            boost::apply_visitor(confirm_message, parse_result.result);
        if (!subscription_confirmed) {
            std::cout << "subscription was not confirmed\n";
            return 1;
        }
        std::cout << "subscription confirmed\n";
        rx_buff.consume(parse_result.consumed);

        while (true) {
            parse_result = c.read(rx_buff);
            auto extract = boost::apply_visitor(r::extractor<Iterator>(),
                                                parse_result.result);
            rx_buff.consume(parse_result.consumed);
            auto payload = boost::apply_visitor(payload_extractor(), extract);
            if (payload) {
                std::cout << "received payload:: '" << *payload << "'\n";
            } else {
                std::cout << "received something else\n";
            }
        };
    } catch (const boost::system::system_error &e) {
        std::cout << "error :: " << e.what() << "\n";
        return 1;
    }

    std::cout << "normal exit\n";
    return 0;
}
