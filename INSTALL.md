Installing Qpid Proton
======================

The CMake build system can build the entire codebase, including proton-c,
and all its language bindings.

Dependencies
------------

Cross-platform dependencies

  - CMake 2.8.7+
  - Swig 1.3+ (for the bindings)
  - Python 2.6+ (for the Python binding)
  - Ruby 1.9+ (for the Ruby binding)

Linux dependencies

  - GNU Make 3.81+
  - GCC 4.4+
  - Cyrus SASL 2.1+ (for SASL support)
  - OpenSSL 1.0+ (for SSL support)

Windows dependencies

  - Visual Studio 2005 or newer (regular or C++ Express)

CMake (Linux)
-------------
The following are prerequisites for building on Alpine

    # Required dependencies
    $ apk add gcc g++ make cmake libuuid libressl-dev cyrus-sasl swig python3 python3-dev


The following prerequisites are required to do a full build on
RPM-based systems (RHEL, Fedora, etc.).  If you do not wish to build a
given language binding you can omit the devel package for that
language.

    # Required dependencies
    $ yum install gcc gcc-c++ make cmake libuuid-devel

    # Dependencies needed for SSL support
    $ yum install openssl-devel

    # Dependencies needed for Cyrus SASL support
    $ yum install cyrus-sasl-devel cyrus-sasl-plain cyrus-sasl-md5

    # Dependencies needed for bindings
    $ yum install swig                                       # Required for all bindings
    $ yum install python-devel                               # Python
    $ yum install ruby-devel rubygem-minitest                # Ruby

    # Dependencies needed to generate documentation
    $ yum install epydoc                                     # Python
    $ yum install rubygem-yard                               # Ruby
    $ yum install doxygen                                    # C, C++

The following prerequisites are required to do a full build on
Debian-based systems (Ubuntu).  If you do not wish to build a given
language binding you can omit the dev package for that language.

    # Required dependencies 
    $ apt-get install gcc g++ cmake cmake-curses-gui uuid-dev

    # Dependencies needed for SSL support
    $ apt-get install libssl-dev

    # dependencies needed for Cyrus SASL support
    $ apt-get install libsasl2-2 libsasl2-dev libsasl2-modules

    # dependencies needed for bindings
    $ apt-get install swig python-dev ruby-dev

    # dependencies needed for python docs
    $ apt-get install python-epydoc

From the directory where you found this `INSTALL.md` file:

    $ mkdir build
    $ cd build

    # Set the install prefix. You may need to adjust it depending on your
    # system.
    $ cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DSYSINSTALL_BINDINGS=ON

    # Omit the docs target if you do not wish to build or install
    # documentation.
    $ make all docs

    # Note that this step will require root privileges.
    $ make install

When make install completes, all installed files are listed in the
`install_manifest.txt` file. The contents of this file may be used to
uninstall.

Python distribution

inside build/python, run `python3 setup sdist` and upload the resulting build/python/dist/*.tar.gz to artifactory.

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

Additional packages are required for the language bindings:

  - Swig (www.swig.org)
  - Development headers and libraries for the language of choice

Notes:

  - Be sure to install relevant Microsoft Service Packs and updates
  - python.exe, cmake.exe and swig.exe _must_ all be added to your PATH

To generate the Visual Studio project files, from the directory where you found
this `INSTALL.md` file:

    > mkdir build
    > cd build
    > cmake ..

If CMake doesn't guess things correctly, useful additional arguments are:

    -G "Visual Studio 10"
    -DSWIG_EXECUTABLE=C:\swigwin-2.0.7\swig.exe

Refer to the CMake documentation for more information.

Build and install from a command prompt (using msbuild):

    > cmake --build . --target install --config RelWithDebInfo

Loading the `ALL_BUILD` project into Visual Studio:

  1. Run the Microsoft Visual Studio IDE
  2. From within the IDE, open the `ALL_BUILD` project file or Proton
     solution file - it should be in the `build` directory you created
     above.
  3. Select the appropriate configuration. RelWithDebInfo works best
     with the included CMake/CTest scripts

Note that if you wish to build debug version of Proton for use with
Swig bindings on Windows, you must have the appropriate debug target
libraries to link against.

Other Platforms
---------------

Proton can use the http://libuv.org IO library on any platform where
it is available. Install the libuv library and header files and adapt
the instructions for building on Linux.

The libuv library is not required on Linux or Windows, but if you wish
you can use it instead of the default native IO by running cmake with
`-Dproactor=libuv`.

Installing Language Bindings
----------------------------

Most dynamic languages provide a way for asking where to install
libraries in order to place them in a default search path.

When `SYSINSTALL_BINDINGS` is enabled (`ON`), the
`CMAKE_INSTALL_PREFIX` does not affect the location for where the
language bindings (Python and Ruby) are installed. For those
elements, the location is determined by the language interpreter
itself; that is, each interpreter is queried for the proper location
for extensions.

When `SYSINSTALL_BINDINGS` is disabled (`OFF`), Proton installs all
dynamic language bindings into a central, default location:

    BINDINGS=${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}/proton/bindings

In order to use these bindings, you'll need to configure your
interpreter to load the bindings from the appropriate directory.

  - Python - Add ${BINDINGS}/python to PYTHONPATH
  - Ruby   - Add ${BINDINGS}/ruby to RUBYLIB

You can configure the build to install a specific binding to the
location specified by the system interpreter with the
SYSINSTALL_[LANGUAGE] options, where [LANGUAGE] is one of PYTHON
or RUBY.

    $ cmake .. -DSYSINSTALL_PYTHON=ON

Disabling Language Bindings
---------------------------

To disable any given language bindings, you can use the
BUILD_[LANGUAGE] option where [LANGUAGE] is one of PYTHON
or RUBY, for example:

    $ cmake .. -DBUILD_PYTHON=OFF
