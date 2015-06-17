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

#include <proton/Values.hpp>
#include <proton/Value.hpp>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace std;
using namespace proton;

// Examples of how to use the Encoder and Decoder to create and examine AMQP values.
//

// Print is defined at the end as an example of how to query and extract complex
// values in terms of their simple components.
void print(Values& values);

// Inserting and extracting simple C++ values.
void simple_insert_extract() {
    Values values;
    cout << endl << "== Simple values: int, string, bool" << endl;
    values << 42 << "foo" << true;
    print(values);
    int i;
    std::string s;
    bool b;
    values.rewind();
    values >> i >> s >> b;
    cout << "Extracted: " << i << ", " << s << ", " << b << endl;
    // Encode and decode as AMQP
    string amqpData = values.encode();
    cout << "Encoded as AMQP in " << amqpData.size() << " bytes" << endl;
    Values values2;
    values.decode(amqpData);
    values >> i >> s >> b;
    cout << "Decoded: " << i << ", " << s << ", " << b << endl;
}

// Inserting values as a specific AMQP type
void simple_insert_extract_exact_type() {
    Values values;
    cout << endl << "== Specific AMQP types: byte, long, symbol" << endl;
    values << Byte('x') << Long(123456789123456789) << Symbol("bar");
    print(values);
    values.rewind();
    // Check that we encoded the correct types, but note that decoding will
    // still convert to standard C++ types, in particular any AMQP integer type
    // can be converted to a long-enough C++ integer type..
    std::int64_t i1, i2;
    std::string s;
    values >> i1 >> i2 >> s;
    cout << "Extracted (with conversion) " << i1 << ", " << i2 << ", " << s << endl;

    // Now use the as() function to fail unless we extract the exact AMQP type expected.
    values.rewind();            // Byte(1) << Long(2) << Symbol("bar");
    Long l;
    // Fails, extracting Byte as Long
    try { values >> as<LONG>(l); throw logic_error("expected error"); } catch (DecodeError) {}
    Byte b;
    values >> as<BYTE>(b) >> as<LONG>(l); // OK, extract Byte as Byte, Long as Long.
    std::string str;
    // Fails, extracting Symbol as String.
    try { values >> as<STRING>(str); throw logic_error("expected error"); } catch (DecodeError) {}
    values >> as<SYMBOL>(str);       // OK, extract Symbol as Symbol
    cout << "Extracted (exact) " << b << ", " << l << ", " << str << endl;
}

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

    Values values;
    values << as<ARRAY>(a) << as<LIST>(l) << as<MAP>(m);
    print(values);

    vector<int> a1, l1;
    map<string, int> m1;
    values.rewind();
    values >> as<ARRAY>(a1) >> as<LIST>(l1) >> as<MAP>(m1);
    cout << "Extracted: " << a1 << ", " << l1 << ", " << m1 << endl;
}

// Containers with mixed types, use Value to represent arbitrary AMQP types.
void mixed_containers() {
    cout << endl << "== List and map of mixed type values." << endl;
    vector<Value> l;
    l.push_back(Value(42));
    l.push_back(Value(String("foo")));
    map<Value, Value> m;
    m[Value("five")] = Value(5);
    m[Value(4)] = Value("four");
    Values values;
    values << as<LIST>(l) << as<MAP>(m);
    print(values);

    vector<Value> l1;
    map<Value, Value> m1;
    values.rewind();
    values >> as<LIST>(l1) >> as<MAP>(m1);
    cout << "Extracted: " << l1 << ", " << m1 << endl;
}

// Insert using stream operators (see printNext for example of extracting with stream ops.)
void insert_extract_stream_operators() {
    cout << endl << "== Insert with stream operators." << endl;
    Values values;
    // Note: array elements must be encoded with the exact type, they are not
    // automaticlly converted. Mismatched types for array elements will not
    // be detected until values.encode() is called.
    values << Start::array(INT) << Int(1) << Int(2) << Int(3) << finish();
    print(values);

    values.clear();
    values << Start::list() << Int(42) << false << Symbol("x") << finish();
    print(values);

    values.clear();
    values << Start::map() << "k1" << Int(42) << Symbol("k2") << false << finish();
    print(values);
}

int main(int, char**) {
    try {
        simple_insert_extract();
        simple_insert_extract_exact_type();
        insert_extract_containers();
        mixed_containers();
        insert_extract_stream_operators();
    } catch (const exception& e) {
        cerr << endl << "error: " << e.what() << endl;
        return 1;
    }
}

// printNext prints the next value from Values by recursively descending into complex values.
//
// NOTE this is for example puroses only: There is a built in ostream operator<< for Values.
//
//
void printNext(Values& values) {
    TypeId type = values.type();
    Start start;
    switch (type) {
      case ARRAY: {
          values >> start;
          cout << "array<" << start.element;
          if (start.isDescribed) {
              cout  << ", descriptor=";
              printNext(values);
          }
          cout << ">[";
          for (size_t i = 0; i < start.size; ++i) {
              if (i) cout << ", ";
              printNext(values);
          }
          cout << "]";
          values >> finish();
          break;
      }
      case LIST: {
          values >> start;
          cout << "list[";
          for (size_t i = 0; i < start.size; ++i) {
              if (i) cout << ", ";
              printNext(values);
          }
          cout << "]";
          values >> finish();
          break;
      }
      case MAP: {
          values >> start;
          cout << "map{";
          for (size_t i = 0; i < start.size/2; ++i) {
              if (i) cout << ", ";
              printNext(values);
              cout << ":";        // key:value
              printNext(values);
          }
          cout << "}";
          values >> finish();
          break;
      }
      case DESCRIBED: {
          values >> start;
          cout << "described(";
          printNext(values);      // Descriptor
          printNext(values);      // Value
          values >> finish();
          break;
      }
      default:
        // A simple type. We could continue the switch for all AMQP types but
        // instead we us the `Value` type which can hold and print any AMQP
        // value.
        Value v;
        values >> v;
        cout << type << "(" << v << ")";
    }
}

// Print all the values with printNext
void print(Values& values) {
    values.rewind();
    cout << "Values: ";
    while (values.more()) {
        printNext(values);
        if (values.more()) cout << ", ";
    }
    cout << endl;
}
