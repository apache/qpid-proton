# Multithreading {#mt_page}

Full multithreading support is available with C++11 and later. Limited
multithreading is possible with older versions of C++.  See the last
section of this page for more information.

`proton::container` handles multiple connections concurrently in a
thread pool, created using `proton::container::run()`. As AMQP events
occur on a connection the container calls `proton::messaging_handler`
event callbacks.  The calls for each connection are *serialized* -
callbacks for the same connection are never made concurrently.

You assign a handler to a connection in `proton::container::connect()`
or `proton::listen_handler::on_accept()` with
`proton::connection_options::handler()`.  We recommend you create a
separate handler for each connection.  That means the handler doesn't
need locks or other synchronization to protect it against concurrent
use by Proton threads.  If you use the handler concurrently from
non-Proton threads then you will need synchronization.

The examples @ref multithreaded_client.cpp and @ref
multithreaded_client_flow_control.cpp illustrate these points.

## Thread-safety rules

`proton::container` is thread-safe *with C++11 or greater*.  An
application thread can open (or listen for) new connections at any
time. The container uses threads that call `proton::container::run()`
to handle network IO and call user-defined `proton::messaging_handler`
callbacks.

`proton::container` ensures that calls to event callbacks for each
connection instance are *serialized* (not called concurrently), but
callbacks for different connections can be safely executed in
parallel.

`proton::connection` and related objects (`proton::session`,
`proton::sender`, `proton::receiver`, `proton::delivery`) are *not*
thread-safe and are subject to the following rules.

1. They can only be used from a `proton::messaging_handler` event
   callback called by Proton or a `proton::work_queue` function (more
   below).

2. You cannot use objects belonging to one connection from a callback
   for another connection.  We recommend a single handler instance per
   connection to avoid confusion.

3. You can store Proton objects in member variables for use in a later
   callback, provided you respect rule two.

`proton::message` is a value type with the same threading constraints
as a standard C++ built-in type.  It cannot be concurrently modified.

## Work queues

`proton::work_queue` provides a safe way to communicate between
different connection handlers or between non-Proton threads and
connection handlers.

 * Each connection has an associated `proton::work_queue`.

 * The work queue is thread-safe (C++11 or greater).  Any thread can
   add *work*.

 * *Work* is a `std::function`, and bound arguments will be
   called like an event callback.

When the work function is called by Proton, it will be serialized
safely so that you can treat the work function like an event callback
and safely access the handler and Proton objects stored on it.

The examples @ref multithreaded_client.cpp and @ref
multithreaded_client_flow_control.cpp show how you can send and
receive messages from non-Proton threads using work queues.

## The wake primitive

`proton::connection::wake()` allows any thread to "wake up" a
connection by generating an `on_connection_wake()` callback. This is
the *only* thread-safe `proton::connection` function.

This is a lightweight, low-level primitive for signaling between
threads.

 * It does not carry any code or data (unlike `proton::work_queue`).

 * Multiple calls to `wake()` can be "coalesced" into a single
   `on_connection_wake()`.

 * Calls to `on_connection_wake()` can occur without any call to
   `connection::wake()`.  Proton uses wake internally.

The semantics of `wake()` are similar to
`std::condition_variable::notify_one`.  There will be a wakeup, but
there must be some shared application state to determine why the
wakeup occurred and what, if anything, to do about it.

Work queues are easier to use in many instances, but `wake()` may be
useful if you already have your own external thread-safe queues and
just need an efficient way to wake a connection to check them for
data.

## Using older versions of C++

Before C++11 there was no standard support for threading in C++.  You
can use Proton with threads but with the following limitations.

 * The container will not create threads, it will only use the single
   thread that calls `proton::container::run()`.

 * None of the Proton library classes are thread-safe, including
   `proton::container` and `proton::work_queue`.  You need an external
   lock to use `proton::container` in multiple threads.

The only exception is `proton::connection::wake()`, it *is*
thread-safe even in older C++.

You can implement your own `proton::container` using your own
threading library, or ignore the container and embed the lower-level
`proton::io::connection_driver` in an external poller. These
approaches still use the same `proton::messaging_handler` callbacks,
so you can reuse most of your application code.  Note that this is an
advanced undertaking.  There are a few pointers in @ref io_page.
