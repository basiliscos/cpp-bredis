//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//
#pragma once

#include <initializer_list>
#include <boost/asio.hpp>
#include <boost/utility/string_ref.hpp>

#include "Result.hpp"
#include "Protocol.hpp"
#include "Error.hpp"

namespace bredis {

template <typename S> class SyncConnection {
    using protocol_type_t = typename S::protocol_type;
    using string_t = boost::string_ref;

    static_assert(std::is_same<protocol_type_t, boost::asio::ip::tcp>::value ||
                      std::is_same<protocol_type_t,
                                   boost::asio::local::stream_protocol>::value,
                  "SyncConnection can be specialized either for ip::tcp or "
                  "local::stream_protocol");

  private:
    S socket_;

  public:
    SyncConnection(S &&socket) : socket_(std::move(socket)) {}

    template <typename C = std::initializer_list<string_t>>
    redis_result_t command(const std::string &cmd, C &&container,
                           boost::asio::streambuf &rx_buff);

    redis_result_t inline command(const std::string &cmd,
                                  boost::asio::streambuf &rx_buff) {
        return command(cmd, std::initializer_list<string_t>{}, rx_buff);
    }
};

} // namespace bredis

#include "impl/sync_connection.hpp"
