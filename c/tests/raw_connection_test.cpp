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

#include <proton/raw_connection.h>
#include "proactor/raw_connection-internal.h"

#include "pn_test.hpp"

#ifdef _WIN32
#include <errno.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#endif

#include <string.h>

#include <array>
#include <utility>
#include <string>
#include <vector>

using namespace pn_test;
using Catch::Matchers::Contains;
using Catch::Matchers::Equals;

namespace {
  pn_raw_connection_t* mk_raw_connection() {
    pn_raw_connection_t* rc = (pn_raw_connection_t*) calloc(1, sizeof(struct pn_raw_connection_t));
    pni_raw_initialize(rc);
    return rc;
  }
  void free_raw_connection(pn_raw_connection_t* c) {
    pni_raw_finalize(c);
    free(c);
  }
  int read_err;
  void set_read_error(pn_raw_connection_t*, const char*, int err) {
    read_err = err;
  }
  int write_err;
  void set_write_error(pn_raw_connection_t*, const char*, int err) {
    write_err = err;
  }

  size_t max_send_size = 0;
  size_t max_recv_size = 0;

#if defined(MSG_DONTWAIT) && defined(MSG_NOSIGNAL) && defined(__linux__)
  // This version uses socketpairs and only gets run on Linux
  // It seems that some versions of macOSX define both symbols but don't
  // implement them fully.

  long rcv(int fd, void* b, size_t s) {
    read_err = 0;
    if (max_recv_size && max_recv_size < s) s = max_recv_size;
    return ::recv(fd, b, s, MSG_DONTWAIT);
  }

  void freepair(int fds[2]) {
      ::close(fds[0]);
      ::close(fds[1]);
  }

  void rcv_stop(int fd) {
      ::shutdown(fd, SHUT_RD);
  }

  void snd_stop(int fd) {
      ::shutdown(fd, SHUT_WR);
  }

  long snd(int fd, const void* b, size_t s) {
    write_err = 0;
    if (max_send_size && max_send_size < s) s = max_send_size;
    return ::send(fd, b, s, MSG_NOSIGNAL | MSG_DONTWAIT);
  }

  int makepair(int fds[2]) {
    return ::socketpair(AF_LOCAL, SOCK_STREAM, PF_UNSPEC, fds);
  }
#else
  // Simple mock up of the read/write functions of a socketpair for testing
  // systems without socketpairs or MSG_NOSIGNAL/MSG_DONTWAIT (Windows/BSDs)
  static const uint16_t buffsize = 4096;
  struct fbuf {
    uint8_t buff[buffsize*2] = {};
    int linked_fd = -1;
    size_t head = 0;
    size_t size = 0;
    bool rclosed = true;
    bool wclosed = true;

    bool closed() {
      return rclosed && wclosed && linked_fd == -1;
    }

    void open_linked(int linked_fd0) {
      CHECK(closed());
      CHECK(head == 0);
      CHECK(size == 0);
      linked_fd = linked_fd0;
      rclosed = false;
      wclosed = false;
    }

    void shutdown_rd() {
      rclosed = true;
    }

    void shutdown_wrt() {
      wclosed = true;
    }

    void close() {
      CHECK_FALSE(closed());
      linked_fd = -1;
      rclosed = true;
      wclosed = true;
      head = 0;
      size = 0;
    }
  };

  static std::vector<fbuf> buffers;

  long rcv(int fd, void* b, size_t s){
    CHECK(fd < buffers.size());
    read_err = 0;
    if (max_recv_size && max_recv_size < s) s = max_recv_size;

    fbuf& buffer = buffers[fd];
    if (buffer.size == 0) {
      if (buffer.rclosed) {
        return 0;
      } else {
        errno = EWOULDBLOCK;
        return -1;
      }
    }

    if (buffer.size < s) s = buffer.size;

    ::memcpy(b, &buffer.buff[buffer.head], s);
    buffer.head += s % buffsize;
    buffer.size -= s;
    return s;
  }

  long snd(int fd, const void* b, size_t s){
    CHECK(fd < buffers.size());
    write_err = 0;
    if (max_send_size && max_send_size < s) s = max_send_size;

    // Write to linked buffer
    fbuf& buffer = buffers[fd];
    fbuf& linked_buffer = buffers[buffer.linked_fd];
    if (linked_buffer.size == buffsize) {
      errno = EWOULDBLOCK;
      return -1;
    }
    if (linked_buffer.rclosed) {
      errno = EPIPE;
      return -1;
    }
    if (s + linked_buffer.size > buffsize) s = buffsize - linked_buffer.size;
    ::memcpy(&linked_buffer.buff[linked_buffer.head+linked_buffer.size], b, s);
    // If we wrote into the second half them write again into the hole at the front
    if (linked_buffer.head+linked_buffer.size > buffsize) {
      size_t r = linked_buffer.head+linked_buffer.size - buffsize;
      ::memmove(&linked_buffer.buff[0], &linked_buffer.buff[buffsize], r);
    }
    linked_buffer.size += s;
    return s;
  }

  void rcv_stop(int fd) {
    CHECK(fd < buffers.size());
    buffers[fd].shutdown_rd();
  }

  void snd_stop(int fd) {
    CHECK(fd < buffers.size());
    buffers[fd].shutdown_wrt();
    buffers[buffers[fd].linked_fd].shutdown_rd();
  }

  int makepair(int fds[2]) {
    size_t maximum_fd = buffers.size();
    buffers.resize( buffers.size()+2);
    buffers[maximum_fd].open_linked(maximum_fd+1);
    buffers[maximum_fd+1].open_linked(maximum_fd);
    fds[0] = maximum_fd;
    fds[1] = maximum_fd+1;
    return 0;
  }

  void freepair(int fds[2]) {
    CHECK(fds[0] < buffers.size());
    CHECK(fds[1] < buffers.size());
    buffers[fds[0]].close();
    buffers[fds[1]].close();
  }
#endif

  // Block of memory for buffers
  const size_t BUFFMEMSIZE = 8*1024;
  const size_t RBUFFCOUNT = 32;
  const size_t WBUFFCOUNT = 32;

  char rbuffer_memory[BUFFMEMSIZE];
  char *rbuffer_brk = rbuffer_memory;

  pn_raw_buffer_t rbuffs[RBUFFCOUNT];
  pn_raw_buffer_t wbuffs[WBUFFCOUNT];

  class BufferAllocator {
    char* buffer;
    uint32_t size;
    uint32_t brk;

  public:
    BufferAllocator(char* b, uint32_t s) : buffer(b), size(s), brk(0) {};

    char* next(uint32_t s) {
      if ( brk+s > size) return NULL;

      char *r = buffer+brk;
      brk += s;
      return r;
    }

    template <class B>
    B next_buffer(uint32_t s);

    template <class B, int N>
    void split_buffers(B (&buffers)[N]) {
      uint32_t buffsize  = (size-brk)/N;
      uint32_t remainder = (size-brk)%N;
      for (int i = 0; i<N; ++i) {
        buffers[i] = next_buffer<B>(i==0 ? buffsize+remainder : buffsize);
      }
    }
  };

  template <>
  pn_raw_buffer_t BufferAllocator::next_buffer(uint32_t s) {
    pn_raw_buffer_t b = {};
    b.bytes = next(s);
    if (b.bytes) {b.capacity = s; b.size = s;}
    return b;
  }
}

char message[] =
"Jabberwocky\n"
"By Lewis Carroll\n"
"\n"
"'Twas brillig, and the slithy toves\n"
"Did gyre and gimble in the wabe:\n"
"All mimsy were the borogroves,\n"
"And the mome raths outgrabe.\n"
"\n"
"Beware the Jabberwock, my son!\n"
"The jaws that bite, the claws that catch!\n"
"Beware the Jubjub bird, and shun\n"
"The frumious Bandersnatch!\n"
"\n"
"He took his vorpal sword in hand;\n"
"Long time the manxome foe he sought-\n"
"So rested he by the Tumtum tree\n"
"And stood awhile in thought.\n"
"\n"
"And, as in uffish thought he stood,\n"
"The Jabberwock with eyes of flame,\n"
"Came whiffling through the tulgey wood,\n"
"And burbled as it came!\n"
"\n"
"One, two! One, two! And through and through,\n"
"The vorpal blade went snicker-snack!\n"
"He left it dead, and with its head\n"
"He went galumphing back.\n"
"\n"
"\"And hast thou slain the JabberWock?\n"
"Come to my arms, my beamish boy!\n"
"O frabjous day! Callooh! Callay!\"\n"
"He chortled in his joy.\n"
"\n"
"'Twas brillig, and the slithy toves\n"
"Did gyre and gimble in the wabe:\n"
"All mimsy were the borogroves,\n"
"And the mome raths outgrabe.\n"
;

TEST_CASE("raw connection refused") {
  auto_free<pn_raw_connection_t, free_raw_connection> p(mk_raw_connection());

  REQUIRE(p);
  REQUIRE(pni_raw_validate(p));
  CHECK_FALSE(pn_raw_connection_is_read_closed(p));
  CHECK_FALSE(pn_raw_connection_is_write_closed(p));

  size_t rbuff_count = pn_raw_connection_read_buffers_capacity(p);
  CHECK(rbuff_count>0);
  size_t wbuff_count = pn_raw_connection_write_buffers_capacity(p);
  CHECK(wbuff_count>0);

  // Simulate connection refused
  pni_raw_connect_failed(p);

  REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_DISCONNECTED);
  REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
}

TEST_CASE("raw connection") {
  auto_free<pn_raw_connection_t, free_raw_connection> p(mk_raw_connection());
  max_send_size = 0;

  REQUIRE(p);
  REQUIRE(pni_raw_validate(p));
  CHECK_FALSE(pn_raw_connection_is_read_closed(p));
  CHECK_FALSE(pn_raw_connection_is_write_closed(p));

  size_t rbuff_count = pn_raw_connection_read_buffers_capacity(p);
  CHECK(rbuff_count>0);
  size_t wbuff_count = pn_raw_connection_write_buffers_capacity(p);
  CHECK(wbuff_count>0);

  BufferAllocator rb(rbuffer_memory, sizeof(rbuffer_memory));
  BufferAllocator wb(message, sizeof(message));

  rb.split_buffers(rbuffs);
  wb.split_buffers(wbuffs);

  size_t rtaken = pn_raw_connection_give_read_buffers(p, rbuffs, RBUFFCOUNT);
  REQUIRE(pni_raw_validate(p));
  REQUIRE(rtaken==rbuff_count);

  SECTION("Write multiple per event loop") {
    size_t wtaken = 0;
    for (size_t i = 0; i < WBUFFCOUNT; ++i) {
      int taken = pn_raw_connection_write_buffers(p, &wbuffs[i], 1);
      if (taken==0) break;
      REQUIRE(pni_raw_validate(p));
      REQUIRE(taken==1);
      wtaken += taken;
    }

    CHECK(pn_raw_connection_read_buffers_capacity(p) == 0);
    CHECK(pn_raw_connection_write_buffers_capacity(p) == 0);

    std::vector<pn_raw_buffer_t> read(rtaken);
    std::vector<pn_raw_buffer_t> written(wtaken);

    SECTION("Simple tests using a looped back socketpair") {
      int fds[2];
      REQUIRE(makepair(fds) == 0);
      pni_raw_connected(p);

      // First event is always connected
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_CONNECTED);
      // No need buffers event as we already gave buffers
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);

      SECTION("Write then read") {
        pni_raw_write(p, fds[0], snd, set_write_error);
        CHECK(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        CHECK(pn_raw_connection_write_buffers_capacity(p) == 0);
        size_t wgiven = pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        CHECK(wgiven==wtaken);

        // Write more
        for (size_t i = wtaken; i < WBUFFCOUNT; ++i) {
          int taken = pn_raw_connection_write_buffers(p, &wbuffs[i], 1);
          if (taken==0) break;
          REQUIRE(pni_raw_validate(p));
          CHECK(taken==1);
          wtaken += taken;
        }

        pni_raw_write(p, fds[0], snd, set_write_error);
        CHECK(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_NEED_WRITE_BUFFERS);
        wgiven += pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        CHECK(wgiven==wtaken);

        // At this point we've written every buffer
        CHECK(pn_raw_connection_write_buffers_capacity(p) == wbuff_count);

        pni_raw_read(p, fds[1], rcv, set_read_error);
        CHECK(read_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        CHECK(pn_raw_connection_read_buffers_capacity(p) == 0);
        size_t rgiven = pn_raw_connection_take_read_buffers(p, &read[0], read.size());
        REQUIRE(pni_raw_validate(p));
        CHECK(rgiven > 0);

        CHECK(pn_raw_connection_read_buffers_capacity(p) == rgiven);

        // At this point we should have read everything - make sure it matches
        char* start = message;
        for (size_t i = 0; i < rgiven; ++i) {
          CHECK(read[i].size > 0);
          CHECK(std::string(read[i].bytes, read[i].size) == std::string(start, read[i].size));
          start += read[i].size;
        }
        REQUIRE(start-message == sizeof(message));
      }
      SECTION("Write then read, short writes") {
        max_send_size = 10;
        size_t wgiven = 0;
        do {
          do {
            pni_raw_write(p, fds[0], snd, set_write_error);
            CHECK(write_err == 0);
            REQUIRE(pni_raw_validate(p));
          } while (pn_event_type(pni_raw_event_next(p)) != PN_RAW_CONNECTION_WRITTEN);
          CHECK(pn_raw_connection_write_buffers_capacity(p) == wgiven);
          size_t given = pn_raw_connection_take_written_buffers(p, &written[wgiven], written.size()-wgiven);
          REQUIRE(pni_raw_validate(p));
          CHECK(given == 1);
          CHECK(written[wgiven].offset == wbuffs[wgiven].offset);
          CHECK(written[wgiven].size == wbuffs[wgiven].size);
          wgiven += given;
        } while (wgiven != wtaken);

        // Write more
        for (size_t i = wtaken; i < WBUFFCOUNT; ++i) {
          int taken = pn_raw_connection_write_buffers(p, &wbuffs[i], 1);
          if (taken==0) break;
          REQUIRE(pni_raw_validate(p));
          CHECK(taken==1);
          wtaken += taken;
        }

        do {
          do {
            pni_raw_write(p, fds[0], snd, set_write_error);
            CHECK(write_err == 0);
            REQUIRE(pni_raw_validate(p));
          } while (pn_event_type(pni_raw_event_next(p)) != PN_RAW_CONNECTION_WRITTEN);
        } while (pn_event_type(pni_raw_event_next(p)) != PN_RAW_CONNECTION_NEED_WRITE_BUFFERS);
        wgiven += pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        CHECK(wgiven==wtaken);

        // At this point we've written every buffer
        CHECK(pn_raw_connection_write_buffers_capacity(p) == wbuff_count);

        pni_raw_read(p, fds[1], rcv, set_read_error);
        CHECK(read_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        CHECK(pn_raw_connection_read_buffers_capacity(p) == 0);
        size_t rgiven = pn_raw_connection_take_read_buffers(p, &read[0], read.size());
        REQUIRE(pni_raw_validate(p));
        CHECK(rgiven > 0);

        CHECK(pn_raw_connection_read_buffers_capacity(p) == rgiven);

        // At this point we should have read everything - make sure it matches
        char* start = message;
        for (size_t i = 0; i < rgiven; ++i) {
          CHECK(read[i].size > 0);
          CHECK(std::string(read[i].bytes, read[i].size) == std::string(start, read[i].size));
          start += read[i].size;
        }
        REQUIRE(start-message == sizeof(message));
      }
      freepair(fds);
    }
  }

  SECTION("Write once per event loop") {
    size_t wtaken = pn_raw_connection_write_buffers(p, wbuffs, WBUFFCOUNT);
    REQUIRE(pni_raw_validate(p));
    CHECK(wtaken==wbuff_count);

    CHECK(pn_raw_connection_read_buffers_capacity(p) == 0);
    CHECK(pn_raw_connection_write_buffers_capacity(p) == 0);

    std::vector<pn_raw_buffer_t> read(rtaken);
    std::vector<pn_raw_buffer_t> written(wtaken);

    SECTION("Check no change in buffer use without read/write") {

      size_t rgiven = pn_raw_connection_take_read_buffers(p, &read[0], rtaken);
      REQUIRE(pni_raw_validate(p));
      CHECK(rgiven==0);
      size_t wgiven = pn_raw_connection_take_written_buffers(p, &written[0], wtaken);
      REQUIRE(pni_raw_validate(p));
      CHECK(wgiven==0);
    }

    SECTION("Close, ensure writes drained correctly") {
      int fds[2];
      REQUIRE(makepair(fds) == 0);
      pni_raw_connected(p);

      // First event is always connected
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_CONNECTED);
      // No need buffers event as we already gave buffers
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);

      pni_raw_write_close(p);

      size_t rgiven = pn_raw_connection_take_read_buffers(p, &read[0], rtaken);
      REQUIRE(pni_raw_validate(p));
      CHECK(rgiven==0);
      size_t wgiven = pn_raw_connection_take_written_buffers(p, &written[0], wtaken);
      REQUIRE(pni_raw_validate(p));
      CHECK(wgiven==0);

      REQUIRE(pn_raw_connection_is_write_closed(p));

      REQUIRE(pni_raw_can_write(p));
      pni_raw_write(p, fds[0], snd, set_write_error);
      REQUIRE(pni_raw_validate(p));
      CHECK(write_err == 0);

      REQUIRE_FALSE(pni_raw_can_write(p));

      REQUIRE(pni_raw_can_read(p));
      pni_raw_read(p, fds[0], rcv, set_read_error);
      REQUIRE(pni_raw_validate(p));
      CHECK(read_err == 0);

      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);

      SECTION("Read close after write close") {
        pni_raw_read_close(p);
      }

      SECTION("Full close after write close") {
        // We should be able to fully close here (even if we read close would be more specific)
        pni_raw_close(p);
      }

      REQUIRE(pn_raw_connection_is_read_closed(p));
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_CLOSED_READ);

      REQUIRE_FALSE(pni_raw_can_read(p));
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_CLOSED_WRITE);
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_DRAIN_BUFFERS);
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
      rgiven = pn_raw_connection_take_read_buffers(p, &read[0], rtaken);
      REQUIRE(pni_raw_validate(p));
      CHECK(rgiven==rtaken);
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
      wgiven = pn_raw_connection_take_written_buffers(p, &written[0], wtaken);
      REQUIRE(pni_raw_validate(p));
      CHECK(wgiven==wtaken);

      // This should have no affect because we are already read and write closed
      pni_raw_close(p);

      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_DISCONNECTED);
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);

      // Now read other end of socket manually and compare
      size_t sz=0;
      do {
        long i = rcv(fds[1], rbuffer_memory+sz, BUFFMEMSIZE-sz);
        if (i<0) break;
        sz+=i;
      } while (true);

      // concatenate the written buffers
      std::string s;
      for (size_t i = 0; i < wtaken; ++i) {
        s+=std::string(written[i].bytes, written[i].size);
      }
      REQUIRE(sz == s.size());
      REQUIRE(s == std::string(rbuffer_memory, sz));
    }

    SECTION("Simple tests using a looped back socketpair") {
      int fds[2];
      REQUIRE(makepair(fds) == 0);
      pni_raw_connected(p);

      // First event is always connected
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_CONNECTED);
      // No need buffers event as we already gave buffers
      REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);

      SECTION("Ensure nothing is read if nothing is written") {
        pni_raw_read(p, fds[1], rcv, set_read_error);
        CHECK(read_err == 0);
        REQUIRE(pni_raw_validate(p));
        CHECK(pn_raw_connection_read_buffers_capacity(p) == 0);
        CHECK(pn_raw_connection_take_read_buffers(p, &read[0], read.size()) == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pni_raw_event_next(p) == NULL);

        snd_stop(fds[0]);
        pni_raw_read(p, fds[1], rcv, set_read_error);
        CHECK(read_err == 0);
        REQUIRE(pni_raw_validate(p));
        CHECK(pn_raw_connection_is_read_closed(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_CLOSED_READ);
        rcv_stop(fds[1]);
        pni_raw_write(p, fds[0], snd, set_write_error);
        CHECK(write_err == EPIPE);
        REQUIRE(pni_raw_validate(p));
        CHECK(pn_raw_connection_is_write_closed(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_CLOSED_WRITE);
        // TODO: Remove  the inapplicable tests when the drain buffers completely replaces read/written
        SECTION("Ensure get read/written events before disconnect if not drained") {
          REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_DRAIN_BUFFERS);
          REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
          REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        }
        SECTION("Ensure no read/written events before disconnect if drained") {
          REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_DRAIN_BUFFERS);
          while(pn_raw_connection_take_read_buffers(p, &read[0], read.size())>0);
          while(pn_raw_connection_take_written_buffers(p, &written[0], written.size())>0);
        }
        SECTION("Ensure no written events before disconnect if write drained") {
          REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_DRAIN_BUFFERS);
          REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
          while(pn_raw_connection_take_read_buffers(p, &read[0], read.size())>0);
          while(pn_raw_connection_take_written_buffers(p, &written[0], written.size())>0);
        }
        SECTION("Ensure no read events before disconnect if read drained") {
          REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_DRAIN_BUFFERS);
          while(pn_raw_connection_take_read_buffers(p, &read[0], read.size())>0);
          REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
          while(pn_raw_connection_take_written_buffers(p, &written[0], written.size())>0);
        }
        SECTION("Ensure no events before disconnect if already drained") {
          while(pn_raw_connection_take_read_buffers(p, &read[0], read.size())>0);
          while(pn_raw_connection_take_written_buffers(p, &written[0], written.size())>0);
        }
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_DISCONNECTED);

      }

      SECTION("Read/Write interleaved") {
        pni_raw_write(p, fds[0], snd, set_write_error);
        CHECK(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        CHECK(pn_raw_connection_write_buffers_capacity(p) == 0);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        size_t wgiven = pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_NEED_WRITE_BUFFERS);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        CHECK(wgiven==wtaken);
        CHECK(pn_raw_connection_write_buffers_capacity(p) == wbuff_count);

        pni_raw_read(p, fds[1], rcv, set_read_error);
        CHECK(read_err == 0);
        REQUIRE(pni_raw_validate(p));
        CHECK(pn_raw_connection_read_buffers_capacity(p) == 0);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
        size_t rgiven = pn_raw_connection_take_read_buffers(p, &read[0], read.size());
        REQUIRE(pni_raw_validate(p));
        CHECK(rgiven > 0);
        CHECK(pn_raw_connection_read_buffers_capacity(p) == rgiven);

        // Write more
        wtaken += pn_raw_connection_write_buffers(p, &wbuffs[wtaken], WBUFFCOUNT-wtaken);
        REQUIRE(pni_raw_validate(p));
        CHECK(wtaken==WBUFFCOUNT);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);

        pni_raw_write(p, fds[0], snd, set_write_error);
        CHECK(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        wgiven += pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_NEED_WRITE_BUFFERS);
        CHECK(wgiven==wtaken);

        // At this point we've written every buffer

        pni_raw_read(p, fds[1], rcv, set_read_error);
        CHECK(read_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
        size_t rgiven_before = rgiven;
        rgiven += pn_raw_connection_take_read_buffers(p, &read[rgiven], read.size()-rgiven);
        REQUIRE(pni_raw_validate(p));
        CHECK(rgiven > rgiven_before);

        CHECK(pn_raw_connection_read_buffers_capacity(p) == rgiven);
        CHECK(pn_raw_connection_write_buffers_capacity(p) == wbuff_count);

        // At this point we should have read everything - make sure it matches
        char* start = message;
        for (size_t i = 0; i < rgiven; ++i) {
          CHECK(read[i].size > 0);
          CHECK(std::string(read[i].bytes, read[i].size) == std::string(start, read[i].size));
          start += read[i].size;
        }
        REQUIRE(start-message == sizeof(message));
      }

      SECTION("Write then read") {
        pni_raw_write(p, fds[0], snd, set_write_error);
        CHECK(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        CHECK(pn_raw_connection_write_buffers_capacity(p) == 0);
        size_t wgiven = pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        CHECK(wgiven==wtaken);

        // Write more
        wtaken += pn_raw_connection_write_buffers(p, &wbuffs[wtaken], WBUFFCOUNT-wtaken);
        REQUIRE(pni_raw_validate(p));
        CHECK(wtaken==WBUFFCOUNT);

        pni_raw_write(p, fds[0], snd, set_write_error);
        CHECK(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_NEED_WRITE_BUFFERS);
        wgiven += pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        CHECK(wgiven==wtaken);

        // At this point we've written every buffer
        CHECK(pn_raw_connection_write_buffers_capacity(p) == wbuff_count);

        pni_raw_read(p, fds[1], rcv, set_read_error);
        CHECK(read_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        CHECK(pn_raw_connection_read_buffers_capacity(p) == 0);
        size_t rgiven = pn_raw_connection_take_read_buffers(p, &read[0], read.size());
        REQUIRE(pni_raw_validate(p));
        CHECK(rgiven > 0);

        CHECK(pn_raw_connection_read_buffers_capacity(p) == rgiven);

        // At this point we should have read everything - make sure it matches
        char* start = message;
        for (size_t i = 0; i < rgiven; ++i) {
          CHECK(read[i].size > 0);
          CHECK(std::string(read[i].bytes, read[i].size) == std::string(start, read[i].size));
          start += read[i].size;
        }
        REQUIRE(start-message == sizeof(message));
      }

      SECTION("Write, close, then read") {
        pni_raw_write(p, fds[0], snd, set_write_error);
        CHECK(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        CHECK(pn_raw_connection_write_buffers_capacity(p) == 0);
        size_t wgiven = pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        CHECK(wgiven==wtaken);

        // Write more
        wtaken += pn_raw_connection_write_buffers(p, &wbuffs[wtaken], WBUFFCOUNT-wtaken);
        REQUIRE(pni_raw_validate(p));
        CHECK(wtaken==WBUFFCOUNT);

        pni_raw_write(p, fds[0], snd, set_write_error);
        CHECK(write_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_WRITTEN);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_NEED_WRITE_BUFFERS);
        wgiven += pn_raw_connection_take_written_buffers(p, &written[0], written.size());
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        CHECK(wgiven==wtaken);

        // At this point we've written every buffer
        CHECK(pn_raw_connection_write_buffers_capacity(p) == wbuff_count);

        snd_stop(fds[0]);
        pni_raw_read(p, fds[1], rcv, set_read_error);
        CHECK(read_err == 0);
        REQUIRE(pni_raw_validate(p));
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_READ);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_RAW_CONNECTION_CLOSED_READ);
        REQUIRE(pn_event_type(pni_raw_event_next(p)) == PN_EVENT_NONE);
        CHECK(pn_raw_connection_read_buffers_capacity(p) == 0);
        size_t rgiven = pn_raw_connection_take_read_buffers(p, &read[0], read.size());
        REQUIRE(pni_raw_validate(p));
        CHECK(rgiven > 0);

        CHECK(read[rgiven-1].size == 0);

        // At this point we should have read everything - make sure it matches
        char* start = message;
        for (size_t i = 0; i < rgiven-1; ++i) {
          CHECK(read[i].size > 0);
          CHECK(std::string(read[i].bytes, read[i].size) == std::string(start, read[i].size));
          start += read[i].size;
        }
        REQUIRE(start-message == sizeof(message));
      }

      freepair(fds);
    }
  }
}
