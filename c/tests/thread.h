#ifndef TESTS_THREAD_H
#define TESTS_THREAD_H

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
 * software distributed under the License is distributed on an * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/* Simulate a subset of POSIX threads for windows */

#ifdef _WIN32

#include <windows.h>
#include <process.h>

typedef struct {
  HANDLE handle;
  void *(*func)(void *);
  void *arg;
} pthread_t;

static unsigned __stdcall pthread_run(void *thr0) {
  pthread_t *t = (pthread_t *) thr0;
  t->func(t->arg);
  return 0;
}

static int pthread_create(pthread_t *t, void *unused, void *(*f)(void *), void *arg) {
  t->func = f;
  t->arg = arg;
  t->handle =  (HANDLE) _beginthreadex(0, 0, &pthread_run, t, 0, 0);
  if (t->handle) {
    return 0;
  }
  return -1;
}

static int pthread_join(pthread_t t, void **unused) {
  if (t.handle) {
    WaitForSingleObject(t.handle, INFINITE);
    CloseHandle(t.handle);
  }
  return 0;
}

typedef CRITICAL_SECTION pthread_mutex_t;
#define pthread_mutex_init(m, unused) InitializeCriticalSectionAndSpinCount(m, 4000)
#define pthread_mutex_destroy(m) DeleteCriticalSection(m)
#define pthread_mutex_lock(m) EnterCriticalSection(m)
#define pthread_mutex_unlock(m) LeaveCriticalSection(m)

void millisleep(long ms) { Sleep(ms); }

#else  /* POSIX */

#include <pthread.h>
#include <unistd.h>             /* For sleep() */

void millisleep(long ms) {
  struct timespec delay = {0};
  delay.tv_sec  = ms / 1000;
  delay.tv_nsec = (ms % 1000) * 1000000;
  nanosleep(&delay, NULL);
}

#endif

#endif  /*!TESTS_THREAD_H*/
