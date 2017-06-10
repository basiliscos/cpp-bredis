# bredis
Boost::ASIO low-level redis client (connector)

[![Travis](https://img.shields.io/travis/basiliscos/cpp-bredis.svg)](https://travis-ci.org/basiliscos/cpp-bredis)
[![Build status](https://ci.appveyor.com/api/projects/status/a302juc7hrcdhhoc?svg=true)](https://ci.appveyor.com/project/basiliscos/cpp-bredis)
[![license](https://img.shields.io/github/license/basiliscos/cpp-bredis.svg)](https://github.com/basiliscos/cpp-bredis/blob/master/LICENSE)
[![Coveralls](https://img.shields.io/coveralls/basiliscos/cpp-bredis.svg)](https://codecov.io/gh/basiliscos/cpp-bredis)

## Features

- header only
- low-level controls (i.e. you can cancel, or do you manual DNS-resolving before connection)
- unix domain sockets support
- works on linux (clang, gcc) and windows (msvc)
- synchronous & asynchronous interface
- inspired by [beast](https://github.com/vinniefalco/Beast)

## Changelog

### 0.01
- initial version 

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
using ParseResult = r::positive_parse_result_t<Iterator>;
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
using result_t = r::positive_parse_result_t<Iterator>;

...
/* establishing connection to redis is outside of bredis */
asio::ip::tcp::endpoint end_point(
    asio::ip::address::from_string("127.0.0.1"), port);
socket_t socket(io_service, end_point.protocol());
socket.connect(end_point);
...
Buffer tx_buff, rx_buff;
c.async_write(
    tx_buff, "ping", [&](const auto &error_code, auto bytes_transferred) {
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
c.command("subscribe", "channel-1", "channel-2");
Buffer rx_buff;

while(true) {
  Buffer rx_buff;
  auto result_markers = c.read(rx_buff);
  auto extract = boost::apply_visitor(r::extractor<Iterator>(), result_markers.result);
  rx_buff.consume(result_markers.consumed);
  
  /* process the result, which might be subscription confirmation
     or a message channel */
  auto& array_reply = boost::get<r::extracts::array_holder_t>(extract);
  auto* type_reply = boost::get<r::extracts::string_t>(&array_reply.elements[0]);
  if (type_reply && type_reply->str == "message") {
      auto& channel = boost::get<r::extracts::string_t>(array_reply.elements[1]);
      auto& payload = boost::get<r::extracts::string_t>(array_reply.elements[2]);
      ...
  }
}
```
### asynchronous subscription

The similar way of synchronous, i.e. push read callback initially and after each successfull read
```cpp
using ParseResult = r::positive_parse_result_t<Iterator>;
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


## API

### `redis_result_t`

The `some_result_t` is `boost::variant` of the following types: 
- `string_holder_t`
- `error_holder_t`
- `nil_t`
- recursive array wrapper of `redis_result_t` (`boost::recursive_wrapper<array_holder_t>`)

`redis_result_t` and `error_holder_t` just have `str` member, which is basically `boost::string_ref`.

`nil_t` is obvious type to present `nil` redis result. 

`array_holder_t` has `elements` member, which is `std::vector` of `some_result_t`.

### `AsyncConnection<T>`

Type `T` can be either TCP socket type or unix-domain sockets (e.g. `boost::asio::ip::tcp::socket` or `boost::asio::local::stream_protocol::socket`). 

Constructor takes socket instance (`T&&`).

Method `cancel` cancels all pending I/O operations.

Method `push_command(const std::string &cmd, C &&contaier, command_callback_t callback)` pushes new redis command with optional list of arguments. `callback` is invoked on error(socket write, socket read error, redis protocol error) or on successfull result parsing. 

`command_callback_t` is `std::function<void(const boost::system::error_code &error_code, redis_result_t &&result)>`;

### `Subscription<T>`

Subscription is special mode, when redis server operates in `push` mode.

Type `T` can be either TCP socket type or unix-domain sockets (e.g. `boost::asio::ip::tcp::socket` or `boost::asio::local::stream_protocol::socket`). 

Constructor takes socket instance (`T&&`) and `command_callback_t callback`, which is executed on every incoming reply; the reply is not necessary `message`, e.g. it can be (un)subscription confirmation from redis.

Method `cancel` cancels all pending I/O operations.

Method `push_command(const std::string &cmd, C &&contaier)` pushes new redis command with optional list of arguments. By semantic the command can be either `subscribe`, `psubscribe`, `unsubscribe` and `punsubscribe`. 

```cpp
subscription.push_command("subscribe", {"some-channel-1", "some-channel-2"});
```

### `SyncConnection<T>`

Type `T` can be either TCP socket type or unix-domain sockets (e.g. `boost::asio::ip::tcp::socket` or `boost::asio::local::stream_protocol::socket`). 

Constructor takes socket instance (`T&&`).

Method `command` returns `redis_result_t`. It's signarute is `command(const std::string &cmd, C &&container, boost::asio::streambuf &rx_buff)`. `rx_buff` is used to store incoming data from redis server.

# License 

MIT

# Contributors

- [Vinnie Falco](https://github.com/vinniefalco)

## See also
- https://github.com/Cylix/cpp_redis
- https://github.com/blackjack/booredis
- https://github.com/nekipelov/redisclient
