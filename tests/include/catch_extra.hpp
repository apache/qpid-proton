#ifndef CATCH_EXTRA_HPP
#define CATCH_EXTRA_HPP

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

// Extensions to the Catch2 framework

#include <catch.hpp>

namespace Catch {
namespace Matchers {

// Matcher for NUL terminated C-strings.

/* Example:
const char *x, *y

CHECK(x == y); // POINTER equality, probably not what you want.

using Catch::Matchers::Equals;
CHECK_THAT(x, Equals(y)); // String equality, like strcmp()
*/

namespace CString {

struct CStringMatcherBase : MatcherBase<const char *> {
  const std::string m_operation;
  const char *m_comparator;

  CStringMatcherBase(const std::string &operation, const char *comparator)
      : m_operation(operation), m_comparator(comparator) {}
  std::string describe() const CATCH_OVERRIDE {
    return m_operation + ": " + Catch::toString(m_comparator);
  }
};

struct EqualsMatcher : CStringMatcherBase {
  EqualsMatcher(const char *comparator)
      : CStringMatcherBase("equals", comparator) {}
  bool match(const char *source) const CATCH_OVERRIDE {
    // match if both null or both non-null and equal
    return source == m_comparator ||
           (source && m_comparator &&
            std::string(source) == std::string(m_comparator));
  }
};

struct ContainsMatcher : CStringMatcherBase {
  ContainsMatcher(const char *comparator)
      : CStringMatcherBase("contains", comparator) {}
  bool match(const char *source) const CATCH_OVERRIDE {
    return source && m_comparator && Catch::contains(source, m_comparator);
  }
};

struct StartsWithMatcher : CStringMatcherBase {
  StartsWithMatcher(const char *comparator)
      : CStringMatcherBase("starts with", comparator) {}
  bool match(const char *source) const CATCH_OVERRIDE {
    return source && m_comparator && Catch::startsWith(source, m_comparator);
  }
};

struct EndsWithMatcher : CStringMatcherBase {
  EndsWithMatcher(const char *comparator)
      : CStringMatcherBase("ends with", comparator) {}
  bool match(const char *source) const CATCH_OVERRIDE {
    return source && m_comparator && Catch::endsWith(source, m_comparator);
  }
};

} // namespace CString

inline CString::EqualsMatcher Equals(const char *str) {
  return CString::EqualsMatcher(str);
}
inline CString::ContainsMatcher Contains(const char *str) {
  return CString::ContainsMatcher(str);
}
inline CString::StartsWithMatcher StartsWith(const char *str) {
  return CString::StartsWithMatcher(str);
}
inline CString::EndsWithMatcher EndsWith(const char *str) {
  return CString::EndsWithMatcher(str);
}

} // namespace Matchers
} // namespace Catch

#endif // CATCH_EXTRA_HPP
