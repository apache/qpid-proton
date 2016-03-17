#ifndef TEST_BITS_HPP
#define TEST_BITS_HPP
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

#include "msg.hpp"
#include <proton/types.hpp>

#include <stdexcept>
#include <iostream>
#include <iterator>
#include <sstream>
#include <math.h>

namespace test {

struct fail : public std::logic_error {
    fail(const std::string& what) : logic_error(what) {}
};

template <class T, class U>
void assert_equal(const T& want, const U& got, const std::string& what) {
    if (!(want == got))
        throw fail(MSG(what << " " << want << " != " << got));
}

void assert_equalish(double want, double got, double delta, const std::string& what)
{
    if (!(fabs(want-got) <= delta))
        throw fail(MSG(what << " " << want << " !=~ " << got));
}

#define FAIL_MSG(WHAT) (MSG(__FILE__ << ":" << __LINE__ << ": " << WHAT).str())
#define FAIL(WHAT) throw fail(FAIL_MSG(WHAT))
#define ASSERT(TEST) do { if (!(TEST)) FAIL("failed ASSERT(" #TEST ")"); } while(false)
#define ASSERT_EQUAL(WANT, GOT) \
    assert_equal((WANT), (GOT), FAIL_MSG("failed ASSERT_EQUAL(" #WANT ", " #GOT ")"))
#define ASSERT_EQUALISH(WANT, GOT, DELTA) \
    assert_equalish((WANT), (GOT), (DELTA), FAIL_MSG("failed ASSERT_EQUALISH(" #WANT ", " #GOT ")"))

#define RUN_TEST(BAD_COUNT, TEST)                                       \
    do {                                                                \
        try {                                                           \
            TEST;                                                       \
            break;                                                      \
        } catch(const fail& e) {                                        \
            std::cout << "FAIL " << #TEST << std::endl << e.what() << std::endl; \
        } catch(const std::exception& e) {                              \
            std::cout << "ERROR " << #TEST << std::endl << __FILE__ << ":" << __LINE__ << ": " << e.what() << std::endl; \
        }                                                               \
            ++BAD_COUNT;                                                \
    } while(0)

template<class T> std::string str(const T& x) { std::ostringstream s; s << x; return s.str(); }

// A way to easily create literal collections that can be compared to std:: collections
// and to print std collections
// e.g.
//     std::vector<string> v = ...;
//     ASSERT_EQUAL(many<string>() + "a" + "b" + "c", v);
template <class T> struct many : public std::vector<T> {
    many() {}
    template<class S> explicit many(const S& s) : std::vector<T>(s.begin(), s.end()) {}
    many& operator+=(const T& t) { this->push_back(t); return *this; }
    many& operator<<(const T& t) { return *this += t; }
    many operator+(const T& t) { many<T> l(*this); return l += t; }
};

template <class T, class S> bool operator==(const many<T>& m, const S& s) {
    return m.size() == s.size() && S(m.begin(), m.end()) == s;
}

template <class T, class S> bool operator==(const S& s, const many<T>& m) {
    return m.size() == s.size() && S(m.begin(), m.end()) == s;
}

template <class T> std::ostream& operator<<(std::ostream& o, const many<T>& m) {
    std::ostream_iterator<T> oi(o, " ");
    std::copy(m.begin(), m.end(), oi);
    return o;
}

}

namespace std {
template <class T> std::ostream& operator<<(std::ostream& o, const std::vector<T>& s) {
    return o << test::many<T>(s);
}

template <class T> std::ostream& operator<<(std::ostream& o, const std::deque<T>& s) {
    return o << test::many<T>(s);
}

template <class T> std::ostream& operator<<(std::ostream& o, const std::list<T>& s) {
    return o << test::many<T>(s);
}

template <class K, class T> std::ostream& operator<<(std::ostream& o, const std::map<K, T>& x) {
    return o << test::many<std::pair<K, T> >(x);
}

template <class U, class V> std::ostream& operator<<(std::ostream& o, const std::pair<U, V>& p) {
    return o << "( " << p.first << " , " << p.second << " )";
}

#if PN_CPP_HAS_CPP11
template <class K, class T> std::ostream& operator<<(std::ostream& o, const std::unordered_map<K, T>& x) {
    return o << test::many<std::pair<const K, T> >(x);
}

template <class T> std::ostream& operator<<(std::ostream& o, const std::forward_list<T>& s) {
    return o << test::many<T>(s);
}
#endif
}

#endif // TEST_BITS_HPP
