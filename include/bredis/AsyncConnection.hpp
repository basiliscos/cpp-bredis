//
//
// Copyright (c) 2017 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#pragma once

#include <utility>
#include <vector>
#include <tuple>
#include <atomic>
#include <queue>
#include <mutex>
#include <initializer_list>
#include <sstream>

#include <boost/asio.hpp>
#include <boost/utility/string_ref.hpp>

#include "Result.hpp"
#include "Protocol.hpp"
#include "Error.hpp"

namespace bredis {

template <typename S> class AsyncConnection {
    using protocol_type_t = typename S::protocol_type;

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
    S socket_;
    tx_queue_t tx_queue_;
    rx_queue_t rx_queue_;

    std::mutex tx_queue_mutex_;
    std::mutex rx_queue_mutex_;

    std::atomic_int tx_in_progress_;
    std::atomic_int rx_in_progress_;
    boost::asio::streambuf rx_buff_;

  public:
    AsyncConnection(S &&socket)
        : socket_(std::move(socket)), tx_in_progress_(0), rx_in_progress_(0),
          tx_queue_(std::make_unique<tx_queue_t::element_type>()) {}

    template <typename C = std::initializer_list<string_t>>
    void push_command(const std::string &cmd, C &&contaier,
                      command_callback_t callback);

    void inline push_command(const std::string &cmd,
                             command_callback_t callback) {
        push_command(cmd, std::initializer_list<string_t>{}, callback);
    }

    void cancel();

  private:
    void try_write();
    void try_read();
    void write(tx_queue_t::element_type &queue);
    void on_write(const boost::system::error_code &error_code,
                  std::size_t bytes_transferred,
                  std::shared_ptr<callbacks_vector_t> callbacks);

    void read();
    void on_read(const boost::system::error_code &error_code,
                 std::size_t bytes_transferred);
    bool try_parse_rx();
};

} // namespace bredis

#include "impl/async_connection.hpp"
