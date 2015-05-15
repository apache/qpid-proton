The Reactor API provides a means to dispatch events occurring across
one or more connections. It can be used purely as a dispatch tool
alongside your own I/O mechanism, however by default it is configured
with a handler that provides I/O for you.

When programming with the reactor it is important to understand the
dispatch model used to process events. Every event is associated with
a context object, i.e. the *target* object upon which the event
occurred. These objects are contained either directly or indirectly
within the Reactor:

    Delivery --> Link --> Session --> Connection --+
                                                   |
                                            Task --+--> Reactor
                                                   |
                                      Selectable --+


Each event is dispatched first to a target-specific handler, and
second to a global handler. The target-specific handler for an event
is located by searching from the event context up through the
hierarchy (terminating with the Reactor) and retrieving the most
specific handler found.

This means that any handler set on the Reactor could receive events
targeting any object. For example if no handlers are associated with a
Connection or any of its child objects, then the Reactor's handler
will receive all the events for that Connection.

Putting a handler on any child, e.g. a Connection or Session or Link
will prevent any handlers set on the ancestors of that object from
seeing any events targeted for that object or its children unless that
handler specifically chooses to delegate those events up to the
parent, e.g. by overriding onUnhandled and delegating.

The global handler (used to dispatch all events after the
target-specific handler is invoked) can be accessed and modified using
Reactor.set/getGlobalHandler. This can be useful for a number of
reasons, e.g. you could log all events by doing this:

    reactor.getGlobalHandler().add(new LoggerHandler());

Where LoggerHandler does this:

    public void onUnhandled(Event evt) {
        System.out.println(evt);
    }

The above trick is particularly useful for debugging.

Handlers themselves can have child handlers which will automatically
delegate the event to those children *after* dispatching the event to
itself. The default global handler is what provides the default I/O
behavior of the reactor. To use the reactor as a pure dispatch
mechanism you can simply set the global handler to null.
