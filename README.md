# bredis
Boost::ASIO low-level redis client (connector)

[![Travis](https://img.shields.io/travis/basiliscos/cpp-bredis.svg)](https://travis-ci.org/basiliscos/cpp-bredis)
[![Build status](https://ci.appveyor.com/api/projects/status/a302juc7hrcdhhoc?svg=true)](https://ci.appveyor.com/project/basiliscos/cpp-bredis)
[![license](https://img.shields.io/github/license/basiliscos/cpp-bredis.svg)](https://github.com/basiliscos/cpp-bredis/blob/master/LICENSE)
[![codecov](https://codecov.io/gh/basiliscos/cpp-bredis/badge.svg)](https://codecov.io/gh/basiliscos/cpp-bredis)

## Features

- header only
- low-level controls (i.e. you can cancel, or do you manual DNS-resolving before connection)
- unix domain sockets support
- works on linux (clang, gcc) and windows (msvc)
- synchronous & asynchronous interface
- inspired by [beast](https://github.com/vinniefalco/Beast)

## Changelog


### 0.03
- improved protocol parser (no memory allocations during input stream validity check)

### 0.02
- added windows support
- added coroutines & futures support
- generalised (templated) buffer support
- changed return type: instead of result of parsing just result markers are returned, extraction of result can be done as separate step
- dropped queing support (queuing policy should be implemented at more higher levels)
- dropped subscription support (can be implemented at higher levels)
- dropped internal buffers (can be implemented at higher levels)
- dropped explicit cancellation (socket reference can be passed to connector, and cancellation
can be done on the socket object outside of the connector)

### 0.01
- initial version

## Work with the result

The general idea is that the result of attempt to  redis reply can be either: no enough data or protocol error (exteame case) or some positive parse result. The last one is just **markers** of result, which is actually stored in *receive buffer* (i.e. outside of markers, and outside of bredis-connection).

The the further work with markers denends on your needs: it is possible either **scan** the result for the expected results (e.g. for `PONG` reply on `PING` command, or for `OK`/`QUEUED` replies on `MULTI`/`EXEC` commands) or **extract** the results (the common redis types: `nil`, `string`, `error`, `int` or (recursive) array of them).

When data in receive buffer is no logner required, it should be consumed.

Scan example:

```cpp
#include "bredis/MarkerHelpers.hpp"
...
namespace r = bredis;
...
using Buffer = boost::asio::streambuf;
...
Buffer rx_buff;
auto result_markers = c.read(rx_buff);
/* check for the responce */
auto eq_pong = r::marker_helpers::equality<Iterator>("PONG");
/* print true or false */
std::cout << boost::apply_visitor(eq_pong, result_markers.result) << "\n";
/* consume the buffers, after finish work with markers */
rx_buff.consume(result_markers.consumed);
```

For *extraction* of results it is possible to use either shipped extactors or write custom one. Shipped extractors detach (copy / convert) extraction results from receive buffer.

```cpp
#include "bredis/Extract.hpp"
...
auto result_markers = c.read(rx_buff);
auto extract = boost::apply_visitor(r::extractor<Iterator>(), result_markers.result);
/* safe to consume buffers now */
rx_buff.consume(result_markers.consumed);
/* we know what the type is, safe to unpack to string */
auto &reply_str = boost::get<r::extracts::string_t>(extract);
/* print "PONG" */
std::cout << reply_str.str << "\n";
```

Custom extractors (visitors) might be useful for performance-aware cases, e.g. when JSON in re-constructed in-place from using string reply markers **without** re-allocating whole JSON-string reply.

The underlying reason for decision to have final results in two steps (get markers and then scan/extract results) is caused by the fact that *receive buffer* might be scattered (fragmented). Scan and extraction can be performed without gathering receive buffers (i.e. without flattening / linearizing it).

In other words, *markers* have **referense semantics** (they refer memory regions in buffer, but do not own), while *extracted results* have **value semantics** (ownership).

## Syncronous TCP-connection example

```cpp
#include "bredis/Connection.hpp"
#include "bredis/MarkerHelpers.hpp"

#include <boost/variant.hpp>
...
namespace r = bredis;
namespace asio = boost::asio;
...
/* define used types */
using socket_t = asio::ip::tcp::socket;
using Buffer = boost::asio::streambuf;
using Iterator = typename r::to_iterator<Buffer>::iterator_t;
...
/* establishing connection to redis is outside of bredis */
asio::ip::tcp::endpoint end_point(
    asio::ip::address::from_string("127.0.0.1"), port);
socket_t socket(io_service, end_point.protocol());
socket.connect(end_point);

/* wrap socket to bredis connection */
r::Connection<socket_t> c(std::move(socket));

/* synchronously write command */
c.write("ping");

/* buffer is allocated outside of bredis connection*/
Buffer rx_buff;
/* get the result markers */
auto result_markers = c.read(rx_buff);
/* check for the responce */
auto eq_pong = r::marker_helpers::equality<Iterator>("PONG");
/* print true */
std::cout << boost::apply_visitor(eq_pong, result_markers.result) << "\n";
/* consume the buffers, after finish work with markers */
rx_buff.consume(result_markers.consumed);
```

In the ping example above the `PONG` reply string from redis is not (re)allocated, but directly scanned in the `rx_buff` using result markers. This can be useful for performance-aware cases, e.g. when JSON in re-constructed in-place from using string reply markers **without** re-allocating whole JSON-string reply.

In the case of need to **extract** reply (i.e. detach it from `rx_buff`), the following can be done:

```cpp
#include "bredis/Extract.hpp"
...
auto result_markers = c.read(rx_buff);
/* extract the results */
auto extract = boost::apply_visitor(r::extractor<Iterator>(), result_markers.result);
/* safe to consume buffers now */
rx_buff.consume(result_markers.consumed);
/* we know what the type is, safe to unpack to string */
auto &reply_str = boost::get<r::extracts::string_t>(extract);
/* print "PONG" */
std::cout << reply_str.str << "\n";
```

The examples above throw Exception in case of I/O or protocol error. It can be used as:

```cpp
boost::system::error_code ec;
c.write("ping", ec);
...
parse_result = c.read(rx_buff, ec);
```

in the case you don't want to the throw-exception behaviour

## Asyncronous TCP-connection example
```cpp
#include "bredis/Connection.hpp"
#include "bredis/MarkerHelpers.hpp"
...
namespace r = bredis;
namespace asio = boost::asio;
...
using socket_t = asio::ip::tcp::socket;
using Buffer = boost::asio::streambuf;
using Iterator = typename r::to_iterator<Buffer>::iterator_t;
using Policy = r::parsing_policy::keep_result;
using result_t = r::parse_result_mapper_t<Iterator, Policy>;

...
/* establishing connection to redis is outside of bredis */
asio::ip::tcp::endpoint end_point(
    asio::ip::address::from_string("127.0.0.1"), port);
socket_t socket(io_service, end_point.protocol());
socket.connect(end_point);
...
Buffer tx_buff, rx_buff;
c.async_write(
    tx_buff, "llen", "my-queue" [&](const auto &error_code, auto bytes_transferred) {
        /* tx_buff must be consumed when it is no longer needed */
        tx_buff.consume(bytes_transferred);
        c.async_read(rx_buff, [&](const auto &error_code, result_t &&r) {
            /* see above how to work wit result */
            auto extract = boost::apply_visitor(r::extractor<Iterator>(), r.result);
            auto &queue_size = boost::get<r::extracts::int_t>(extract);
            std::cout << "queue size: " << queue_size << "\n";
            ...
            /* consume rx_buff when it is no longer needed */
            rx_buff.consume(r.consumed);
        });
    });

```

In the example above separete receive and transfer buffers are used. In theory you can use only one buffer for both operations, but you must ensure that it will not be used simultaneously for reading and writing, in other words you cannot use [pipelining](https://redis.io/topics/pipelining) redis feature.


## Asyncronous unix domain sockets connection

The same as above, except the underlying socket type should be changed:

```cpp
using socket_t = asio::local::stream_protocol::socket;
```

## Subscriptions

There is no specific support of subscriptions, but you can easily build your own like

### synchronous subscription

```cpp
r::single_command_t subscribe_cmd{"subscribe", "some-channel1", "some-channel2"};
c.write(subscribe_cmd);
Buffer rx_buff;

/* get the 2 confirmations, as we subscribed to 2 channels */
r::marker_helpers::check_subscription<Iterator> check_subscription{std::move(subscribe_cmd)};
for (auto i = 0; i < 2; ++i){
  auto result_markers = c.read(rx_buff);
  bool confirmed =  boost::apply_visitor(check_subscription, result_markers.result);
  if (!confirmed) {
    // do something!
    ...;
  }
  rx_buff.consume(result_markers.consumed);
}

while(true) {
  auto result_markers = c.read(rx_buff);
  auto extract = boost::apply_visitor(r::extractor<Iterator>(), result_markers.result);
  rx_buff.consume(result_markers.consumed);

  /* process the result  */
  auto& array_reply = boost::get<r::extracts::array_holder_t>(extract);
  auto* type_reply = boost::get<r::extracts::string_t>(&array_reply.elements[0]);
  if (type_reply && type_reply->str == "message") {
      auto& channel = boost::get<r::extracts::string_t>(array_reply.elements[1]);
      auto& payload = boost::get<r::extracts::string_t>(array_reply.elements[2]);
      ...
  }
}
```

See `examples/synch-subscription.cpp` for the full example

### asynchronous subscription

The similar way of synchronous, i.e. push read callback initially and after each successfull read
```cpp
using Policy = r::parsing_policy::keep_result;
using ParseResult = r::parse_result_mapper_t<Iterator, Policy>;
using read_callback_t = std::function<void(const boost::system::error_code &error_code, ParseResult &&r)>;
using Extractor = r::extractor<Iterator>;
...
/* we can execute subscription command synchronously, as it is easier */
c.command("subscribe", "channel-1", "channel-2");
...
Buffer rx_buff;
read_callback_t notification_callback = [&](const boost::system::error_code,
                                            ParseResult &&r) {
    auto extract = boost::apply_visitor(Extractor(), r.result);
    rx_buff.consume(r.consumed);
    /* process the result, see above */
    ...
    /* re-trigger new message processing */
    c.async_read(rx_buff, notification_callback);
};

/* initialise listening subscriptions */
c.async_read(rx_buff, notification_callback);
```

See `examples/stream-parse.cpp` for the full example

## Transactions

There is no specific support for transactions in bredis, but you can easily build your own for you needs.

First, wrap your commands into tranaction:

```cpp

r::command_container_t tx_commands = {
    r::single_command_t("MULTI"),
        r::single_command_t("INCR", "foo"),
        r::single_command_t("GET", "bar"),
    r::single_command_t("EXEC"),
};
r::command_wrapper_t cmd(tx_commands);
c.write(cmd);
```

Then, as above there was **4** redis commands, there should come **4** redis
replies: `OK`, `QUEUED`, `QUEUED` and the array of results of execution of commands
in transaction (i.e. results for `INCR` and `GET` above)

```cpp
Buffer rx_buff;
c.async_read(rx_buff, [&](const auto& error_code, auto&& r){
    auto &replies = boost::get<r::markers::array_holder_t<Iterator>>(r.result);
    /* scan stream for OK, QUEUED, QUEUED */
    ...
    assert(replies.elements.size() == 4);
    auto eq_OK = r::marker_helpers::equality<Iterator>("OK");
    auto eq_QUEUED = r::marker_helpers::equality<Iterator>("QUEUED");
    assert(boost::apply_visitor(eq_OK, replies.elements[0]);
    assert(boost::apply_visitor(eq_QUEUED, replies.elements[1]));
    assert(boost::apply_visitor(eq_QUEUED, replies.elements[2]));

    /* get tx replies */
    auto &tx_replies = boost::get<r::markers::array_holder_t<Iterator>>(replies.elements[3]);
    ...;
    rx_buff.consume(r.consumed);
},
4); /* pay attention here */

```

## Futures & Coroutines

The similiar way as in `Boost::ASIO` (special thanks to Vinnie Falko for the suggestion)

### Futures

```cpp
#include <boost/asio/use_future.hpp>
...
Buffer rx_buff, tx_buff;
auto f_tx_consumed = c.async_write(tx_buff, "ping", asio::use_future);
auto f_result_markers = c.async_read(rx_buff, asio::use_future);
...
tx_buff.consume(f_tx_consumed.get());
auto result_markers = f_result_markers.get();
/* scan/extract result, and consume rx_buff as usual */
```

### Coroutines

```cpp
#include <boost/asio/spawn.hpp>
Buffer rx_buff, tx_buff;

boost::asio::spawn(
    io_service, [&](boost::asio::yield_context yield) mutable {
        boost::system::error_code error_code;
        auto consumed = c.async_write(tx_buff, "ping", yield[error_code]);
        tx_buff.consume(consumed);
        ...
        auto parse_result = c.async_read(rx_buff, yield[error_code], 1);
        /* scan/extract result */
        rx_buff.consume(parse_result.consumed);
    });
```

## Inspecting network traffic

See `t/SocketWithLogging.hpp` for example. The main idea is quite simple:
instead of providing real socket implementation supplied by `Boost::ASIO`,
provide an wrapper (proxy) which will **spy** on the traffic before
delegating it to/from `Boost::ASIO` socket.

## Cancellation & other socket operations

There is nothing specific with bredis, but if you need low-level socket
operations, instead of moving *socket* into bredis connection, you can
simply move a *reference* to it, and keep (own) the socket somewhere
outside of bredis connection.

```cpp
using socket_t = asio::ip::tcp::socket;
using next_layer_t = socket_t &;
...
asio::ip::tcp::endpoint end_point(asio::ip::address::from_string("127.0.0.1"), port);
socket_t socket(io_service, end_point.protocol());
socket.connect(end_point);
r::Connection<next_layer_t> c(socket);
...
socket.cancel();
```

## API

### `Iterator` template

underlying iterator type for used dynamic buffer type (e.g. `boost::asio::streambuf`)

### `redis_result_t<Iterator>`

Header: `include/bredis/Markers.hpp`

Namespace: `bredis::markers`


`boost::variant` for the basic types in redis protocol [](https://redis.io/topics/protocol),
i.e. the following marker types :
- `nil_t<Iterator>`
- `int_t<Iterator>`
- `string_t<Iterator>` (simple string and bulk strings)
- `error_t<Iterator>`
- `array_holder_t<Iterator>`

The basic type is `string_t<Iterator>`, which contains `from` and `to` members (`Iterator`),
where string is held. String does not contain special redis-protocol symbols, and other
metadata, i.e. can be used to extract/flatten the whole string.

`nil_t<Iterator>`, `int_t<Iterator>`, `error_t<Iterator>` just have `string` member
to point underlying string in redis protocol.

`array_holder_t` is recursive wrapper for the `redis_result_t<Iterator>`, it contains
`elements` member of `std::array` of `redis_result_t<Iterator>`

### `parse_result_t<Iterator>`

Header: `include/bredis/Result.hpp`

Namespace: `bredis`

Represents results of parse attempt. It is `boost::variant` of the following types:
- `no_enogh_data_t`
- `protocol_error_t`
- `positive_parse_result_t<Iterator>`

`no_enogh_data_t` is empty struct, meaning that buffer just does not contains enough
information to completely parse it.

`protocol_error_t` has `std::string what` member, descriping the error in protocol,
(e.g. when type in stream is specified as integer, but it cannot be converted to integer).
This error should never occur in production code, meaning that no (logical) errors
are expected in redis-server nor in bredis parser. The error might occur if buffer 
is corrupted.

`positive_parse_result_t<Iterator>` contains members:
- `markers::redis_result_t<Iterator> result` - result of mark-up buffer; can be used
either for scanning for particular results or for extraction of results.
- `size_t consumed` - how many bytes of receive buffer must be consumed, after
using `result` field.


### marker helpers

Header: `include/bredis/MarkerHelpers.hpp`

Namespace: `bredis::marker_helpers`

#### `stringizer<Iterator>`

Apply this `boost::static_visitor<std::string>`s for
stringize the result (can be useful for debugging).

#### `equality<Iterator>`

Apply this `boost::static_visitor<bool>` to find *string* in the
parsed results (the markup can point to integer types, but as it
is transferred as string anyway, it still can be founded as string
too)

Constructor: `equality<Iterator>(std::string str)`

#### `check_subscription<Iterator>`

This `boost::static_visitor<bool>` hepler is used to check
whether redis reply confirms to one of requested channels. Hence,
the constructor is `check_subscription(single_command_t)`.

Usually, the redis subscription reply is in the form:
```
[array] {
    [string] "subcribe",
    [string] channel_name,
    [int] reference
}
```

So, it checks, that:
1. Redis reply is 3-element array
2. The 1st reply element is string, and it *case-insentensively*
matches the command, i.e.  is is supposed, that
command will be `subscribe` or `psubscribe`.
3. That 3rd reply element is reference, and it is presented
among command arguments.

It is possible to reuse the same `check_subscription<Iterator>`
to *multiple* redis replies to signle subsription command.

Example:

```cpp
bredis::single_command_t subscribe_cmd{
    "subscribe", "channel-1", "channel-2"
};
...
// write command, so the subscribe_cmd
// will be no longer required
...;
bredis::marker_helpers::check_subscription<Iterator>
    check_subscription{std::move(subscribe_cmd)};
...;
// get the 1st reply
auto parse_result = ...;
bool channel_1_ok = boost::apply_visitor(check_subscription, parse_result.result);
...;
// get the 2nd reply
parse_result = ...;
bool channel_2_ok = boost::apply_visitor(check_subscription, parse_result.result);
```

### `command_wrapper_t`

Header: `include/bredis/Command.hpp`

Namespace: `bredis`

`boost::variant` for the basic commands:
- `single_command_t`
- `command_container_t`

`single_command_t` represents single redis command with all it's arguments, e.g.:

```cpp
// compile-time version
r::single_command_t cmd_ping {"ping"};
r::single_command_t cmd_get {"get", "queu-name"};
...
// or runtime-version
std::vector<std::string> subscription_items { "subscribe", "channel-a", "channel-b"};
r::single_command_t cmd_subscribe {
    subscription_items.cbegin(),
    subscription_items.cend()
};
```

The arguments must be conversible to `boost::string_ref`.

`command_container_t` is `std::vector` of `single_command_t`. Useful for transactions
or buck messages creation.

### `Connection<NextLayer>`

Header: `include/bredis/Connection.hpp`

Namespace: `bredis`

A thin wrapper around `NextLayer`; represents connection to redis. `NextLayer` can
be either `asio::ip::tcp::socket` or `asio::ip::tcp::socket&` or custom wrapper, which
follows the specification of `asio::ip::tcp::socket`.

Constructor `template <typename... Args> Connection(Args &&... args)` used for
construction of NextLayer (stream interface).

Stream interface accessors:
- `NextLayer &next_layer()`
- `const NextLayer &next_layer() const`

return underlying stream object.

#### Synchronous interface

Perform synchonous write of redis command:

- `void write(const command_wrapper_t &command)`
- `void write(const command_wrapper_t &command, boost::system::error_code &ec)`

Perform synchonous read of redis result until the buffer will be parsed or
some error (procol or I/O) occurs:

- `template <typename DynamicBuffer> positive_parse_result_t<Iterator> read(DynamicBuffer &rx_buff)`
- `template <typename DynamicBuffer> positive_parse_result_t<Iterator> read(DynamicBuffer &rx_buff, boost::system::error_code &ec);`

`DynamicBuffer` must conform `boost::asio::streambuf` interface.

#### Asynchronous interface

##### async_write

`WriteCallback` template should be callable object with the signature:

`void (const boost::system::error_code&, std::size_t bytes_transferred)`

The asynchnous write has the following signature:

```cpp
void-or-deduced
async_write(DynamicBuffer &tx_buff, const command_wrapper_t &command,
                WriteCallback write_callback)
```

It write the redis command (or commands) into *transfer buffer*, sends them
to the *next_layer* stream, and invokes `write_callback` after completion.

`tx_buff` must consume `bytes_transferred` upon `write_callback` invocation.

Client must guarantee that `async_write` is not invoked, until the previous
invocation is finished.

##### async_read

`ReadCallback` template should be callable object with the signature:

`void(boost::system::error_code, r::positive_parse_result_t<Iterator>&& result)`

The asynchnous read has the following signature:

```cpp
void-or-deduced
async_read(DynamicBuffer &rx_buff, ReadCallback read_callback,
               std::size_t replies_count = 1);
```

It reads `replies_count` replies from the *nex_layer* steam, which will be
stored in `rx_buff`, or until error (I/O or procol) will be met; then
`read_callback` will be invoked.

If `replies_count` is greater then `1`, the result type will always be
`bredis::array_wrapper_t`; if the `replies_count` is `1` then the result type
depends on redis answer type.

On `read_callback` invocation with successfull parse result it is expected,
that `rx_buff` will consume the specified in `result` amount of bytes.

Client must guarantee that `async_read` is not invoked, until the previous
invocation is finished. If you invoke `async_read` from `read_callback`
don't forget to **consume** `rx_buff` first, otherwise it leads to
subtle bugs.

# License

MIT

# Contributors

- [Vinnie Falco](https://github.com/vinniefalco)

## See also
- https://github.com/Cylix/cpp_redis
- https://github.com/hmartiro/redox
- https://github.com/nekipelov/redisclient
