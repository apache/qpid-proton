## Multithreading {#threads}

The @ref pn_proactor allows you to create multi-threaded client and server
applications. You can call `pn_proactor_run` in multiple threads to create a
thread pool for proton to handle events and IO.

The @ref proactor functions are safe to call from any thread. Most other
functions are **not** safe to call concurrently. The following rules applies:
objects and events associated with *different* connections can safely be used
concurrently. Objects and events associated with *the same* connection cannot.

The proactor helps you to handle connections safely: `pn_proactor_get()` and
`pn_proactor_wait()` will never return events associated with the same
connection to different threads concurrently. As long as you process only the
objects available from the event returned by the proactor, you are in no danger.

When you need to process objects belonging to a *different* connection (for
example if you receive a message on one connection and want to send it on
another) you must use locks and thread safe data structures and the
`pn_connection_wake()` function.

### The wake function

`pn_connection_wake()` can be called from any thread as long as the
`pn_connection_t` has not been freed. It allows any thread to "wake up" a
connection by generating a @ref PN\_CONNECTION\_WAKE event for that connection.

For example, supposed a message arrives on connection A and must be sent on
connection B. The thread that receives events for A from the proactor can safely
call `pn_link_recv()` on A's link to decode the @ref message. However it cannot
call `pn_link_send()` on any link belonging to B.

The decoded @ref message is independent of any connection, so the thread
handling A can store the message in a thread-safe data structure, and call
`pn_connection_wake()` to notify B. Shortly after, some thread will get a @ref
PN\_CONNECTION\_WAKE event for B, and can retrieve the message and send it
on B's link.

@note Be careful to ensure (using a mutex for example) that connection B
cannot be deleted before connection A calls `pn_connection_wake`. Connections
are automatically deleted by the proactor after the @ref PN\_TRANSPORT\_CLOSED event
git
