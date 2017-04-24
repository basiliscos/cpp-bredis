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

template <typename AsyncStream> class AsyncConnection {
    using protocol_type_t = typename AsyncStream::protocol_type;

    static_assert(std::is_same<protocol_type_t, boost::asio::ip::tcp>::value ||
                      std::is_same<protocol_type_t,
                                   boost::asio::local::stream_protocol>::value,
                  "AsyncConnection can be specialized either for ip::tcp or "
                  "local::stream_protocol");

  public:
    using string_t = boost::string_ref;
    using args_container_t = std::vector<string_t>;
    using callback_ptr_t = std::shared_ptr<command_callback_t>;
    using item_t = std::tuple<std::string, args_container_t, callback_ptr_t>;
    using tx_queue_t = std::unique_ptr<std::queue<item_t>>;
    using rx_queue_t = std::queue<callback_ptr_t>;
    using callbacks_vector_t = std::vector<callback_ptr_t>;

  private:
    AsyncStream socket_;

  public:
    template <typename... Args>
    AsyncConnection(Args &&... args) : socket_(std::forward<Args>(args)...) {}

    inline AsyncStream &next_layer() { return socket_; }
    inline AsyncStream &&move_layer() { return std::move(socket_); }

    /* asynchronous interface */
    template <typename WriteCallback>
    void async_write(const command_wrapper_t &command,
                     WriteCallback write_callback);

    template <typename ReadCallback, typename Buffer>
    void async_read(Buffer &rx_buff, ReadCallback read_callback);

    /* synchronous interface */
    void write(const command_wrapper_t &command);

    redis_result_t read(boost::asio::streambuf &rx_buff);

    redis_result_t inline execute(const command_wrapper_t &command,
                                  boost::asio::streambuf &rx_buff) {
        write(command);
        return read(rx_buff);
    }
};

} // namespace bredis

#include "impl/async_connection.ipp"
