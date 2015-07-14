# C++ binding for proton.

This is a C++ binding for the proton API.

There are [examples](../../../examples/cpp/README.md) and the header files have
API documentation in doxygen format.

# TO DO

Doc & examples
- Finish example set and tutorial documentation, following pyton examples.
- Auto tests for all examples validating all statements & code in tutorial.
- C++ section on website.

Bugs
- Memory management
  - Drop PIMPL pattern in API: pn_foo pointer is already hiding the impl.
  - Proper ownership of pn_objects created by user, e.g. Message. Let user choose pointer style?
- Error handling, examples crash on error e.g. queue not found.
- TODO notes in code.

Tests
- Interop/type testing for full AMQP type coverage.
- Unit testing for reasonable code coverage.
- Valgrind for automated unit and example tests.

Features
- SASL/SSL support with interop tests.
- Reconnection
- Finish blocking API & examples.
- Described types, full support and tests.
- Durable subscriptions & demos (see python changes)

# Nice to have

- Helpers (or at least doc) for multi-threaded use (reactor/engine per connection)
- Usable support for decimal types.
