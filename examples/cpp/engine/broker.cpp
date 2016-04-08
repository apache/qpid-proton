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

#include "../options.hpp"
#include "../broker.hpp"

#include <iostream>

#ifndef WIN32                   // TODO aconway 2016-03-23: windows broker example
#include <proton/io/socket.hpp>
#include <sys/select.h>
#include <set>

template <class T> T check(T result, const std::string& msg="io_error: ") {
    if (result < 0)
        throw proton::io::socket::io_error(msg + proton::io::socket::error_str());
    return result;
}

void fd_set_if(bool on, int fd, fd_set *fds);

class broker {
    typedef std::set<proton::io::socket::engine*> engines;

    queues queues_;
    broker_handler handler_;
    proton::io::connection_engine::container container_;
    engines engines_;
    fd_set reading_, writing_;

  public:
    broker() : handler_(queues_) {
        FD_ZERO(&reading_);
        FD_ZERO(&writing_);
    }

    ~broker() {
        for (engines::iterator i = engines_.begin(); i != engines_.end(); ++i)
            delete *i;
    }

    void run(const proton::url& url) {
        proton::io::socket::listener listener(url.host(), url.port());
        std::cout << "listening on " << url << " fd=" << listener.socket() << std::endl;
        FD_SET(listener.socket(), &reading_);
        while(true) {
            fd_set readable_set = reading_;
            fd_set writable_set = writing_;
            check(select(FD_SETSIZE, &readable_set, &writable_set, NULL, NULL), "select");

            if (FD_ISSET(listener.socket(), &readable_set)) {
                std::string client_host, client_port;
                int fd = listener.accept(client_host, client_port);
                std::cout << "accepted " << client_host << ":" << client_port
                          << " fd=" << fd << std::endl;
                engines_.insert(
                    new proton::io::socket::engine(
                        fd, handler_, container_.make_options()));
                FD_SET(fd, &reading_);
                FD_SET(fd, &writing_);
            }

            for (engines::iterator i = engines_.begin(); i != engines_.end(); ) {
                proton::io::socket::engine *eng = *(i++);
                int flags = 0;
                if (FD_ISSET(eng->socket(), &writable_set))
                    eng->write();
                if (FD_ISSET(eng->socket(), &readable_set))
                    eng->read();
                if (eng->dispatch()) {
                    fd_set_if(eng->read_buffer().size, eng->socket(), &reading_);
                    fd_set_if(eng->write_buffer().size, eng->socket(), &writing_);
                } else {
                    std::cout << "closed fd=" << eng->socket() << std::endl;
                    engines_.erase(eng);
                    delete eng;
                }
            }
        }
    }
};

void fd_set_if(bool on, int fd, fd_set *fds) {
    if (on)
        FD_SET(fd, fds);
    else
        FD_CLR(fd, fds);
}

int main(int argc, char **argv) {
    // Command line options
    std::string address("0.0.0.0");
    options opts(argc, argv);
    opts.add_value(address, 'a', "address", "listen on URL", "URL");
    try {
        opts.parse();
        broker().run(address);
        return 0;
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}
#else // WIN32

#include "proton/acceptor.hpp"
#include "proton/container.hpp"
#include "proton/value.hpp"

#include "../fake_cpp11.hpp"

class broker {
  public:
    broker(const proton::url& url) : handler_(url, queues_) {}

    proton::handler& handler() { return handler_; }

  private:

    class my_handler : public broker_handler {
      public:
        my_handler(const proton::url& u, queues& qs) : broker_handler(qs), url_(u) {}

        void on_container_start(proton::container &c) override {
            c.listen(url_);
            std::cout << "broker listening on " << url_ << std::endl;
        }

      private:
        const proton::url& url_;
    };

  private:
    queues queues_;
    my_handler handler_;
};

int main(int argc, char **argv) {
    // Command line options
    proton::url url("0.0.0.0");
    options opts(argc, argv);
    opts.add_value(url, 'a', "address", "listen on URL", "URL");
    try {
        opts.parse();
        broker b(url);
        proton::container(b.handler()).run();
        return 0;
    } catch (const bad_option& e) {
        std::cout << opts << std::endl << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 1;
}

#endif
