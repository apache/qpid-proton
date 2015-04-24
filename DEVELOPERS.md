Qpid Proton Developer Information
=================================

Please see http://qpid.apache.org/proton/development.html for the current
roadmap.

Development Environment
-----------------------

Developers wishing to work across multiple languages should become
familiar with the CMake build system as this will build and run all
available tests and code whereas the maven build system only run Java
tests.

First you will need to set up your basic build environment with CMake and all
prerequisites (see the instructions in INSTALL) so that you can build the full
code base.

To setup shell variables for your development environment, you must source
the file config.sh from the CMake build directory.

    $ cd build
    $ source config.sh

This file sets the necessary environment variables for Java, for all supported
dynamic languages (Python, Perl, Ruby, PHP) and for running the tests.

Testing
-------

As a developer on Proton, it is a good idea to build and test with the
appropriate dependencies and settings in place so that the complete set of
language bindings and implementations are present.

Note that there is a common test suite written in python which will run against
both the proton-c and proton-j implementations to help keep them in sync with
each other. This can be found under the top level `tests/python` directory.
This has been integrated into the maven build via Jython (and is hence included
in the proton-java ctest suite). When you run the python test suite in
Jython, the swig generated cproton doesn't actually exist since it is a C
module. Instead, you get the `cproton.py` that resides in the Java source tree
under `proton-j/src/main/resources`.  This `cproton.py` and its dependent files
serve as a shim that adapts between the Java API and the C API.

### Running tests

To test Proton you should use the CMake build. By default this will invoke the
maven tests as well, so the maven prerequisites will additionally be required.

To run the tests, cd into your build directory and use the following commands:

    # to run all the tests, summary mode
    $ ctest

    # to list the available testsuites
    $ ctest -N

    # to run a single testsuite, verbose output
    $ ctest -V -R c-engine-tests

Additional packages required for testing the language bindings:

    # ruby dependencies
    $ yum install rubygem-minitest rubygem-rspec rubygem-simplecov

    # alternatively ruby depedencies on non-RPM based systems
    $ gem install minitest rspec simplecov


Mailing list
------------

Subscribe to the Qpid Proton mailing list here:

  http://qpid.apache.org/proton/mailing_lists.html


Patches
-------

The best way to submit patches is to create a bug report or feature request
on the project's JIRA instance:

  http://issues.apache.org/jira/browse/PROTON

You can attach any patch(es) to the report/request there
