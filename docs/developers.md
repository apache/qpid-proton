Qpid Proton Developer Information
=================================

Development Environment
-----------------------

First you will need to set up your basic build environment with CMake and all
prerequisites (see the instructions in INSTALL) so that you can build the full
code base.

To setup shell variables for your development environment, you must source
the file config.sh from the CMake build directory.

    $ cd build
    $ source config.sh

This file sets the necessary environment variables for all supported
dynamic languages (Python and Ruby) and for running the tests.

Testing
-------

As a developer on Proton, it is a good idea to build and test with the
appropriate dependencies and settings in place so that the complete set of
language bindings and implementations are present.

### Running tests

To test Proton you should use the CMake build.

By default the unit tests are run using the system's default Python
interpreter.  However, Proton's Python language bindings support both
versions of the python language (Python 2.x and Python 3.x).  These
bindings should be tested using both versions of the Python
interpreter.  CMake makes this possible by automatically running the
Python unit tests under all versions of Python installed on the
system.

Developers can ensure that Proton remains compatible with both
versions of Python by installing the following prerequisites:

_Note: currently CMake only supports multi-Python testing in **Linux**
based environments.  Non-Linux developers may skip the following two
steps._

1. Installing both Python2.x and Python3.x and their associated
development environments on your system.  Most modern Linux
distributions support installing Python 2.x and Python 3.x in
parallel.

2. Install the **tox** Python testing tool, e.g. for older Fedora:

   $ yum install python-tox

   For newer Fedora:

   $ dnf install python3-tox redhat-rpm-config

To run the tests, cd into your build directory and use the following commands:

    # to run all the tests, summary mode
    $ ctest

    # to list the available testsuites
    $ ctest -N

    # to run a single testsuite, verbose output
    $ ctest -V -R c-engine-tests

Additional packages required for testing the language bindings:

    # ruby dependencies
    $ yum install rubygem-minitest

    # alternatively ruby depedencies on non-RPM based systems
    $ gem install minitest

To run coverage reporting:

    # install coverage tools
    $ dnf install lcov
    $ pip install coverage
    $ gem install simplecov

    $ cmake -DCMAKE_BUILD_TYPE=Coverage && make && ctest && make coverage
    # Then browse to {CMAKE_BUILD_DIR}/coverage_results/html/index.html to see C/C++ coverage
    # and to {CMAKE_BUILD_DIR}/ruby/coverage/index.html for Ruby coverage

Mailing list
------------

Subscribe to the Qpid mailing lists using details at:

  https://qpid.apache.org/discussion.html


Patches
-------

The best way to submit patches is to create a bug report or feature request
on the project's JIRA instance:

  http://issues.apache.org/jira/browse/PROTON

You can attach any patch(es) to the report/request there, or create a Pull Request
on the project's GitHub mirror:

  https://github.com/apache/qpid-proton

When creating a Pull Request, reference the associated JIRA by number
at the beginning of the commit message.