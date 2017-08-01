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
    add_definitions(-DPN_CPP_HAS_${name}=1)
  endif()
endmacro()

check_cxx_source_compiles("#if defined(__cplusplus) && __cplusplus >= 201103\nint main(int, char**) { return 0; }\n#endif" CPP11)
# Don't need to check individual flags if compiler claims to be C++11 or later as they will be set automatically
if (NOT CPP11)
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_STANDARD} ${CXX_WARNING_FLAGS}")
  cxx_test("long long ll; int main(int, char**) { return 0; }" LONG_LONG)
  cxx_test("#include <memory>\nstd::shared_ptr<int> i; int main(int, char**) { return 0; }" SHARED_PTR)
  cxx_test("#include <memory>\nstd::unique_ptr<int> u; int main(int, char**) { return 0; }" UNIQUE_PTR)
  cxx_test("int* x = nullptr; int main(int, char**) { return 0; }" NULLPTR)
  cxx_test("#include <string>\nvoid blah(std::string&&) {} int main(int, char**) { blah(\"hello\"); return 0; }" RVALUE_REFERENCES)
  cxx_test("class x {explicit operator int(); }; int main(int, char**) { return 0; }" EXPLICIT_CONVERSIONS)
  cxx_test("class x {x(x&&)=default; }; int main(int, char**) { return 0; }" DEFAULTED_FUNCTIONS)
  cxx_test("class x {x()=delete; }; int main(int, char**) { return 0; }" DELETED_FUNCTIONS)
  cxx_test("struct x {x() {}}; int main(int, char**) { static thread_local x foo; return 0; }" THREAD_LOCAL)
  cxx_test("#include <functional>\nstd::function<int(void)> f = [](){return 42;}; int main(int, char**) { return 0; }" STD_FUNCTION)
  cxx_test("#include <functional>\nvoid f(int) {} int main(int, char**) { std::bind(f, 42); return 0; }" STD_BIND)
  cxx_test("#include <chrono>\nint main(int, char**) { return 0; }" CHRONO)
  cxx_test("#include <random>\nint main(int, char**) { return 0; }" RANDOM)
  cxx_test("#include <thread>\nstd::thread t; int main(int, char**) { return 0; }" STD_THREAD)
  cxx_test("#include <mutex>\nstd::mutex m; int main(int, char**) { return 0; }" STD_MUTEX)
  cxx_test("#include <atomic>\nstd::atomic<int> a; int main(int, char**) { return 0; }" STD_ATOMIC)
endif()
