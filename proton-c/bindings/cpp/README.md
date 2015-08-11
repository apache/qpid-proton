# C++ binding for proton.

This is a C++ binding for the proton API.

The documentation includes a tutorial and API documentation.

To generate the documentation go to your build directory, run `make docs-cpp`
and open `proton-c/bindings/cpp/docs/html/index.html` in a browser.

# TO DO

Doc & website
- Fill out API documentation
- Update website with C++ docs.

Tests
- Interop/type testing: proton/tests/interop, new interop suite
- Unit testing for reasonable code coverage.
- Valgrind for automated unit and example tests.
- Test examples against AM-Q and qpidd.

Bugs
- Error handling: examples exit silently on broker exit/not running, core on no-such-queue.
- TODO notes in code.

Features
- SASL/SSL support with interop tests.
- Reconnection
- Browsing
- Selectors
- AMQP described types and arrays, full support and tests.
- Durable subscriptions & demos (see python changes)

# Nice to have

- Helpers (or at least doc) for multi-threaded use (container per connection)
- Usable support for decimal types.
