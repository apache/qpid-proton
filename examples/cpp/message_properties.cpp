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

#include <proton/message.hpp>
#include <proton/types.hpp>
#include <iostream>
#include <map>

int main(int argc, char **argv) {
    try {
        proton::message m;

        // Setting properties: legal types are converted automatically to their
        // AMQP counterpart.
        m.properties().put("short", int16_t(123));
        m.properties().put("string", "foo");
        m.properties().put("symbol", proton::symbol("sym"));

        // Examining properties using proton::get()

        // 1 argument get<>() template specifies expected type of property.
        std::string s = proton::get<std::string>(m.properties().get("string"));

        // 2 argument get, property must have matching type to output argument.
        int16_t i;
        proton::get(m.properties().get("short"), i);

        // Checking property types
        proton::type_id type = m.properties().get("symbol").type();
        if (type != proton::SYMBOL) {
            throw std::logic_error("wrong type!");
        }

        // proton::scalar has its own ostream <<
        std::cout << "using put/get:"
                  << " short=" << i
                  << " string=" << s
                  << " symbol=" << m.properties().get("symbol")
                  << std::endl;

        // Converting properties to a convertible type
        std::cout << "using coerce:"
                  << " short(as long)="
                  << proton::coerce<long>(m.properties().get("short"))
                  << std::endl;

        // Extract the properties as a std::map for more complex map operations.
        // You can use other map and sequence types to hold a map, see @ref types_page
        typedef std::map<std::string, proton::scalar> property_map;
        property_map props;
        proton::get(m.properties(), props);
        for (property_map::iterator i = props.begin(); i != props.end(); ++i) {
            std::cout << "props[" << i->first << "]=" << i->second << std::endl;
        }
        props["string"] = "bar";
        props["short"] = 42;
        // Update the properties in the message from the modified props map
        m.properties() = props;

        std::cout << "short=" << m.properties().get("short")
                  << " string=" << m.properties().get("string")
                  << std::endl;

        // proton::get throws an exception if types do not match exactly.
        try {
            proton::get<uint32_t>(m.properties().get("short")); // bad: uint32_t != int16_t
            throw std::logic_error("expected exception");
        } catch (const proton::conversion_error& e) {
            std::cout << "expected conversion_error: \"" << e.what() << '"' << std::endl;
        }

        // proton::coerce throws an exception if types are not convertible.
        try {
            proton::get<uint32_t>(m.properties().get("string"));  // bad: string to uint32_t
            throw std::logic_error("expected exception");
        } catch (const proton::conversion_error& e) {
            std::cout << "expected conversion_error: \"" << e.what() << '"' << std::endl;
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "unexpected exception: " << e.what() << std::endl;
        return 1;
    }
}
