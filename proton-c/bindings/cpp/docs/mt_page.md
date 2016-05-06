# Multi-threaded proton {#mt_page}

<!-- FIXME aconway 2016-05-04: doc -->

Most classes in namespace @ref proton are not thread-safe. Objects associated
with a single connection *must not* be used concurrently. However objects
associated with *different* connections *can* be used concurrently in separate
threads.

The recommended way to use proton multi-threaded is to *serialize* the work for
each connection but allow different connections to be processed concurrently.

proton::container allows you to manage connections in a multi-threaded way. You
supply a proton::handler for each connection. Proton will ensure that the
`proton::handler::on_*()` functions are never called concurrently so
per-connection handlers do not need a lock even if they have state.

proton::work_queue allows you to make calls to arbitrary functions or other
code, serialized in the same way as `proton::handler::on_()` calls. Typically
this is used to call your own handler's member functions in the same way as
proton::handler override functions.

For examples see @ref mt/broker.cpp, mt/simple\_send.cpp and mt/simple\_recv.cpp
