Installing Qpid Proton
======================

The CMake build system can build the entire codebase, including proton-c,
and all its language bindings.

Dependencies
------------

Cross-platform dependencies

  - CMake 3.16+
  - Python 3.9+ (required to build core C library)
  - Swig 1.3+ (for the Ruby binding)
  - Ruby 1.9+ (for the Ruby binding)
  - Go 1.11+ (for the Go binding)

Linux dependencies

  - GNU Make 3.81+
  - GCC 9+
  - Cyrus SASL 2.1+ (for SASL support)
  - OpenSSL 1.0.2a+ (for SSL support)
  - JsonCpp 1.8+ for C++ connection configuration file support

Windows dependencies

  - Visual Studio 2019 or newer (Community or Enterprise Editions)

CMake (Linux)
-------------

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
    $ yum install swig                                       # Required for ruby binding
    $ yum install python-devel                               # Python
    $ yum install ruby-devel rubygem-minitest                # Ruby
    $ yum install jsoncpp-devel                              # C++ optional config file

    # Dependencies needed to generate documentation
    $ yum install python-sphinx                              # Python
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
    $ apt-get install python-sphinx

From the directory where you found this `INSTALL.md` file:

    $ mkdir build
    $ cd build

    # Set the install prefix. You may need to adjust it depending on your
    # system.
    $ cmake .. -DCMAKE_INSTALL_PREFIX=/usr

    # Omit the docs target if you do not wish to build or install
    # documentation.
    $ make all docs

    # Note that this step will require root privileges.
    $ make install

When make install completes, all installed files are listed in the
`install_manifest.txt` file. The contents of this file may be used to
uninstall.

CMake (Windows)
---------------

This describes how to build the Proton library on Windows using
Microsoft Visual C++.

The Proton build uses the CMake tool to generate the Visual Studio
project files. These project files can then be loaded into Visual
Studio and used to build the Proton library.

The following packages must be installed:

  - Visual Studio 2019 or newer (Community or Enterprise Editions)
  - Python 3.9 or newer (www.python.org)
  - CMake (www.cmake.org)

Additional packages are required for language bindings:

  - Swig (www.swig.org) for the ruby bindings
  - Development headers and libraries for the language of choice

Notes:
  - Be sure to install relevant Microsoft Service Packs and updates
  - python.exe, cmake.exe and swig.exe _must_ all be added to your PATH
  - Chocolatey or scoop are useful tools that can be used to install cmake, python
    and swig (see https://chocolatey.org/ or https://scoop.sh/)

To generate the Visual Studio project files, from the directory where you found
this `INSTALL.md` file:

    > mkdir build
    > cd build
    > cmake ..

If CMake doesn't guess things correctly, useful additional arguments are:

    -G "Visual Studio 16 2019"
or

    -G "Visual Studio 17 2022"
and

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
it is available. The libuv library is not required on Linux or Windows,
but if you wish you can use it instead of the default native IO:
  - Install the libuv library and header files (`libuv-devel` on Fedora-based
    and `libuv1-dev` on Debian-based).
  - Run cmake with `-DPROACTOR=libuv`.


Installing Language Bindings
----------------------------

For the Python and Ruby language bindings the primary product is a package file that can be installed with the language specific package manager.

### Python
The built python packages will be found in the `python/dist` sub-directory of the build directory. They can be installed into an instance of python by using a comand like:

    > python -m pip install python/dist/python_qpid_proton-0.39.0-cp311-cp311-linux_x86_64.whl

### Ruby
The built ruby gem can be found in the `ruby/gem` sub-directory of the build directory. It can be installed into a ruby installation by using a command like:

    > gem install ruby/gem/qpid_proton-0.39.0.gem

Disabling Language Bindings
---------------------------

To disable any given language bindings, you can use the
BUILD_[LANGUAGE] option where [LANGUAGE] is one of PYTHON
or RUBY or GO, for example:

    $ cmake .. -DBUILD_PYTHON=OFF
