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

#include "core/memory.h"

#ifdef PN_MEMDEBUG
#include "logger_private.h"

#include "proton/object.h"
#include "proton/cid.h"

#include <stdlib.h>

#include <signal.h>

// Non portable actual size of allocated block
// malloc_usable_size() for glibc
// _msize() for MSCRT
// malloc_size() for BSDs
#ifdef __GLIBC__
#include <malloc.h>
#define msize malloc_usable_size
#elif defined(_WIN32)
#include <malloc.h>
#define msize _msize
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <malloc/malloc.h>
#define msize malloc_size
#else
#define msize(x) (0)
#endif

static struct stats {
  const char* name;
  size_t count_alloc;
  size_t count_dealloc;
  size_t requested;
  size_t alloc;
  size_t dealloc;
  size_t count_suballoc;
  size_t count_subrealloc;
  size_t count_subdealloc;
  size_t subrequested;
  size_t suballoc;
  size_t subdealloc;
} stats[CID_pn_raw_connection+1] = {{0}}; // Just happens to be the last CID

static bool debug_memory = false;

// Hidden export
PN_EXTERN void pn_mem_dump_stats(void);

// Dump out memory use
void pn_mem_dump_stats(void)
{
  pn_logger_t *logger = pn_default_logger();

  size_t total_alloc = 0;
  size_t total_dealloc = 0;
  size_t count_alloc = 0;
  size_t count_dealloc = 0;

  if (PN_SHOULD_LOG(logger, PN_SUBSYSTEM_MEMORY, PN_LEVEL_DEBUG)) {
    pn_logger_logf(logger, PN_SUBSYSTEM_MEMORY, PN_LEVEL_DEBUG, "Memory allocation stats:");
    pn_logger_logf(logger, PN_SUBSYSTEM_MEMORY, PN_LEVEL_DEBUG, "%15s:          %5s %5s %5s %9s %9s",
                   "class", "alloc", "free", "rallc", "bytes alloc", "bytes free"
    );
  }
  for (int i = 1; i<=CID_pn_listener_socket; i++) {
    struct stats *entry = &stats[i];
    count_alloc += entry->count_alloc+entry->count_suballoc;
    count_dealloc += entry->count_dealloc+entry->count_subdealloc;
    total_alloc += entry->alloc+entry->suballoc;
    total_dealloc += entry->dealloc+entry->subdealloc;
    if (entry->name && PN_SHOULD_LOG(logger, PN_SUBSYSTEM_MEMORY, PN_LEVEL_DEBUG)) {
      pn_logger_logf(logger, PN_SUBSYSTEM_MEMORY, PN_LEVEL_DEBUG, "%15s:   direct %5d %5d       %9d %9d",
                     entry->name,
                     entry->count_alloc, entry->count_dealloc,
                     entry->alloc, entry->dealloc
      );
      pn_logger_logf(logger, PN_SUBSYSTEM_MEMORY, PN_LEVEL_DEBUG, "%15s: indirect %5d %5d %5d %9d %9d",
                     entry->name,
                     entry->count_suballoc, entry->count_subdealloc, entry->count_subrealloc,
                     entry->suballoc, entry->subdealloc
      );
    }
  }

  if (!PN_SHOULD_LOG(logger, PN_SUBSYSTEM_MEMORY, PN_LEVEL_INFO)) return;

  pn_logger_logf(logger, PN_SUBSYSTEM_MEMORY, PN_LEVEL_INFO, "%15s:        %7d %5d       %9d %9d",
                 "Totals", count_alloc, count_dealloc, total_alloc, total_dealloc
  );
}

#ifdef _WIN32
void pni_mem_setup_logging(void)
{
}
#else
static void pn_mem_dump_stats_int(int i)
{
  pn_logger_t *logger = pn_default_logger();
  pn_logger_t save = *logger;

  pn_logger_set_mask(logger, PN_SUBSYSTEM_MEMORY, PN_LEVEL_DEBUG | PN_LEVEL_INFO);

  pn_mem_dump_stats();

  *logger = save;
}

void pni_mem_setup_logging(void)
{
  debug_memory = true;
  signal(SIGUSR1, pn_mem_dump_stats_int);
}
#endif

void pni_init_memory(void)
{
}

void pni_fini_memory(void)
{
  pn_logger_t *logger = pn_default_logger();
  pn_logger_t save = *logger;

  if (debug_memory)
    pn_logger_set_mask(logger, PN_SUBSYSTEM_MEMORY, PN_LEVEL_DEBUG | PN_LEVEL_INFO);

  pn_mem_dump_stats();

  *logger = save;
}

static inline struct stats *pni_track_common(const pn_class_t *clazz, void *o, size_t *size)
{
  struct stats *entry = &stats[pn_class_id(clazz)];
  if (!entry->name) {entry->name = pn_class_name(clazz);}
  *size = msize(o);
  return entry;
}

static void pni_track_alloc(const pn_class_t *clazz, void *o, size_t requested)
{
  size_t size;
  struct stats *entry = pni_track_common(clazz, o, &size);

  entry->count_alloc++;
  entry->alloc += size;
  entry->requested += requested;
}

static void pni_track_dealloc(const pn_class_t *clazz, void *o)
{
  size_t size;
  struct stats *entry = pni_track_common(clazz, o, &size);

  entry->count_dealloc++;
  entry->dealloc += size;
}

static void pni_track_suballoc(const pn_class_t *clazz, void *o, size_t requested)
{
  size_t size;
  struct stats *entry = pni_track_common(clazz, o, &size);

  entry->count_suballoc++;
  entry->subrequested = requested;
  entry->suballoc += size;
}

static void pni_track_subdealloc(const pn_class_t *clazz, void *o)
{
  size_t size;
  struct stats *entry = pni_track_common(clazz, o, &size);

  entry->count_subdealloc++;
  entry->subdealloc += size;
}

static void pni_track_subrealloc(const pn_class_t *clazz, void *o, size_t oldsize, size_t requested)
{
  size_t size;
  struct stats *entry = pni_track_common(clazz, o, &size);

  if (oldsize) {
    entry->count_subrealloc++;
  } else {
    entry->count_suballoc++;
  }

  int change = size - oldsize;
  if (change > 0) entry->suballoc += change;
  if (change < 0) entry->subdealloc += -change;
}

void *pni_mem_zallocate(const pn_class_t *clazz, size_t size)
{
  void *o = calloc(1, size);
  pni_track_alloc(clazz, o, size);
  return o;
}

void *pni_mem_allocate(const pn_class_t *clazz, size_t size)
{
  void * o = malloc(size);
  pni_track_alloc(clazz, o, size);
  return o;
}

void pni_mem_deallocate(const pn_class_t *clazz, void *object)
{
  if (!object) return;
  pni_track_dealloc(clazz, object);
  free(object);
}

void *pni_mem_suballocate(const pn_class_t *clazz, void *object, size_t size)
{
  void * o = malloc(size);
  pni_track_suballoc(clazz, o, size);
  return o;
}

void *pni_mem_subreallocate(const pn_class_t *clazz, void *object, void *buffer, size_t size)
{
  size_t oldsize = buffer ? msize(buffer) : 0;
  void *o = realloc(buffer, size);
  pni_track_subrealloc(clazz, o, oldsize, size);
  return o;
}

void pni_mem_subdeallocate(const pn_class_t *clazz, void *object, void *buffer)
{
  if (!buffer) return;
  pni_track_subdealloc(clazz, buffer);
  free(buffer);
}
#else

// Versions with no memory debugging - so we can compile with no performance penalty

#include <stdlib.h>

void pni_init_memory(void) {}
void pni_fini_memory(void) {}

void pni_mem_setup_logging(void) {}

void *pni_mem_allocate(const pn_class_t *clazz, size_t size) { return malloc(size); }
void *pni_mem_zallocate(const pn_class_t *clazz, size_t size) { return calloc(1, size); }
void pni_mem_deallocate(const pn_class_t *clazz, void *object) { free(object); }

void *pni_mem_suballocate(const pn_class_t *clazz, void *object, size_t size) { return malloc(size); }
void *pni_mem_subreallocate(const pn_class_t *clazz, void *object, void *buffer, size_t size) { return realloc(buffer, size); }
void pni_mem_subdeallocate(const pn_class_t *clazz, void *object, void *buffer) { free(buffer); }

#endif
