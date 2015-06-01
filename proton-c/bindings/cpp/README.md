# C++ binding for proton.

This is a C++ wrapper for the proton reactor API.
It is very similar to the python wrapper for the same API.

There are [examples](../../../examples/cpp/README.md) and the header files have
API documentation in doxygen format.

# TO DO

There are a number of things that remain to be done.

- Type mapping between AMQP and C++ types, e.g. encoding std::map as map message.

- Finish blocking API & demos.
- API documentation, HTML docs on website.
- FIXME and TODO notes in code, esp. error handling, missing sender/receiver/connection methods.

- Valgrind for automated tests and demos.
- More automated tests and examples.

- Security: SASL/SSL support.
- Reconnection
