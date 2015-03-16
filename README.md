# Thrift via boost::asio

## motivation

### non-blocking
in time sensitive or "real time" applications, it might be desirable to use the RPC mechanisms of
[apache thrift](https://thrift.apache.org/) in a non-blocking, asynchronous manner.

By using [boost::asio](http://www.boost.org/doc/libs/release/libs/asio/) for the implementation for the client
transport and server no extra threads have to be created. The non-blocking server can serve multiple clients and can
be polled/updated from the main thread (or any other thread that is convenient).

another advantage is, that the application developer is in control of when exactly the RPC callbacks are called.

An interesting read on the topic is [The C10K problem](http://www.kegel.com/c10k.html).

### bidirectional

Apache thrift by design employs a client server model. That is, a client initiates a request, the server fulfills it.
There is no obvious way to perform bidirectional communication other than via the response to a request.

This implementation enables bidirectional communication without creating another network connection from the server to
the client. For this to work, all rpc functions have to be marked as 'oneway' to keep the beck-channel free of packets.

## design

This library is header-only. Also C++11 stl data-types are used in precedence over boosts wherever possible.

## License

This library is Distributed under the [Boost Software License, Version 1.0](http://www.boost.org/LICENSE_1_0.txt) .

## Bugs

In case you find any bugs, please don't hesitate to contact me. Also pull-requests are highly apprechiated.
