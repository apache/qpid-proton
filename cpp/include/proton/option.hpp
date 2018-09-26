#ifndef PROTON_OPTION_HPP
#define PROTON_OPTION_HPP

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

#include <stdexcept>

namespace proton {

/// An option may or may not hold a value.
template <class T> class option {
    bool is_set_;
    T value_;

  public:
    /// An empty option - this->is_set() == false
    option() : is_set_(false), value_() {}

    /// An option containing x
    option(const T& x) : is_set_(true), value_(x) {}

    /// Set option value to x
    option& operator=(const T& x) { value_ = x;  is_set_ = true; return *this; }

    /// Copy value from another option if it is set, no op otherwise
    void update(const option<T>& x) { if (x.is_set()) *this = x.get(); }

    /// @return true if the option has a value
    bool is_set() const { return is_set_; }

    /// @return !is_set()
    bool empty() const { return !is_set_; }

    /// @return the option value if is_set()
    /// @throw std::logic_error if !is_set()
    T get() const {
        if (!is_set()) throw std::logic_error("proton::option is not set");
        return value_;
    }

    /// @return the option value if is_set() or x if not
    T get(const T& x) const { return is_set_ ? value_ : x; }
};

}
#endif // PROTON_OPTION_HPP
