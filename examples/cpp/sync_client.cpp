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
#include "proton/sync_request_response.hpp"
#include "proton/url.hpp"
#include "proton/value.hpp"
#include "proton/types.hpp"

#include <iostream>
#include <string>

int main(int argc, char **argv) {
    // Command line options
    proton::url url("127.0.0.1:5672/examples");
    uint64_t timeout(5000);
    options opts(argc, argv);
    opts.add_value(url, 'a', "address", "connect to URL", "URL");
    opts.add_value(timeout, 't', "timeout", "give up after this TIMEOUT (milliseconds)", "TIMEOUT");

    std::string requests[] = { "Twas brillig, and the slithy toves",
                               "Did gire and gymble in the wabe.",
                               "All mimsy were the borogroves,",
                               "And the mome raths outgrabe." };
    int requests_size=4;

    try {
        opts.parse();
        proton::duration d(timeout);
        proton::blocking_connection conn(url, d);
        proton::sync_request_response client(conn, url.path());
        for (int i=0; i<requests_size; i++) {
            proton::message request;
            request.body(requests[i]);
            proton::message response = client.call(request);
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
