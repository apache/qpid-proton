Verifying the examples 
======================

If you have installed support for self-testing a proton installation you can
automatically run and verify the examples.

To build C, C++ tests and run all tests:

    cd TEMP-BUILD-DIR
    cmake INSTALL-PREFIX/share/proton-VERSION/examples
    make
    ctest

To run an individual test that does not require CMake, (e.g. ruby):

    cd  INSTALL-PREFIX/share/proton-VERSION/examples/ruby
    ./testme

Some testme scripts accept additional arguments (e.g. -v for verbose)

