Proton memory mangement
=======================

Proton is a collection of libraries in different programming langauges. Some of
the libraries (e.g. C and Java) are written directly in those langauges. Some
(e.g. python and ruby) are "bindings", native libraries that that internally use
the C library. All the libraries provide a "native" memory management
experience, you don't need to know anything about C or the proton C library to
use them.

If you only program in java, python, ruby or a similar garbage-collected
language then you may be wondering "what is *memory management*?"  Don't worry,
the proton library takes care of it. However, as usual, you are responsible for
some non-memory *resource* management. For example if you fail to `close()` a
proton connection there won't be memory leaks but the remote end will get a rude
"connection failed" error instead of an orderly connection close.

If you are a C programmer, you are probably rolling your eyes and wondering why
the kids these days expect their memory to be managed for them. Don't worry, the
C API offers the standard C experience: you must call `pn_X_free` on a `pn_X_t*`
when the time is right and you must know when a `pn_X_t*` can become invalid as
a side effect of some other call. Read the doc, learn the rules and get it
right. You are a C programmer!

If you are a modern C++11 programmer and always use `shared_ptr` and
`unique_ptr` to write leak-safe code, then the C++ binding is leak-safe. If you
are stuck on C++03, the binding supports `boost::shared_ptr` and
`boost::intrusive_ptr`. If you cannot even use boost, the binding provides a
simple (type safe, intrusive) smart pointer of its own which can help you.

If you are a Go programmer, you know that Go takes care of *Go-allocated* memory
but you are responsible other resource cleanup, e.g. closing connections.  Go
does not have object-scoped cleanup (or indeed objects) but as a crafty Go
programmer you know that function-scoped cleanup is all you really need, and
`defer` is your friend. The Go binding internally starts goroutines and
allocates memory that is not tracked by Go, so proper cleanup is important (but
you knew that.)

The role of reference counts
----------------------------

Internally, the proton C library uses reference counting, and you can
*optionally* use it in your code. You should choose *either* reference counting
*or* `pn_X_free` in your code, *not both*. It might work, but it is the sort of
Bad Idea that might break your code in the future and will hurt your head in the
present. `pn_X_free` is all you really need to write an AMQP application in
straight C.

However, proton is designed to be embedded and integrated. If you are
integrating proton with a new programming language, or some other kind of
framework, reference counts may be useful. If your integration target has some
form of automatic clean-up that you can hook into (reference-counted pointers,
finalizers, destructors or the like) then reference counts may help
(e.g. python, ruby and C++ bindings all use them).

As a counter-example the Go language is garbage collected, and has finalizers,
but does not use reference counts. Go garbage collection is scheduled around
memory use, so the timing may not be suitable for other resources. The Go
binding does not use proton reference counts, it simply calls `pn_X_free` as
part of resource cleanup (e.g. during Connection.Close()) or via finalizers as a
fail-safe if resources are not cleaned up properly by the application.

If you are mixing your own C code with code using a reference-counted proton
binding (e.g. C++ or python) then you may need to at least be aware of reference
counting.

You can even use reference counts in plain C code if you find that helpful. I
don't see how it would be but you never know.

The reference counting rules
----------------------------

The proton C API has standard reference counting rules (but see [1] below)

- A pointer *returned* by a `pn_` function is either *borrowed* by the caller,
  or the caller *owns* a reference (the API doc should say which)
- The owner of a reference must call `pn_decref()` exactly once to
  release it.
- To keep a borrowed pointer, call `pn_incref()`. This adds a new
  reference, which you now own.
- A pointer *passed* to a `pn_` function has no change of ownership. If you
  owned a reference you still do, if you didn't you still don't.
- An object is never freed while there are still references to it.
- An object is freed when all references to it are released.

A *borrowed* pointer is valid within some scope (typically the scope of an event
handling function) but beyond that scope you cannot assume it is valid unless
you make a new reference with `pn_incref`. The API documentation for the
function that returned the pointer should tell you what the scope is.

There are "container" relationships in proton: e.g. a connection contains
sessions, a session contains links. Containers hold a reference to their
contents. Freeing a container *releases* that reference. For example freeing a
connection releases its sessions.

If you don't use reference counts, then freeing a container *frees* the contents
in traditional C style. However if you add a reference to a contained object it
will *not* be freed till you release your reference, even if all references to
container are released [1]. This is useful for bindings to langauges with
"finalizers" or "destructors". You can use reference counts to "pin" proton C
objects in memory for as long as any binding object needs them.

For example: if you call `pn_message()` then you *own* a reference to the
newly-created `pn_message_t` and you must call `pn_decref` when you are
done [2]. If you call `pn_event_link()` in an event handler then you get a
*borrowed* reference to the link. You can use it in the scope of the event
handler, but if you want to save it for later you must call `pn_incref`
to add a reference and of course call `pn_decref` when you are done with
that reference.

You should treat `pn_decref` *exactly* like freeing the object: the pointer is
dead to you, you must never even look at it again. *Never* write code that
assumes that "something else" still has a reference after you have released your
own. *Never* write code that checks the value of the reference count (except for
debugging purposes.) If you own a reference you can use the pointer. Once you
release your reference, the pointer is dead to you. That's the whole story.

The point of reference counts is to break ownership dependencies between
different parts of the code so that everything will Just Work provided each part
of the code independently obeys the simple rules. If your code makes assumptions
about distant refcounts or "peeks" to vary its behavior based on what others are
doing, you defeat the purpose of reference counting [1].

[1] *Internally* the proton library plays tricks with reference counts to
implement 'weak' pointers and manage circular containment relationships. You do
*not* need to understand this to use proton, even if you are writing bindings or
doing funky mixed-language development. However if you are working on the
implementation of the proton C library itself you may need to learn more, ask on
proton@qpid.apache.org.

[2] Actually if you call `pn_message()` then you must *either* call
`pn_decref()` *or* `pn_message_free()`, definitely not both. It is
possible to mix reference couting and 'free' style memory management in the same
codebase (`free` is sort-of an alias for `decref`) but it is probably not a good
idea.
