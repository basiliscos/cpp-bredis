//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#pragma once

#include <atomic>
#include <initializer_list>
#include <mutex>
#include <queue>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

#include <boost/asio.hpp>
#include <boost/utility/string_ref.hpp>

#include "Command.hpp"
#include "Error.hpp"
#include "Protocol.hpp"
#include "Result.hpp"
#include "Subscription.hpp"

namespace bredis {

template <typename AsyncStream, typename NotificationCallback>
class Subscription {
    using protocol_type_t = typename AsyncStream::protocol_type;

  public:
    using string_t = boost::string_ref;

  private:
    AsyncStream socket_;
    NotificationCallback callback_;
    boost::asio::streambuf rx_buff_;

    void async_read();

  public:
    Subscription(AsyncStream &&socket, NotificationCallback &&callback)
        : socket_(std::move(socket)), callback_(std::move(callback)) {
        async_read();
    }

    template <typename WriteCallback>
    void async_write(const command_wrapper_t &command,
                     WriteCallback write_callback);

    AsyncStream inline &next_layer() { return socket_; }
};

} // namespace bredis

#include "impl/subscription.ipp"
