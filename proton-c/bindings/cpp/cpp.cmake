#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)

# Check C++ capabilities.

# NOTE: The checks here are for the C++ compiler used to build the proton *library*
#
# A developer using the library will get the checks done by internal/config.hpp
# which may not be the same, for example you can use a c++03 compiler to build
# applications that are linked with a library built with c++11.


set (BUILD_CPP_03 OFF CACHE BOOL "Compile the C++ binding as C++03 even when C++11 is available")

# Manual feature checks for older CMake versions

macro(cxx_compile_checks)

  macro (cxx_test prog name)
    check_cxx_source_compiles("${prog}" HAS_${name})
    if (HAS_${name})
      add_definitions(-DPN_CPP_HAS_${name}=1)
    endif()
  endmacro()

  check_cxx_source_compiles("#if defined(__cplusplus) && __cplusplus >= 201103\nint main(int, char**) { return 0; }\n#endif" CPP11)

  # Don't need to check individual flags if compiler claims to be C++11 or later as they will be set automatically
  if (NOT CPP11)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_STANDARD} ${CXX_WARNING_FLAGS}")
    cxx_test("long long ll; int main(int, char**) { return 0; }" LONG_LONG_TYPE)
    cxx_test("int* x = nullptr; int main(int, char**) { return 0; }" NULLPTR)
    cxx_test("#include <string>\nvoid blah(std::string&&) {} int main(int, char**) { blah(\"hello\"); return 0; }" RVALUE_REFERENCES)
    cxx_test("class x {explicit operator int(); }; int main(int, char**) { return 0; }" EXPLICIT_CONVERSIONS)
    cxx_test("class x {x()=default; }; int main(int, char**) { return 0; }" DEFAULTED_FUNCTIONS)
    cxx_test("class x {x(x&&)=default; }; int main(int, char**) { return 0; }" DEFAULTED_MOVE_INITIALIZERS)
    cxx_test("class x {x()=delete; }; int main(int, char**) { return 0; }" DELETED_FUNCTIONS)
    cxx_test("struct x {x() {}}; int main(int, char**) { static thread_local x foo; return 0; }" THREAD_LOCAL)
    cxx_test("int main(int, char**) { int a=[](){return 42;}(); return a; }" LAMBDAS)
    cxx_test("template <class... X> void x(X... a) {} int main(int, char**) { x(1); x(43, \"\"); return 0; }" VARIADIC_TEMPLATES)

    cxx_test("#include <random>\nint main(int, char**) { return 0; }" HEADER_RANDOM)
    cxx_test("#include <memory>\nstd::unique_ptr<int> u; int main(int, char**) { return 0; }" STD_UNIQUE_PTR)
    cxx_test("#include <thread>\nstd::thread t; int main(int, char**) { return 0; }" STD_THREAD)
    cxx_test("#include <mutex>\nstd::mutex m; int main(int, char**) { return 0; }" STD_MUTEX)
    cxx_test("#include <atomic>\nstd::atomic<int> a; int main(int, char**) { return 0; }" STD_ATOMIC)
  endif()
endmacro()


# Check for cmake C++ feature support (version 3.1 or later)
if (DEFINED CMAKE_CXX_COMPILE_FEATURES)
  if (BUILD_CPP_03)
    set(STD 98)
  else ()
    set(STD 11)
    list(FIND CMAKE_CXX_COMPILE_FEATURES cxx_std_11 INDEX)
    if (NOT ${INDEX} EQUAL -1)
      set(HAS_CPP11 True)
    endif()
  endif ()

  # Note: this will "degrade" to the highest available standard <= ${STD}
  set(CMAKE_CXX_STANDARD ${STD})
  set(CMAKE_CXX_EXTENSIONS OFF)
  # AStitcher 20170804: Disabled for present - work on this when Windows C++ works
  #  cmake_minimum_required(VERSION 3.1)
  #  include(WriteCompilerDetectionHeader)
  #  write_compiler_detection_header(
  #    FILE cpp_features.h
  #    PREFIX PN
  #    COMPILERS GNU Clang MSVC SunPro
  #    FEATURES ${CMAKE_CXX_COMPILE_FEATURES}
  #    ALLOW_UNKNOWN_COMPILERS)
  if (MSVC)  # Compiler feature checks only needed for Visual Studio in this case
    cxx_compile_checks()
  endif()

else (DEFINED CMAKE_CXX_COMPILE_FEATURES)

  if (BUILD_CPP_03)
    set(CXX_STANDARD "-std=c++98")
  else ()
    # These flags work with GCC/Clang/SunPro compilers
    check_cxx_compiler_flag("-std=c++11" ACCEPTS_CXX11)
    check_cxx_compiler_flag("-std=c++0x" ACCEPTS_CXX0X)
    if (ACCEPTS_CXX11)
      set(CXX_STANDARD "-std=c++11")
    elseif(ACCEPTS_CXX0X)
      set(CXX_STANDARD "-std=c++0x")
      cxx_compile_checks() # Compiler checks needed for C++0x as not all C++11 may be supported
    else()
      cxx_compile_checks() # Compiler checks needed as we have no idea whats going on here!
    endif()
  endif()
endif ()
