# Qpid Proton C++

This is a C++ binding for the Proton API.

The documentation includes a tutorial and API documentation.

To generate the documentation go to your build directory, run `make
docs-cpp`, and open `cpp/docs/html/index.html` in a
browser.

## Todo

### Tests

- Interop/type testing: proton/tests/interop, new interop suite
- More unit testing, measured code coverage
- Test examples against ActiveMQ and qpidd

### Bugs

- Error handling:
  - examples exit silently on broker exit/not running, core on
    no-such-queue (e.g., with qpidd)

### Features

- SASL/SSL support with interop tests.
- Reconnection
- Browsing
- Selectors
- AMQP described types and arrays, full support and tests
- Durable subscriptions & demos (see python changes)
- Transactions
- Heartbeats

### Nice to have

- C++11 lambda version of handlers
- Helpers (or at least doc) for multi-threaded use (container per connection)
- Usable support for decimal types
- Expose endpoint conditions as C++ proton::condition error class
- Selectables and 3rd party event loop support
- More efficient shared_ptr (single family per proton object)
