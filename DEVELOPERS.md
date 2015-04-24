Qpid Proton Developer Information
=================================

Please see http://qpid.apache.org/proton/development.html for the current
roadmap.

Development Environment
-----------------------

Developers wishing to work across multiple languages should become
familiar with the cmake build system as this will build and run all
available tests and code whereas the maven build system only run Java
tests.

First you will need to set up your basic build environment with cmake and all
prequisites (see the instructions in INSTALL) so that you can build the full
codebase

To setup shell variables for your development environment, you must source
the file config.sh from the cmake build directory.

    $ cd build
    $ source config.sh

This file sets the needed environment variables for all supported dynamic
languages (Python, Perl, Ruby, PHP) as well as for Java and for testing.

Testing
-------

As a developer on proton, it is a good idea to build and test with the
appropriate dependencies and settings in place so that the complete set of
language bindings and implementations are present. There is a common test suite
that runs against both the proton-c and proton-j implementations to help keep
them in sync with each other - this is the python test suite that lives
underneath the top level `tests/python` directory. These tests have been
integrated into the maven build via Jython (and are hence included in the
proton-java ctest). When you run the python test suite in Jython, the swig
generated cproton doesn't actually exist since it is a C module. Instead, you
get the cproton.py that resides in the java source tree underneath
`proton-j/src/main/resources`.  This cproton.py and its dependent files serve as
a shim that adapts between the Java API and the C API.

### Running tests

To test Proton you should use the CMake build. By default this will invoke the
maven tests as well, so the maven prerequisites are required in addition to the
cmake prerequisites. 
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
