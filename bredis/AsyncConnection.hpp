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

// Rename `S` to `NextLayer` or `AsyncStream` 
template <typename S> class AsyncConnection {
    using protocol_type_t = typename S::protocol_type;

    // What is the reason for constraining the type of S this way?
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
    // Consider this signature instead:
    //
    // template<class... Args>
    // AsyncConnection(Args&&... args)
    //
    // and use perfect forwarding to initiailze `socket_`
    //
    AsyncConnection(S &&socket)
        : socket_(std::move(socket)), tx_in_progress_(0), rx_in_progress_(0),
          tx_queue_(std::make_unique<tx_queue_t::element_type>()) {}

    // Perhaps add next_layer() and lowest_layer() member
    // functions, so callers can access `socket_` and perform
    // operations
    //
    // Example:
    // https://github.com/vinniefalco/Beast/blob/b8e5a21bfd46d7e912c0e89bd6fcf0901b26ed0a/include/beast/websocket/stream.hpp#L274

    // Consider `boost::string_ref` instead of `std::string`
    template <typename C = std::initializer_list<string_t>>
    void push_command(const std::string &cmd, C &&contaier,
                      command_callback_t callback);

    // How does this callback work? It looks like it functions the
    // same as an Asio completion handler. But command_callback_t
    // is std::function, which performs type erasure. With some
    // refactoring, we can make the callback a template type instead
    // and thus allow asynchronous bredis calls to work with coroutines,
    // std::future, and user defined completion handlers with custom
    // hooks.
    //
    void inline push_command(const std::string &cmd,
                             command_callback_t callback) {
        push_command(cmd, std::initializer_list<string_t>{}, callback);
    }

    // `cancel` is not part of the requirements of `AsyncStream`; by
    // providing this function callers are limited in the types they can
    // provide for `S`. If instead you provide the `next_layer` and
    // `lowest_layer` member functions, callers can use `next_layer().cancel()` or
    // `lowest_layer().cancel()` instead, allowing more choices for
    // the `S` template type here.
    //
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

// This file can't be included by itself,
// consider renaming it to have the .ipp extension
// to make that clear to readers.
//
#include "impl/async_connection.hpp"
