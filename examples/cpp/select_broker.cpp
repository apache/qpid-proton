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

#include "options.hpp"
#include "broker.hpp"

#include "proton/engine.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

template <class T> T check(T result, const std::string& msg=std::string()) {
     // Note strerror is thread unsafe, this example is single-threaded.
    if (result < 0) throw std::runtime_error(msg + ": " + strerror(errno));
    return result;
}

static void fd_set_if(bool on, int fd, fd_set *fds) {
    if (on)
        FD_SET(fd, fds);
    else
        FD_CLR(fd, fds);
}

class broker {

    typedef std::map<int, proton::engine*> engine_map;

    queues queues_;
    broker_handler handler_;
    engine_map engines_;
    fd_set reading_, writing_;
    int listen_;

  public:
    broker() : handler_(queues_) {
        FD_ZERO(&reading_);
        FD_ZERO(&writing_);
    }

    ~broker() {
        for (engine_map::iterator i = engines_.begin(); i != engines_.end(); ++i)
            delete i->second;
    }

    void run(uint16_t port) {

        listen(port);

        while(true) {
            fd_set readable_set = reading_;
            fd_set writable_set = writing_;

            check(::select(FD_SETSIZE, &readable_set, &writable_set, NULL, NULL), "select");
            for (int fd = 0; fd < FD_SETSIZE; ++fd) {
                if (fd == listen_ && FD_ISSET(fd, &readable_set)) 
                    accept();

                if (engines_.find(fd) != engines_.end()) {
                    proton::engine& eng = *engines_[fd];
                    try {
                        if (FD_ISSET(fd, &readable_set))
                            readable(fd, eng);

                        if (FD_ISSET(fd, &writable_set))
                            writable(fd, eng);
                    } catch (const std::exception& e) {
                        std::cout << e.what() << " fd=" << fd << std::endl;
                        eng.close_input();
                        eng.close_output();
                    }
                    // Set reading/writing bits for next time around
                    fd_set_if(eng.input().size(), fd, &reading_);
                    fd_set_if(eng.output().size(), fd, &writing_);

                    if (eng.closed()) {
                        ::close(fd);
                        delete engines_[fd];
                        engines_.erase(fd);
                    }
                }
            }
        }
    }

  private:

    void listen(uint16_t port) {
        listen_ = check(::socket(PF_INET, SOCK_STREAM, 0), "create listener");
        int yes = 1;
        check(::setsockopt(listen_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)), "setsockopt");
        struct sockaddr_in name;
        name.sin_family = AF_INET;
        name.sin_port = htons (port);
        name.sin_addr.s_addr = htonl (INADDR_ANY);
        check(::bind(listen_, (struct sockaddr *)&name, sizeof(name)), "bind listener");
        check(::listen(listen_, 32), "listen");
        std::cout << "listening on port " << port << " fd=" << listen_ << std::endl;
        FD_SET(listen_, &reading_);
    }

    void accept() {
        struct sockaddr_in client_addr;
        socklen_t size = sizeof(client_addr);
        int fd = check(::accept(listen_, (struct sockaddr *)&client_addr, &size), "accept");
        engines_[fd] = new proton::engine(handler_);
        FD_SET(fd, &reading_);
        FD_SET(fd, &writing_);
        std::cout << "accept " << ::inet_ntoa(client_addr.sin_addr)
                  << ":" << ntohs(client_addr.sin_port)
                  << " fd=" << fd << std::endl;
    }

    void readable(int fd, proton::engine& eng) {
        proton::buffer<char> input = eng.input();
        if (input.size()) {
            ssize_t n = check(::read(fd, input.begin(), input.size()));
            if (n > 0) {
                eng.received(n);
            } else {
                eng.close_input();
            }
        }
    }

    void writable(int fd, proton::engine& eng) {
        proton::buffer<const char> output = eng.output();
        if (output.size()) {
            ssize_t n = check(::write(fd, output.begin(), output.size()));
            if (n > 0)
                eng.sent(n);
            else {
                eng.close_output();
            }
        }
    }

};

int main(int argc, char **argv) {
    // Command line options
    proton::url url("0.0.0.0");
    options opts(argc, argv);
    opts.add_value(url, 'a', "address", "listen on URL", "URL");
    try {
        opts.parse();
        broker().run(url.port_int());
        return 0;
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}


