Qpid Proton Install Information
===============================

The CMake build system can build the entire codebase, including proton-c,
and all its language bindings.

CMake (Linux)
-------------

The following prerequisites are required to do a full build on RPM based systems (RHEL, Fedora etc.).
If you do not wish to build a given language binding you can omit the devel
package for that language:

    # required dependencies
    $ yum install gcc cmake libuuid-devel

    # dependencies needed for ssl support
    $ yum install openssl-devel

    # dependencies needed for Cyrus SASL support
    $ yum install cyrus-sasl-devel

    # dependencies needed for bindings
    $ yum install swig          # Required for all bindings
    $ yum install python-devel                               # Python
    $ yum install ruby-devel rubygem-rspec rubygem-simplecov # Ruby
    $ yum install pphp-devel                                 # PHP
    $ yum install perl-devel                                 # Perl

    # dependencies needed for python docs
    $ yum install epydoc

The following prerequisites are required to do a full build on Debian based systems (Ubuntu). 
If you do not wish to build a given language binding you can omit the dev
package for that language:

    # Required dependencies 
    $ apt-get install gcc cmake cmake-curses-gui uuid-dev

    # dependencies needed for ssl support
    $ apt-get install libssl-dev

    # dependencies needed for Cyrus SASL support
    $ apt-get install libsasl2-2 libsasl2-dev

    # dependencies needed for bindings
    $ apt-get install swig python-dev ruby-dev libperl-dev

    # dependencies needed for python docs
    $ apt-get install python-epydoc

From the directory where you found this README file:

    $ mkdir build
    $ cd build

    # Set the install prefix. You may need to adjust depending on your
    # system.
    $ cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DSYSINSTALL_BINDINGS=ON

    # Omit the docs target if you do not wish to build or install
    # documentation.
    $ make all docs

    # Note that this step will require root privileges.
    $ make install

When make install completes, all installed files are listed in the
install_manifest.txt file. The contents of this file may be used to
uninstall.

Note: When SYSINSTALL_BINDINGS is enabled (ON), the
CMAKE_INSTALL_PREFIX does not affect the location for where the
language bindings (Python, Perl, PHP, Ruby) are installed. For those
elements, the location is determined by the language interpreter
itself; i.e., each interpreter is queried for the proper location for
extensions. If you want to constrain where the Proton code is
installed, set SYSINSTALL_BINDINGS to OFF. This will install all
bindings to a common location under ${CMAKE_INSTALL_PREFIX}. When
installed like this, each user will need to manually configure their
interpreters with the respective binding location.

CMake (Windows)
---------------

This describes how to build the Proton library on Windows using
Microsoft Visual C++.

The Proton build uses the CMake tool to generate the Visual Studio
project files. These project files can then be loaded into Visual
Studio and used to build the Proton library.

The following packages must be installed:

  - Visual Studio 2005 or newer (regular or C++ Express)
  - Python (www.python.org)
  - CMake (www.cmake.org)

Additional packages are required for the language bindings

  - swig (www.swig.org)
  - development headers and libraries for the language of choice

Notes:

  - be sure to install relevant Microsoft Service Packs and updates
  - python.exe, cmake.exe and swig.exe  _must_ all be added to your PATH

To generate the Visual Studio project files, from the directory where you found
this README file:

    > mkdir build
    > cd build
    > cmake ..

If CMake doesn't guess things correctly, useful additional arguments are:

    -G "Visual Studio 10"
    -DSWIG_EXECUTABLE=C:\swigwin-2.0.7\swig.exe

Refer to the CMake documentation for more information.

Build and install from a command prompt (using msbuild)
    > cmake --build . --target install --config RelWithDebInfo

Loading the ALL_BUILD project into Visual Studio

  1. Run the Microsoft Visual Studio IDE
  2. From within the IDE, open the ALL_BUILD project file or proton
     solution file - it should be in the 'build' directory you created
     above.
  3. Select the appropriate configuration. RelWithDebInfo works best
     with the included CMake/CTest scripts

Note that if you wish to build debug version of proton for use with
swig bindings on Windows, you must have the appropriate debug target
libraries to link against.

Installing Language Bindings
----------------------------

Most dynamic languages provide a way for asking where to install
libraries in order to place them in a default search path.

When SYSINSTALL_BINDINGS is disabled (OFF), Proton installs all
dynamic language bindings into a central, default location:

    BINDINGS=${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}/proton/bindings

In order to use these bindings, you'll need to configure your
interpreter to load the bindings from the appropriate directory:

 * Perl   - Add ${BINDINGS}/perl to PERL5LIB
 * PHP    - Set the PHPRC envvar to point to ${BINDINGS}/php/proton.ini
 * Python - Add ${BINDINGS}/python to PYTHONPATH
 * Ruby   - Add ${BINDINGS}/ruby to RUBYLIB

You can configure the build to install a specific binding to the
location specified by the system interpreter with the
SYSINSTALL_[LANGUAGE] options, where [LANGUAGE] is one of PERL,
PHP, PYTHON, or RUBY.:

    $ cmake .. -DSYSINSTALL_PHP=ON

Disabling Language Bindings
---------------------------

To disable any given language bindings, you can use the
BUILD_[LANGUAGE] option where [LANGUAGE] is one of PERL, PHP,
PYTHON or RUBY, e.g.:

    $ cmake .. -DBUILD_PHP=OFF

