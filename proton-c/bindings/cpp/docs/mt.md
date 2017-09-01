# Multi-threading {#mt_page}

Full multi-threading support is available with C++11 and later. Limited
multi-threading is possible with older versions of C++, see the final section of
this page.

`proton::message` is a value type with the same threading constraints as a
standard C++ built-in type, it cannot be concurrently modified.

The `proton::container` is thread safe *with C++11 or greater*. It provides the following:

* User threads can open (or listen for) new connections at any time.
* Manages worker threads to process connections.
* Handles network IO and calls the relevant `proton::messaging_handler::on_xxx` event callbacks that can be over-ridden by user code.

The `proton::container` ensures that calls to event callbacks for each connection
instance are *serialized* (not called concurrently), but callbacks for different
connections may be called in parallel.

@note The `proton::connection` and related objects (`proton::session`,
`proton::sender`, `proton::receiver`, `proton::delivery`) are *not* thread safe, and
are subject to the following rules:

1. They may only be used from a `proton::messaging_handler` event callback called by proton (or a `proton::work_queue` function, more below)

2. You can't use objects belonging to one connection in a callback for a different connection. We recommend a single handler instance per connection to avoid confusion.

3. You can store proton objects in member variables for use in a later callback, provided you respect 2.

The `proton::work_queue` provides a safe way to communicate between different
connection handlers or between non-proton threads and connection handlers.

* Each connection has an associated `proton::work_queue`
* The work queue is thread safe (C++11 or greater), any thread can add work.
* "work" is a `std::function` and bound arguments to be called like an event callback.

When the work function is called by proton, it will be serialized safely so that
you can treat the work function like an event callback and safely access the
handler and proton objects stored on it.

The examples @ref multithreaded_client.cpp and @ref multithreaded_client_flow_control.cpp show
how you can send/receive messages from non-proton threads using these techniques.


## The wake primitive

`proton::connection::wake()` allows any thread to "wake up" a connection by
generating a `proton::messaging_handler::on_connection_wake()` callback. This is
the *only* thread-safe `proton::connection` function.

This is a light-weight, low-level primitive for signalling between threads:

* It does not carry any code or data (unlike `proton::work_queue`)
* Multiple calls to wake() can be "coalesced" into a single `proton::messaging_handler::on_connection_wake()`
* There may be calls to `proton::messaging_handler::on_connection_wake()` occur without any call to `connection::wake()`, generated internally by the library.

The semantics of `wake()` are similar to `std::condition_variable::notify_one`:
there will be a wakeup, but there must be some shared application state to
determine why the wakeup occurred and what, if anything, to do about it.

`proton::work_queue` is easier to use in many instances, but `wake()` may be
useful if you already have your own external thread-safe queues and just need an
efficient way to wake a connection to check them for data.

## Using older versions of C++

Before C++11 there was no standard support for threading in C++. You can use
proton with threads but with limitations:

* The container will not create threads, it will only use the single thread that calls proton::container::run()

* None of the proton library classes are thread-safe, including proton::container and proton::work_queue. You need an external lock to use proton::container in multiple threads.

The only exception is `proton::connection::wake()`, it *is* thread safe even in older C++.

You can implement your own `proton::container` using your own threading library,
or ignore the container and embed the lower-level `proton::connection_engine` in
an external poller. These approaches still use the same
`proton::messaging_handler` callbacks, so you can re-use most of your
application code. This is an advanced undertaking, there are a few pointers on
@ref io_page.
