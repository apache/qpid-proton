
#include <proton/connection.hpp>
#include <proton/connection_options.hpp>
#include <proton/container.hpp>
#include <proton/delivery.hpp>
#include <proton/link.hpp>
#include <proton/listen_handler.hpp>
#include <proton/listener.hpp>
#include <proton/message.hpp>
#include <proton/message_id.hpp>
#include <proton/messaging_handler.hpp>
#include <proton/tracker.hpp>
#include <proton/types.h>
#include <proton/types.hpp>
#include <proton/transaction_handler.hpp>
#include <proton/value.hpp>
#include <proton/messaging_handler.hpp>

#include "test_bits.hpp"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace {
std::mutex m;
std::condition_variable cv;
bool listener_ready = false;
int listener_port;
} //namespace

class test_recv : public proton::messaging_handler {
  private:
    class listener_ready_handler : public proton::listen_handler {
        void on_open(proton::listener &l) override {
            {
                std::lock_guard<std::mutex> lk(m);
                listener_port = l.port();
                listener_ready = true;
            }
        cv.notify_one();
        }
    };

    std::string url;
    proton::listener listener;
    listener_ready_handler listen_handler;

  public:
    test_recv(const std::string &s) : url(s) {}

    void on_container_start(proton::container &c) override {
        listener = c.listen(url, listen_handler);
    }

    void on_message(proton::delivery &d, proton::message &msg) override {
        
    }
};

class test_send : public proton::messaging_handler, proton::transaction_handler {
  private:
    std::string url;
    proton::sender sender;
    proton::session session;
    int batch_index = 0;
    int current_batch = 0;
    int committed = 0;
    int confirmed = 0;

    int batch_size = 3;
    int total = 6;
  public:
    test_send(const std::string &s) : url(s) {}

    void on_container_start(proton::container &c) override {
        proton::connection_options co;
        sender = c.open_sender(url, co);
    }


    void on_session_open(proton::session &s) override {
        session = s;
        s.declare_transaction(*this);
    }


    void on_transaction_declared(proton::session s) override {
        send(sender);
    }

    void on_sendable(proton::sender &s) override {
        send(s);
    }

    void send(proton::sender &s) {
        static int unique_id = 10000;
        while (session.transaction_is_declared() && sender.credit() &&
               (committed + current_batch) < total) {
            proton::message msg;
            std::map<std::string, int> m;
            m["sequence"] = committed + current_batch;

            msg.id(unique_id++);
            msg.body(m);
            s.send(msg);
            current_batch += 1;
            if(current_batch == batch_size)
            {
                std::cout << " >> Txn attempt commit" << std::endl;
                if (batch_index % 2 == 0) {
                    session.transaction_commit();
                } else {
                    session.transaction_abort();
                }       
                batch_index++;
            }
        }
    }
};

int test_transaction() {
    
    std::string recv_address("127.0.0.1:0/test");
    test_recv recv(recv_address);
    proton::container c(recv);

    std::thread thread_recv([&c]() -> void { c.run(); });

    // wait until listener is ready
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return listener_ready; });

    std::string send_address =
        "127.0.0.1:" + std::to_string(listener_port) + "/test";
    test_send send(send_address);
    proton::container(send).run();
    thread_recv.join();

    return 0;
}

int main(int argc, char **argv) {
    int failed = 0;
    RUN_ARGV_TEST(failed, test_transaction());
    return failed;
}
