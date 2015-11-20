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

#include <proton/value.hpp>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <vector>
#include <list>


// Examples of how to use the encoder and decoder to create and examine AMQP values.
//

// Print is defined at the end as an example of how to query and extract complex
// values from a decoder in terms of their simple components.
void print(proton::value&);

// Some helper templates to print map and std::vector results.
namespace std {
template<class T, class U> ostream& operator<<(ostream& o, const std::pair<T,U>& p) {
    return o << p.first << ":" << p.second;
}
template<class T> ostream& operator<<(ostream& o, const std::vector<T>& v) {
    o << "[ ";
    ostream_iterator<T> oi(o, " ");
    copy(v.begin(), v.end(), oi);
    return o << "]";
}
template<class T> ostream& operator<<(ostream& o, const std::list<T>& v) {
    o << "[ ";
    ostream_iterator<T> oi(o, " ");
    copy(v.begin(), v.end(), oi);
    return o << "]";
}
template<class K, class T> ostream& operator<<(ostream& o, const map<K, T>& m) {
    o << "{ ";
    ostream_iterator<std::pair<K,T> > oi(o, " ");
    copy(m.begin(), m.end(), oi);
    return o << "}";
}
}

// Insert/extract native C++ containers with uniform type values.
void uniform_containers() {
    std::cout << std::endl << "== Array, list and map of uniform type." << std::endl;
    proton::value v;

    std::vector<int> a;
    a.push_back(1);
    a.push_back(2);
    a.push_back(3);
    // By default a C++ container is encoded as an AMQP array.
    v = a;
    print(v);
    std::list<int> a1;
    v.get(a1);                  // Decode as a C++ std::list instead
    std::cout << a1 << std::endl;

    // You can specify that a container should be encoded as an AMQP list instead.
    v = proton::as<proton::LIST>(a1);
    print(v);
    std::cout << v.get<std::vector<int> >() << std::endl;

    // C++ map types (types with key_type, mapped_type) convert to an AMQP map by default.
    std::map<std::string, int> m;
    m["one"] = 1;
    m["two"] = 2;
    v = m;
    print(v);
    std::cout << v.get<std::map<std::string, int> >() << std::endl;

    // You can convert a sequence of pairs to an AMQP map if you need to control the
    // encoded ordering.
    std::vector<std::pair<std::string, int> > pairs;
    pairs.push_back(std::make_pair("z", 3));
    pairs.push_back(std::make_pair("a", 4));
    v = proton::as<proton::MAP>(pairs);
    print(v);
    // You can also decode an AMQP map as a sequence of pairs using decoder() and proton::to_pairs
    std::vector<std::pair<std::string, int> > pairs2;
    v.decoder() >> proton::to_pairs(pairs2);
    std::cout << pairs2 << std::endl;
}

// Containers with mixed types use value to represent arbitrary AMQP types.
void mixed_containers() {
    std::cout << std::endl << "== List and map of mixed type values." << std::endl;
    proton::value v;

    std::vector<proton::value> l;
    l.push_back(proton::value(42));
    l.push_back(proton::value(proton::amqp_string("foo")));
    // By default, a sequence of proton::value is treated as an AMQP list.
    v = l;
    print(v);
    std::vector<proton::value> l2;
    v.get(l2);
    std::cout << l2 << std::endl;

    std::map<proton::value, proton::value> m;
    m[proton::value("five")] = proton::value(5);
    m[proton::value(4)] = proton::value("four");
    v = m;
    print(v);
    std::map<proton::value, proton::value> m2;
    v.get(m2);
    std::cout << m2 << std::endl;
}

// Insert using stream operators (see print_next for example of extracting with stream ops.)
void insert_stream_operators() {
    std::cout << std::endl << "== Insert with stream operators." << std::endl;
    proton::value v;

    // Create an array of INT with values [1, 2, 3]
    v.encoder() << proton::start::array(proton::INT)
                << proton::amqp_int(1) << proton::amqp_int(2) << proton::amqp_int(3)
                << proton::finish();
    print(v);

    // Create a mixed-type list of the values [42, false, "x"].
    v.encoder() << proton::start::list()
                << proton::amqp_int(42) << false << proton::amqp_symbol("x")
                << proton::finish();
    print(v);

    // Create a map { "k1":42, "k2": false }
    v.encoder() << proton::start::map()
                << "k1" << proton::amqp_int(42)
                << proton::amqp_symbol("k2") << false
                << proton::finish();
    print(v);
}

int main(int, char**) {
    try {
        uniform_containers();
        mixed_containers();
        insert_stream_operators();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << std::endl << "error: " << e.what() << std::endl;
    }
    return 1;
}

// print_next prints the next value from values by recursively descending into complex values.
//
// NOTE this is for example puroses only: There is a built in ostream operator<< for values.
//
//
void print_next(proton::decoder& d) {
    proton::type_id type = d.type();
    proton::start s;
    switch (type) {
      case proton::ARRAY: {
          d >> s;
          std::cout << "array<" << s.element;
          if (s.is_described) {
              std::cout  << ", descriptor=";
              print_next(d);
          }
          std::cout << ">[";
          for (size_t i = 0; i < s.size; ++i) {
              if (i) std::cout << ", ";
              print_next(d);
          }
          std::cout << "]";
          d >> proton::finish();
          break;
      }
      case proton::LIST: {
          d >> s;
          std::cout << "list[";
          for (size_t i = 0; i < s.size; ++i) {
              if (i) std::cout << ", ";
              print_next(d);
          }
          std::cout << "]";
          d >> proton::finish();
          break;
      }
      case proton::MAP: {
          d >> s;
          std::cout << "map{";
          for (size_t i = 0; i < s.size/2; ++i) {
              if (i) std::cout << ", ";
              print_next(d);
              std::cout << ":";        // key:value
              print_next(d);
          }
          std::cout << "}";
          d >> proton::finish();
          break;
      }
      case proton::DESCRIBED: {
          d >> s;
          std::cout << "described(";
          print_next(d);      // Descriptor
          print_next(d);      // value
          d >> proton::finish();
          break;
      }
      default:
        // A simple type. We could continue the switch for all AMQP types but
        // we will take a short cut and extract to another value and print that.
        proton::value v2;
        d >> v2;
        std::cout << type << "(" << v2 << ")";
    }
}

// Print a value, for example purposes. Normal code can use operator<<
void print(proton::value& v) {
    proton::decoder d = v.decoder();
    d.rewind();
    while (d.more()) {
        print_next(d);
        if (d.more()) std::cout << ", ";
    }
    std::cout << std::endl;
}
