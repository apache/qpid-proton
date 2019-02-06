/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */
#include <stdint.h>
#include <stdlib.h>
#include <cxxabi.h>
#include <iostream>
#include <string>
#include <vector>
#include <typeinfo>

void print_type(const char* type, const char* mangled_type)
{
  int status;
  char* demangled_type =
    abi::__cxa_demangle(mangled_type, 0, 0, &status);
  if (demangled_type) {
    std::cout << "s/" << type << "/" << demangled_type << "/g\n";
  }
  ::free(demangled_type);
}

#define mangle_name(x) typeid(x).name()

#define print_subst(x) print_type(#x, mangle_name(x))

int main() {
  print_subst(uint64_t);
  print_subst(uint32_t);
  print_subst(uint16_t);
  print_subst(uint8_t);
  print_subst(size_t);
  print_subst(int64_t);
  print_subst(int32_t);
  print_subst(int16_t);
  print_subst(int8_t);
  print_subst(std::string);
  print_subst(std::vector<char>);
}
