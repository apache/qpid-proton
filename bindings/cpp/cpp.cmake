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

# Check C++ capabilities.

include(CheckCXXSourceCompiles)

macro (cxx_test prog name)
  check_cxx_source_compiles("${prog}" HAS_${name})
  if (HAS_${name})
    list(APPEND CPP_DEFINITIONS "HAS_${name}")
  else()
    set(CPP_TEST_FAILED True)
  endif()
endmacro()

set(CPP_DEFINITIONS "")
set(CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_STANDARD} ${CXX_WARNING_FLAGS}")
cxx_test("#if defined(__cplusplus) && __cplusplus >= 201103\nint main(int, char**) { return 0; }\n#endif" CPP11)
# Don't need to check individual flags if compiler claims to be C++11 or later as they will be set automatically
if (NOT HAS_CPP11)
  set(CPP_TEST_FAILED False)
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

  # If all the tests passed this is the same as if we have C++11 for the purposes of compilation
  # (this shortens the compile command line for VS 2017 significantly)
  if (NOT CPP_TEST_FAILED)
    set(CPP_DEFINITIONS "HAS_CPP11")
  endif()
endif()
unset(CMAKE_REQUIRED_FLAGS) # Don't contaminate later C tests with C++ flags
