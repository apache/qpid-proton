# Multithreaded Proton applications {#mt_page}

For examples see @ref broker.cpp, @ref multithreaded_client.cpp, and
@ref multithreaded_client_flow_control.cpp.

Most classes in namespace @ref proton are not thread-safe. Objects
belonging to a single connection (`proton::connection`,
`proton::sender`, `proton::receiver`, and so on) *must not* be used
concurrently. Objects associated with *different* connections *can* be
used concurrently in separate threads.

A multithreaded container calls event-handling functions for each
connection *sequentially* but can process *different* connections
concurrently in different threads. If you use a separate
`proton::messaging_handler` for each connection, then event-handling
functions can use their parameters and the handler's own data
members without locks. The handler functions will never be called
concurrently. You can set the handlers for each connection using
`proton::container::connect()` and `proton::container::listen()`.

The example @ref broker.cpp is a broker that can be run in single- or
multithreaded mode.  It creates a new handler for each incoming
connection to manage the state of that connection's `proton::sender`
and `proton::receiver` links. The handler needs no lock because it
only deals with state in the context of one connection.

The examples @ref multithreaded_client.cpp and @ref
multithreaded_client_flow_control.cpp show how application threads can
communicate safely with Proton handler threads.
