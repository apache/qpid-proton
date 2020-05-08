#ifndef LOGGER_PRIVATE_H
#define LOGGER_PRIVATE_H
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

#include <proton/logger.h>

#if __cplusplus
extern "C" {
#endif

struct pn_logger_t {
    pn_log_sink_t sink;
    intptr_t      sink_context;
    pn_string_t  *scratch;
    uint16_t      sub_mask;
    uint16_t      sev_mask;
};

void pni_init_default_logger(void);
void pni_fini_default_logger(void);

void pni_logger_init(pn_logger_t*);
void pni_logger_fini(pn_logger_t*);

void pni_logger_log(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t severity, const char *message);
void pni_logger_vlogf(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t severity, const char *fmt, va_list ap);
void pni_logger_log_data(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t severity, const char *msg, const char *bytes, size_t size);

#define PN_SHOULD_LOG(logger, subsys, sev) \
    (((sev) & PN_LEVEL_CRITICAL) || (((logger)->sub_mask & (subsys)) && ((logger)->sev_mask & (sev))))

#define PN_LOG(logger, subsys, sev, ...) \
    do { \
        if (PN_SHOULD_LOG(logger, subsys, sev)) \
            pn_logger_logf(logger, (pn_log_subsystem_t) (subsys), (pn_log_level_t) (sev), __VA_ARGS__); \
    } while(0)

#define PN_LOG_DEFAULT(subsys, sev, ...) PN_LOG(pn_default_logger(), subsys, sev, __VA_ARGS__)

#define PN_LOG_DATA(logger, subsys, sev, ...) \
    do { \
        if (PN_SHOULD_LOG(logger, subsys, sev)) \
            pni_logger_log_data(logger, (pn_log_subsystem_t) (subsys), (pn_log_level_t) (sev), __VA_ARGS__); \
    } while(0)

#if __cplusplus
}
#endif

#endif
