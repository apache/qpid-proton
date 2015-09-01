#ifndef COUNTED_HPP
#define COUNTED_HPP
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

namespace proton {

/// Base class for reference counted objects other than proton struct facade types.
class counted {
  protected:
    counted() : refcount_(0) {}
    virtual ~counted() {}

  private:
    counted(const counted&);
    counted& operator=(const counted&);
    int refcount_;

    friend void incref(const counted* p);
    friend void decref(const counted* p);
  template <class T> friend class counted_ptr;
};

// TODO aconway 2015-08-27: atomic operations.
inline void incref(const counted* p) { if (p) const_cast<counted*>(p)->refcount_ += 1; }
inline void decref(const counted* p) { if (p && --const_cast<counted*>(p)->refcount_ == 0) delete p; }

}
#endif // COUNTED_HPP
