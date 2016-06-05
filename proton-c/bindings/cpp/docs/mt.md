# Multithreaded Proton applications {#mt_page}

For an example see @ref mt/broker.cpp

Most classes in namespace @ref proton are not thread-safe. Objects
belonging to a single connection (`proton::connection`,
`proton::sender`, `proton::receiver`, and so on) *must not* be used
concurrently. Objects associated with *different* connections *can* be
used concurrently in separate threads.

A multithreaded container calls event-handling functions for each
connection *sequentially* but can process *different* connections
concurrently in different threads. If you use a *separate*
`proton::messaging_handler` for each connection, then event-handling
functions can can use their parameters and the handler's own data
members without locks. The handler functions will never be called
concurrently. You can set the handlers for each connection using
`proton::container::connect()` and `proton::container::listen()`.

The example @ref mt/broker.cpp is a multithreaded broker using this
approach.  It creates a new handler for each incoming connection to
manage the state of that connection's `proton::sender` and
`proton::receiver` links. The handler needs no lock because it only
deals with state in the context of one connection.

The `proton::event_loop` class represents the sequence of events
associated with a connection.  `proton::event_loop::inject()` allows
another thread to "inject" work to be executed in sequence with the
rest of the events so it can operate safely on data associated with
the connection.

In the @ref mt/broker.cpp example, a queue can receive messages from
one connection but have subscribers on another connection. Subscribers
pass a function object to the queue which uses
`proton::event_loop::inject()` to call a notification callback on the
handler for that connection. The callback is executed in the
connection's event loop so it can use a `proton::sender` object to
send the message safely.

*Note*: It is possible to share a single handler between more than one
connection.  In that case it *can* be called concurrently on behalf of
different connections, so you will need suitable locking.

@see @ref io_page - Implementing your own container.
