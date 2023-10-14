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

#include "logger_private.h"

#include <proton/logger.h>
#include <proton/error.h>

#include "fixed_string.h"
#include "memory.h"
#include "util_str.h"
#include "value_dump.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void pni_default_log_sink(intptr_t logger, pn_log_subsystem_t subsystem, pn_log_level_t severity, const char *message)
{
  fprintf(stderr, "[%p]:%5s:%5s:%s\n", (void *) logger, pn_logger_subsystem_name(subsystem), pn_logger_level_name(severity), message);
  fflush(stderr);
}

static pn_logger_t the_default_logger = {
  .sink = pni_default_log_sink,
  .sink_context = (intptr_t) &the_default_logger,
  .sub_mask = PN_SUBSYSTEM_ALL,
  .sev_mask = PN_LEVEL_CRITICAL
};

void pni_logger_init(pn_logger_t *logger)
{
  *logger = the_default_logger;
  logger->sink_context = (intptr_t) logger;
}

void pni_logger_fini(pn_logger_t *logger)
{
}

#define LOGLEVEL(x)   {sizeof(#x)-1, #x, PN_LEVEL_ ## x, PN_LEVEL_ ## x-1}
#define TRACE(x)      {sizeof(#x)-1, #x, PN_LEVEL_ ## x}
#define SPECIAL(x, y) {sizeof(#x)-1, #x, PN_LEVEL_NONE, PN_LEVEL_NONE, y}
typedef struct {
    uint8_t   strlen;
    const char str[11];
    uint16_t   level;
    uint16_t   plus_levels;
    void     (*special)(void);
} log_level;
static const log_level log_levels[] = {
  LOGLEVEL(ERROR),
  LOGLEVEL(WARNING),
  LOGLEVEL(INFO),
  LOGLEVEL(DEBUG),
  LOGLEVEL(TRACE),
  LOGLEVEL(ALL),
  TRACE(FRAME),
  TRACE(RAW),
  SPECIAL(MEMORY, pni_mem_setup_logging),
  {0, ""}
};

void pni_decode_log_env(const char *log_env, int *setmask)
{
  if (!log_env) return;

  for (int i = 0; log_env[i]; i++) {
    for (const log_level *level = &log_levels[0]; level->strlen; level++) {
      if (pn_strncasecmp(&log_env[i], level->str, level->strlen)==0) {
        *setmask |= level->level;
        i += level->strlen;
        if (log_env[i]=='+') {
          i++;
          *setmask |= level->plus_levels;
        }
        i--;
        if (level->special) level->special();
        break;
      }
    }
  }
}

void pni_init_default_logger(void)
{
  int sev_mask = 0;
  int sub_mask = 0;
  /* Back compatible environment settings */
  if (pn_env_bool("PN_TRACE_RAW")) { sev_mask |= PN_LEVEL_RAW; }
  if (pn_env_bool("PN_TRACE_FRM")) { sev_mask |= PN_LEVEL_FRAME; }

  /* These are close enough for obscure undocumented settings */
  if (pn_env_bool("PN_TRACE_DRV")) { sev_mask |= PN_LEVEL_TRACE | PN_LEVEL_DEBUG; }
  if (pn_env_bool("PN_TRACE_EVT")) { sev_mask |= PN_LEVEL_DEBUG; }

  /* Decode PN_LOG into logger settings */
  pni_decode_log_env(getenv("PN_LOG"), &sev_mask);

  the_default_logger.sev_mask = (pn_log_level_t) (the_default_logger.sev_mask | sev_mask);
  the_default_logger.sub_mask = (pn_log_subsystem_t) (the_default_logger.sub_mask | sub_mask);
}

void pni_fini_default_logger(void)
{
  pni_logger_fini(&the_default_logger);
}

const char *pn_logger_level_name(pn_log_level_t severity)
{
  if (severity==PN_LEVEL_ALL)     return "*ALL*";
  if (severity&PN_LEVEL_CRITICAL) return "CRITICAL";
  if (severity&PN_LEVEL_ERROR)    return "ERROR";
  if (severity&PN_LEVEL_WARNING)  return "WARNING";
  if (severity&PN_LEVEL_INFO)     return "INFO";
  if (severity&PN_LEVEL_DEBUG)    return "DEBUG";
  if (severity&PN_LEVEL_TRACE)    return "TRACE";
  if (severity&PN_LEVEL_FRAME)    return "FRAME";
  if (severity&PN_LEVEL_RAW)      return "RAW";
  return "UNKNOWN";
}

const char *pn_logger_subsystem_name(pn_log_subsystem_t subsystem)
{
  if (subsystem==PN_SUBSYSTEM_ALL)    return "*ALL*";
  if (subsystem&PN_SUBSYSTEM_MEMORY)  return "MEMORY";
  if (subsystem&PN_SUBSYSTEM_IO)      return "IO";
  if (subsystem&PN_SUBSYSTEM_EVENT)   return "EVENT";
  if (subsystem&PN_SUBSYSTEM_AMQP)    return "AMQP";
  if (subsystem&PN_SUBSYSTEM_SSL)     return "SSL";
  if (subsystem&PN_SUBSYSTEM_SASL)    return "SASL";
  if (subsystem&PN_SUBSYSTEM_BINDING) return "BINDING";
  return "UNKNOWN";
}

pn_logger_t *pn_default_logger(void)
{
  return &the_default_logger;
}

void pn_logger_set_mask(pn_logger_t *logger, uint16_t subsystem, uint16_t severity)
{
  logger->sev_mask = (pn_log_level_t) (logger->sev_mask | severity);
  logger->sub_mask = (pn_log_subsystem_t) (logger->sub_mask | subsystem);
}

void pn_logger_reset_mask(pn_logger_t *logger, uint16_t subsystem, uint16_t severity)
{
  logger->sev_mask = (pn_log_level_t) (logger->sev_mask & ~severity);
  logger->sub_mask = (pn_log_subsystem_t) (logger->sub_mask & ~subsystem);
}

void pn_logger_set_log_sink(pn_logger_t *logger, pn_log_sink_t sink, intptr_t sink_context)
{
  logger->sink = sink;
  logger->sink_context = sink_context;
}

pn_log_sink_t pn_logger_get_log_sink(pn_logger_t *logger)
{
  return logger->sink;
}

intptr_t pn_logger_get_log_sink_context(pn_logger_t *logger)
{
  return logger->sink_context;
}

void pni_logger_log_data(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t severity, const char *msg, const char *bytes, size_t size)
{
  char buf[256];
  ssize_t n = pn_quote_data(buf, 256, bytes, size);
  if (n >= 0) {
    pn_logger_logf(logger, subsystem, severity, "%s: \"%s\"", msg, buf);
  } else if (n == PN_OVERFLOW) {
    pn_logger_logf(logger, subsystem, severity, "%s: \"%s\"... (truncated)", msg, buf);
  }
}

void pni_logger_log_raw(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t severity, pn_bytes_t bytes, size_t size, const char* msg)
{
  char buf[256];

  const char *start = &bytes.start[bytes.size-size];
  for (unsigned i = 0; i < size; i+=16) {
    pn_fixed_string_t out = pn_fixed_string(buf, sizeof(buf));
    pn_fixed_string_addf(&out, "%s%04x/%04zx: ", msg, i, size);
    for (unsigned j = 0; j<16; j++) {
      if (i+j<size) {
        pn_fixed_string_addf(&out, "%02hhx ", start[i+j]);
      } else {
        pn_fixed_string_append(&out, pn_string_const("   ", 3));
      }
    }
    for (unsigned j = 0; j<16; j++) {
      if (i+j>=size) break;
      char c = start[i+j];
      if (c>32) { // c is signed so the high bit set is negative
        pn_fixed_string_append(&out, pn_string_const(&c, 1));
      } else {
        pn_fixed_string_append(&out, STR_CONST(.));
      }
    }
    pn_fixed_string_terminate(&out);
    pni_logger_log(logger, subsystem, severity, buf);
  }
}

void pni_logger_log_msg_inspect(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t severity, void* object, PN_PRINTF_FORMAT const char *fmt, ...) {
  va_list ap;
  char buf[1024];
  pn_fixed_string_t out = pn_fixed_string(buf, sizeof(buf));

  va_start(ap, fmt);
  pn_fixed_string_vaddf(&out, fmt, ap);
  va_end(ap);

  pn_finspect(object, &out);
  pn_fixed_string_terminate(&out);
  pni_logger_log(logger, subsystem, severity, buf);
}

void pni_logger_log_msg_frame(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t severity, pn_bytes_t frame, PN_PRINTF_FORMAT const char *fmt, ...) {
  va_list ap;
  char buf[1024];
  pn_fixed_string_t output = pn_fixed_string(buf, sizeof(buf));

  va_start(ap, fmt);
  pn_fixed_string_vaddf(&output, fmt, ap);
  va_end(ap);

  size_t psize = pni_value_dump(frame, &output);
  pn_bytes_t payload = {.size=frame.size-psize, .start=frame.start+psize};
  if (payload.size>0) {
    pn_fixed_string_addf(&output, " (%zu) ", payload.size);
    pn_fixed_string_quote(&output, payload.start, payload.size);
  }
  if (output.position==output.size) {
    // Message overflow
    const char truncated[] = " ... (truncated)";
    output.position -= sizeof(truncated);
    pn_fixed_string_append(&output, pn_string_const(truncated, sizeof(truncated)));
  }
  pn_fixed_string_terminate(&output);
  pni_logger_log(logger, subsystem, severity, buf);
}

void pni_logger_log(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t severity, const char *message)
{
  assert(logger);
  logger->sink(logger->sink_context, subsystem, severity, message);
}

void pni_logger_vlogf(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t severity, const char *fmt, va_list ap)
{
  assert(logger);
  char buf[1024];
  pn_fixed_string_t output = pn_fixed_string(buf, sizeof(buf));
  pn_fixed_string_vaddf(&output, fmt, ap);
  if (output.position==output.size) {
    // Message overflow
    const char truncated[] = " ... (truncated)";
    output.position -= sizeof(truncated);
    pn_fixed_string_append(&output, pn_string_const(truncated, sizeof(truncated)));
  }
  pn_fixed_string_terminate(&output);
  pni_logger_log(logger, subsystem, severity, buf);
}

void pn_logger_logf(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t severity, PN_PRINTF_FORMAT const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  pni_logger_vlogf(logger, subsystem, severity, fmt, ap);
  va_end(ap);
}
