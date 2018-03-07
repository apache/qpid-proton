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

/* This test is intended to be run under a race detector (e.g. helgrind,
   libtsan) to uncover races by generating concurrent, pseudo-random activity.
   It should also be run under memory checkers (e.g. valgrind memcheck, libasan)
   for race conditions that leak memory.

   The goal is to drive concurrent activity as hard as possible while staying
   within the rules of the API. It may do things that an application is unlikely
   to do (e.g. close a listener before it opens) but that are not disallowed by
   the API rules - that's the goal.

   It is impossible to get repeatable behaviour with multiple threads due to
   unpredictable scheduling. Currently using plain old rand(), if quality of
   randomness is problem we can upgrade.

   TODO
   - closing connections
   - pn_proactor_release_connection and re-use with pn_proactor_connect/accept
   - sending/receiving/tracking messages
   - cancel timeout
*/

#include "thread.h"

#include <proton/connection.h>
#include <proton/event.h>
#include <proton/listener.h>
#include <proton/netaddr.h>
#include <proton/proactor.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#undef NDEBUG                   /* Enable assert even in release builds */
#include <assert.h>

#define BACKLOG 16              /* Listener backlog */
#define TIMEOUT_MAX 100         /* Milliseconds */
#define SLEEP_MAX 100           /* Milliseconds */

/* Set of actions that can be enabled/disabled/counted */
typedef enum { A_LISTEN, A_LCLOSE, A_CONNECT, A_CCLOSE, A_WAKE, A_TIMEOUT } action;
const char* action_name[] = { "listen", "lclose", "connect", "cclose", "wake", "timeout" };
#define action_size (sizeof(action_name)/sizeof(*action_name))
bool action_enabled[action_size] = { 0 } ;

static int assert_no_err(int n) { assert(n >= 0); return n; }

static bool debug_enable = false;

/*
  NOTE: Printing to a single debug log from many threads creates artificial
  before/after relationships that may mask race conditions (locks in fputs)

  TODO: something better, e.g. per-thread timestamped logs that are only
  collated and printed periodically or at end of process.
*/
#define debug(...) if (debug_enable) debug_impl(__VA_ARGS__)

static void debug_impl(const char *fmt, ...) {
  /* Collect thread-id and message in a single buffer to minimize mixed messages */
  char msg[256];
  char *i = msg;
  char *end = i + sizeof(msg);
  i += assert_no_err(snprintf(i, end-i, "(%lx) ", pthread_self()));
  if (i < end) {
    va_list ap;
    va_start(ap, fmt);
    i += assert_no_err(vsnprintf(i, end-i, fmt, ap));
    va_end(ap);
  }
  fputs(msg, stderr);
}

/* Shorthand for debugging an action using id as identifier */
static inline void debuga(action a, void *id) {
  debug("[%p] %s", id, action_name[a]);
}

/* Thread safe pools of structs.

   An element can be "picked" at random concurrently by many threads, and is
   guaranteed not to be freed until "done" is called by all threads.  Element
   structs must have their own locks if picking threads can modify the element.
*/

struct pool;

/* This must be the first field in structs that go in a pool */
typedef struct pool_entry {
  struct pool *pool;
  int ref;                      /* threads using this entry */
} pool_entry;

typedef void (*pool_free_fn)(void*);

typedef struct pool {
  pthread_mutex_t lock;
  pool_entry **entries;
  size_t size, max;
  pool_free_fn free_fn;
} pool;


void pool_init(pool *p, size_t max, pool_free_fn free_fn) {
  pthread_mutex_init(&p->lock, NULL);
  p->entries = (pool_entry**)calloc(max, sizeof(*p->entries));
  p->size = 0;
  p->max = max;
  p->free_fn = free_fn;
}

void pool_destroy(pool *p) {
  pthread_mutex_destroy(&p->lock);
  free(p->entries);
}

bool pool_add(pool *p, pool_entry *entry) {
  pthread_mutex_lock(&p->lock);
  entry->pool = p;
  entry->ref = 1;
  bool ok = false;
  if (p->size < p->max) {
    p->entries[p->size++] = entry;
    ok = true;
  }
  pthread_mutex_unlock(&p->lock);
  return ok;
}

void pool_unref(pool_entry *entry) {
  pool *p = (pool*)entry->pool;
  pthread_mutex_lock(&p->lock);
  if (--entry->ref == 0) {
    size_t i = 0;
    while (i < p->size && p->entries[i] != entry) ++i;
    if (i < p->size) {
      --p->size;
      memmove(p->entries + i, p->entries + i + 1, (p->size - i)*sizeof(*p->entries));
      (*p->free_fn)(entry);
    }
  }
  pthread_mutex_unlock(&p->lock);
}

/* Pick a random element, NULL if empty. Call pool_unref(entry) when finished */
pool_entry *pool_pick(pool *p) {
  pthread_mutex_lock(&p->lock);
  pool_entry *entry = NULL;
  if (p->size) {
    entry = p->entries[rand() % p->size];
    ++entry->ref;
  }
  pthread_mutex_unlock(&p->lock);
  return entry;
}

/* Macro for type-safe pools */
#define DECLARE_POOL(P, T)                                              \
  typedef struct P { pool p; } P;                                       \
  void P##_init(P *p, size_t max, void (*free_fn)(T*)) { pool_init((pool*)p, max, (pool_free_fn) free_fn); } \
  bool P##_add(P *p, T *entry) { return pool_add((pool*)p, &entry->entry); } \
  void P##_unref(T *entry) { pool_unref(&entry->entry); }               \
  T *P##_pick(P *p) { return (T*)pool_pick((pool*) p); }                \
  void P##_destroy(P *p) { pool_destroy(&p->p); }


/* Connection pool */

typedef struct connection_ctx {
  pool_entry entry;
  pn_connection_t *pn_connection;
  pthread_mutex_t lock;    /* Lock PN_TRANSPORT_CLOSED vs. pn_connection_wake() */
} connection_ctx;

DECLARE_POOL(cpool, connection_ctx)

static connection_ctx* connection_ctx_new(void) {
  connection_ctx *ctx = (connection_ctx*)malloc(sizeof(*ctx));
  ctx->pn_connection = pn_connection();
  pthread_mutex_init(&ctx->lock, NULL);
  pn_connection_set_context(ctx->pn_connection, ctx);
  return ctx;
}

static void connection_ctx_free(connection_ctx* ctx) {
  /* Don't free ctx->pn_connection, freed by proactor */
  pthread_mutex_destroy(&ctx->lock);
  free(ctx);
}

void cpool_connect(cpool *cp, pn_proactor_t *proactor, const char *addr) {
  if (!action_enabled[A_CONNECT]) return;
  connection_ctx *ctx = connection_ctx_new();
  if (cpool_add(cp, ctx)) {
    debuga(A_CONNECT, ctx->pn_connection);
    pn_proactor_connect(proactor, ctx->pn_connection, addr);
  } else {
    pn_connection_free(ctx->pn_connection); /* Won't be freed by proactor */
    connection_ctx_free(ctx);
  }
}

void cpool_wake(cpool *cp) {
  if (!action_enabled[A_WAKE]) return;
  connection_ctx *ctx = cpool_pick(cp);
  if (ctx) {
    /* Required locking: application may not call wake on a freed connection */
    pthread_mutex_lock(&ctx->lock);
    if (ctx && ctx->pn_connection) {
      debuga(A_WAKE, ctx->pn_connection);
      pn_connection_wake(ctx->pn_connection);
    }
    pthread_mutex_unlock(&ctx->lock);
    cpool_unref(ctx);
  }
}

static void connection_ctx_on_close(connection_ctx *ctx) {
  /* Required locking: mark connection (possibly) closed no more wake calls */
  pthread_mutex_lock(&ctx->lock);
  ctx->pn_connection = NULL;
  pthread_mutex_unlock(&ctx->lock);
  cpool_unref(ctx);
}

/* Listener pool */

typedef struct listener_ctx {
  pool_entry entry;
  pn_listener_t *pn_listener;
  pthread_mutex_t lock;         /* Lock PN_LISTENER_CLOSE vs. pn_listener_close() */
  char addr[PN_MAX_ADDR];
} listener_ctx;

DECLARE_POOL(lpool, listener_ctx)

static listener_ctx* listener_ctx_new(void) {
  listener_ctx *ctx = (listener_ctx*)malloc(sizeof(*ctx));
  ctx->pn_listener = pn_listener();
  pthread_mutex_init(&ctx->lock, NULL);
  /* Use "invalid:address" because "" is treated as ":5672" */
  strncpy(ctx->addr, "invalid:address", sizeof(ctx->addr));
  pn_listener_set_context(ctx->pn_listener, ctx);
  return ctx;
}

static void listener_ctx_free(listener_ctx *ctx) {
  /* Don't free ctx->pn_listener, freed by proactor */
  pthread_mutex_destroy(&ctx->lock);
  free(ctx);
}

static void lpool_listen(lpool *lp, pn_proactor_t *proactor) {
  if (!action_enabled[A_LISTEN]) return;
  char a[PN_MAX_ADDR];
  pn_proactor_addr(a, sizeof(a), "", "0");
  listener_ctx *ctx = listener_ctx_new();
  if (lpool_add(lp, ctx)) {
    debuga(A_LISTEN,  ctx->pn_listener);
    pn_proactor_listen(proactor, ctx->pn_listener, a, BACKLOG);
  } else {
    pn_listener_free(ctx->pn_listener); /* Won't be freed by proactor */
    listener_ctx_free(ctx);
  }
}

/* Advertise address once open */
static void listener_ctx_on_open(listener_ctx *ctx) {
  pthread_mutex_lock(&ctx->lock);
  if (ctx->pn_listener) {
    pn_netaddr_str(pn_listener_addr(ctx->pn_listener), ctx->addr, sizeof(ctx->addr));
  }
  debug("[%p] listening on %s", ctx->pn_listener, ctx->addr);
  pthread_mutex_unlock(&ctx->lock);
}

static void listener_ctx_on_close(listener_ctx *ctx) {
  pthread_mutex_lock(&ctx->lock);
  ctx->pn_listener = NULL;
  pthread_mutex_unlock(&ctx->lock);
  lpool_unref(ctx);
}

/* Pick a random listening address from the listener pool.
   Returns "invalid:address" for no address.
*/
static void lpool_addr(lpool *lp, char* a, size_t s) {
  strncpy(a, "invalid:address", s);
  listener_ctx *ctx = lpool_pick(lp);
  if (ctx) {
    pthread_mutex_lock(&ctx->lock);
    strncpy(a, ctx->addr, s);
    pthread_mutex_unlock(&ctx->lock);
    lpool_unref(ctx);
  }
}

void lpool_close(lpool *lp) {
  if (!action_enabled[A_LCLOSE]) return;
  listener_ctx *ctx = lpool_pick(lp);
  if (ctx) {
    pthread_mutex_lock(&ctx->lock);
    if (ctx->pn_listener) {
      pn_listener_close(ctx->pn_listener);
      debuga(A_LCLOSE, ctx->pn_listener);
    }
    pthread_mutex_unlock(&ctx->lock);
    lpool_unref(ctx);
  }
}

/* Global state */

typedef struct {
  pn_proactor_t *proactor;
  int threads;
  lpool listeners;              /* waiting for PN_PROACTOR_LISTEN */
  cpool connections_active;     /* active in the proactor */
  cpool connections_idle;       /* released and can be reused */

  pthread_mutex_t lock;
  bool shutdown;
} global;

void global_init(global *g, int threads) {
  memset(g, 0, sizeof(*g));
  g->proactor = pn_proactor();
  g->threads = threads;
  lpool_init(&g->listeners, g->threads/2, listener_ctx_free);
  cpool_init(&g->connections_active, g->threads/2, connection_ctx_free);
  cpool_init(&g->connections_idle, g->threads/2, connection_ctx_free);
  pthread_mutex_init(&g->lock, NULL);
}

void global_destroy(global *g) {
  pn_proactor_free(g->proactor);
  lpool_destroy(&g->listeners);
  cpool_destroy(&g->connections_active);
  cpool_destroy(&g->connections_idle);
  pthread_mutex_destroy(&g->lock);
}

bool global_get_shutdown(global *g) {
  pthread_mutex_lock(&g->lock);
  bool ret = g->shutdown;
  pthread_mutex_unlock(&g->lock);
  return ret;
}

void global_set_shutdown(global *g, bool set) {
  pthread_mutex_lock(&g->lock);
  g->shutdown = set;
  pthread_mutex_unlock(&g->lock);
}

void global_connect(global *g) {
  char a[PN_MAX_ADDR];
  lpool_addr(&g->listeners, a, sizeof(a));
  cpool_connect(&g->connections_active, g->proactor, a);
}

/* Return true with given probability */
static bool maybe(double probability) {
  if (probability == 1.0) return true;
  return rand() < (probability * RAND_MAX);
}

/* Run random activities that can be done from any thread. */
static void global_do_stuff(global *g) {
  if (maybe(0.5)) global_connect(g);
  if (maybe(0.3)) lpool_listen(&g->listeners, g->proactor);
  if (maybe(0.5)) cpool_wake(&g->connections_active);
  if (maybe(0.5)) cpool_wake(&g->connections_idle);
  if (maybe(0.1)) lpool_close(&g->listeners);
  if (action_enabled[A_TIMEOUT] && maybe(0.5)) {
    debuga(A_TIMEOUT, g->proactor);
    pn_proactor_set_timeout(g->proactor, rand() % TIMEOUT_MAX);
  }
}

static void* user_thread(void* void_g) {
  debug("user_thread start");
  global *g = (global*)void_g;
  while (!global_get_shutdown(g)) {
    global_do_stuff(g);
    millisleep(rand() % SLEEP_MAX);
  }
  debug("user_thread end");
  return NULL;
}

static bool handle(global *g, pn_event_t *e) {
  switch (pn_event_type(e)) {

   case PN_PROACTOR_TIMEOUT: {
     if (global_get_shutdown(g)) return false;
     global_do_stuff(g);
     break;
   }
   case PN_LISTENER_OPEN: {
     listener_ctx *ctx = (listener_ctx*)pn_listener_get_context(pn_event_listener(e));
     listener_ctx_on_open(ctx);
     cpool_connect(&g->connections_active, g->proactor, ctx->addr); /* Initial connection */
     break;
   }
   case PN_LISTENER_CLOSE: {
     listener_ctx_on_close((listener_ctx*)pn_listener_get_context(pn_event_listener(e)));
     break;
   }
   case PN_TRANSPORT_CLOSED: {
     connection_ctx_on_close((connection_ctx*)pn_connection_get_context(pn_event_connection(e)));
     break;
   }
   case PN_PROACTOR_INACTIVE:           /* Shutting down */
    pn_proactor_interrupt(g->proactor); /* Interrupt remaining threads */
    return false;

   case PN_PROACTOR_INTERRUPT:
    pn_proactor_interrupt(g->proactor); /* Pass the interrupt along */
    return false;

    /* FIXME aconway 2018-03-09: MORE */

   default:
    break;
  }
  return true;
}

static void* proactor_thread(void* void_g) {
  debug("proactor_thread start");
  global *g = (global*) void_g;
  bool ok = true;
  while (ok) {
    pn_event_batch_t *events = pn_proactor_wait(g->proactor);
    pn_event_t *e;
    while (ok && (e = pn_event_batch_next(events))) {
      ok = ok && handle(g, e);
    }
    pn_proactor_done(g->proactor, events);
  }
  debug("proactor_thread end");
  return NULL;
}

/* Command line arguments */

static const int default_runtime = 1;
static const int default_threads = 8;

void usage(const char **argv, const char **arg, const char **end) {
  fprintf(stderr, "usage: %s [options]\n", argv[0]);
  fprintf(stderr, "  -time TIME: total run-time in seconds (default %d)\n", default_runtime);
  fprintf(stderr, "  -threads THREADS: total number of threads (default %d)\n", default_threads);
  fprintf(stderr, "  -debug: print debug messages\n");
  fprintf(stderr, " ");
  for (int i = 0; i < (int)action_size; ++i) fprintf(stderr, " -%s", action_name[i]);
  fprintf(stderr, ": enable actions\n\n");

  fprintf(stderr, "bad argument: ");
  for (const char **a = argv+1; a < arg; ++a) fprintf(stderr, "%s ", *a);
  fprintf(stderr, ">>> %s <<<", *arg++);
  for (; arg < end; ++arg) fprintf(stderr, " %s", *arg);
  exit(1);
}

int main(int argc, const char* argv[]) {
  const char **arg = argv + 1;
  const char **end = argv + argc;
  int runtime = default_runtime;
  int threads = default_threads;

  while (arg < end) {
    if (!strcmp(*arg, "-time") && ++arg < end) {
      runtime = atoi(*arg);
      if (runtime <= 0) usage(argv, arg, end);
    }
    else if (!strcmp(*arg, "-threads") && ++arg < end) {
      threads = atoi(*arg);
      if (threads <= 0) usage(argv, arg, end);
      if (threads % 2) threads += 1; /* Round up to even: half proactor, half user */
    }
    else if (!strcmp(*arg, "-debug")) {
      debug_enable = true;
    }
    else if (**arg == '-') {
      size_t i = 0;
      while (i < action_size && strcmp((*arg)+1, action_name[i])) ++i;
      if (i == action_size) usage(argv, arg, end);
      action_enabled[i] = true;
    } else {
      break;
    }
    ++arg;
  }
  int i = 0;
  /* If no actions are requested on command line, enable them all */
  while (i < (int)action_size && !action_enabled[i]) ++i;
  if (i == action_size) {
    for (i = 0; i < (int)action_size; ++i) action_enabled[i] = true;
  }

  /* Set up global state, start threads */
  debug("threaderciser start threads=%d, time=%d\n", threads, runtime);
  global g;
  global_init(&g, threads);
  lpool_listen(&g.listeners, g.proactor); /* Start initial listener */

  pthread_t *user_threads = (pthread_t*)calloc(threads/2, sizeof(pthread_t));
  pthread_t *proactor_threads = (pthread_t*)calloc(threads/2, sizeof(pthread_t));
  for (i = 0; i < threads/2; ++i) {
    pthread_create(&user_threads[i], NULL, user_thread, &g);
    pthread_create(&proactor_threads[i], NULL, proactor_thread, &g);
  }
  millisleep(runtime*1000);

  debug("shut down");
  global_set_shutdown(&g, true);
  void *ignore;
  for (i = 0; i < threads/2; ++i) pthread_join(user_threads[i], &ignore);
  debug("disconnect");
  pn_proactor_disconnect(g.proactor, NULL);
  for (i = 0; i < threads/2; ++i) pthread_join(proactor_threads[i], &ignore);

  free(user_threads);
  free(proactor_threads);
  global_destroy(&g);
}
