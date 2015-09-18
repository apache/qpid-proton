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

#include "options.hpp"

#include "proton/container.hpp"
#include "proton/blocking_connection.hpp"
#include "proton/sync_request_response.hpp"
#include "proton/url.hpp"
#include "proton/types.hpp"

#include <iostream>
#include <vector>
#include <string>


int main(int argc, char **argv) {
    // Command line options
    proton::url url("127.0.0.1:5672/examples");
    uint64_t timeout(5000);
    options opts(argc, argv);
    opts.add_value(url, 'a', "address", "connect to URL", "URL");
    opts.add_value(timeout, 't', "timeout", "give up after this TIMEOUT (milliseconds)", "TIMEOUT");

    std::vector<std::string> requests;
    requests.push_back("Twas brillig, and the slithy toves");
    requests.push_back("Did gire and gymble in the wabe.");
    requests.push_back("All mimsy were the borogroves,");
    requests.push_back("And the mome raths outgrabe.");

    try {
        opts.parse();

        proton::blocking_connection conn(url, proton::duration(timeout));
        proton::sync_request_response client(conn, url.path());
        for (std::vector<std::string>::const_iterator i=requests.begin(); i != requests.end(); i++) {
            proton::message request;
            request.body(*i);
            proton::message response(client.call(request));
            std::cout << request.body() << " => " << response.body() << std::endl;
        }
        return 0;
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
