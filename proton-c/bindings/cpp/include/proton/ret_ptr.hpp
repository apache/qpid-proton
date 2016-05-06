#ifndef PROTON_RET_PTR_HPP
#define PROTON_RET_PTR_HPP
/*
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
 */

#include "proton/config.hpp"
#include <memory>

namespace proton {

/// A simple unique ownership pointer, used only as a return value from
/// functions that transfer ownership to the caller.
///
/// If a ret_ptr return value is ignored, it will delete the return value
/// automatically. Otherwise implicitly converts to a plain pointer that must be
/// deleted by the caller using std::unique_ptr, std::shared_ptr, std::auto_ptr.
/// or operator delete
///
template <class T> class ret_ptr {
  public:
    ret_ptr(const ret_ptr& x) : ptr_(x) {}
    ~ret_ptr() { if (ptr_) delete(ptr_); }
    operator T*() const { T* p = ptr_; ptr_ = 0; return p; }

  private:
    void operator=(const ret_ptr&);
    ret_ptr(T* p=0) : ptr_(p) {}
    T* ptr_;
};

}

/// @endcond

#endif // PROTON_RET_PTR_HPP
