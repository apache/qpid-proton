# Multithreaded Proton {#mt_page}

**Experimental**

<!-- FIXME aconway 2016-05-04: doc -->

Most classes in namespace @ref proton are not thread-safe. Objects
associated with a single connection *must not* be used
concurrently. However, objects associated with *different* connections
*can* be used concurrently in separate threads.

The recommended way to use proton multithreaded is to *serialize* the
work for each connection but allow different connections to be
processed concurrently.

proton::container allows you to manage connections in a multithreaded
way. You supply a proton::messaging_handler for each
connection. Proton will ensure that the
`proton::messaging_handler::on_*()` functions are never called
concurrently so per-connection handlers do not need a lock even if
they have state.

proton::event_loop allows you to make calls to arbitrary functions or
other code, serialized in the same way as
`proton::messaging_handler::on_*()` calls. Typically this is used to
call your own handler's member functions in the same way as
proton::messaging_handler override functions.

For an example see @ref mt/broker.cpp.
