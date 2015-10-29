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

using namespace std;

// Examples of how to use the encoder and decoder to create and examine AMQP values.
//

// Print is defined at the end as an example of how to query and extract complex
// values from a decoder in terms of their simple components.
void print(proton::value&);

// Some helper templates to print map and vector results.
namespace std {
template<class T, class U> ostream& operator<<(ostream& o, const pair<T,U>& p) {
    return o << p.first << ":" << p.second;
}
template<class T> ostream& operator<<(ostream& o, const vector<T>& v) {
    o << "[ ";
    ostream_iterator<T> oi(o, " ");
    copy(v.begin(), v.end(), oi);
    return o << "]";
}
template<class K, class T> ostream& operator<<(ostream& o, const map<K, T>& m) {
    o << "{ ";
    ostream_iterator<pair<K,T> > oi(o, " ");
    copy(m.begin(), m.end(), oi);
    return o << "}";
}
}

// Insert/extract C++ containers.
void insert_extract_containers() {
    cout << endl << "== Array, list and map." << endl;

    vector<int> a;
    a.push_back(1);
    a.push_back(2);
    a.push_back(3);
    vector<int> l;
    l.push_back(4);
    l.push_back(5);
    map<string, int> m;
    m["one"] = 1;
    m["two"] = 2;

    proton::value v;
    v.encoder() << proton::as<proton::ARRAY>(a) << proton::as<proton::LIST>(l) << proton::as<proton::MAP>(m);
    print(v);

    vector<int> a1, l1;
    map<string, int> m1;
    v.decoder().rewind();
    v.decoder() >> proton::as<proton::ARRAY>(a1) >> proton::as<proton::LIST>(l1) >> proton::as<proton::MAP>(m1);
    cout << "Extracted: " << a1 << ", " << l1 << ", " << m1 << endl;
}

// Containers with mixed types, use value to represent arbitrary AMQP types.
void mixed_containers() {
    cout << endl << "== List and map of mixed type values." << endl;
    vector<proton::value> l;
    l.push_back(proton::value(42));
    l.push_back(proton::value(proton::amqp_string("foo")));
    map<proton::value, proton::value> m;
    m[proton::value("five")] = proton::value(5);
    m[proton::value(4)] = proton::value("four");
    proton::value v;
    v.encoder() << proton::as<proton::LIST>(l) << proton::as<proton::MAP>(m);
    print(v);

    vector<proton::value> l1;
    map<proton::value, proton::value> m1;
    v.decoder().rewind();
    v.decoder() >> proton::as<proton::LIST>(l1) >> proton::as<proton::MAP>(m1);
    cout << "Extracted: " << l1 << ", " << m1 << endl;
}

// Insert using stream operators (see print_next for example of extracting with stream ops.)
void insert_extract_stream_operators() {
    cout << endl << "== Insert with stream operators." << endl;
    proton::value v;
    // Note: array elements must be encoded with the exact type, they are not
    // automaticlly converted. Mismatched types for array elements will not
    // be detected until v.encode() is called.
    v.encoder() << proton::start::array(proton::INT) << proton::amqp_int(1) << proton::amqp_int(2) << proton::amqp_int(3) << proton::finish();
    print(v);

    v.clear();
    v.encoder() << proton::start::list() << proton::amqp_int(42) << false << proton::amqp_symbol("x") << proton::finish();
    print(v);

    v.clear();
    v.encoder() << proton::start::map() << "k1" << proton::amqp_int(42) << proton::amqp_symbol("k2") << false << proton::finish();
    print(v);
}

int main(int, char**) {
    try {
        insert_extract_containers();
        mixed_containers();
        insert_extract_stream_operators();
        return 0;
    } catch (const exception& e) {
        cerr << endl << "error: " << e.what() << endl;
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
          cout << "array<" << s.element;
          if (s.is_described) {
              cout  << ", descriptor=";
              print_next(d);
          }
          cout << ">[";
          for (size_t i = 0; i < s.size; ++i) {
              if (i) cout << ", ";
              print_next(d);
          }
          cout << "]";
          d >> proton::finish();
          break;
      }
      case proton::LIST: {
          d >> s;
          cout << "list[";
          for (size_t i = 0; i < s.size; ++i) {
              if (i) cout << ", ";
              print_next(d);
          }
          cout << "]";
          d >> proton::finish();
          break;
      }
      case proton::MAP: {
          d >> s;
          cout << "map{";
          for (size_t i = 0; i < s.size/2; ++i) {
              if (i) cout << ", ";
              print_next(d);
              cout << ":";        // key:value
              print_next(d);
          }
          cout << "}";
          d >> proton::finish();
          break;
      }
      case proton::DESCRIBED: {
          d >> s;
          cout << "described(";
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
        cout << type << "(" << v2 << ")";
    }
}

// Print a value, for example purposes. Normal code can use operator<<
void print(proton::value& v) {
    proton::decoder& d = v.decoder();
    d.rewind();
    cout << "Values: ";
    while (d.more()) {
        print_next(d);
        if (d.more()) cout << ", ";
    }
    cout << endl;
}
