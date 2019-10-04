#ifndef LOGGER_H
#define LOGGER_H
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

/**@file
 *
 * General proton logging facility
 */

#include <proton/import_export.h>
#include <proton/object.h>

#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pn_logger_t pn_logger_t;

/**
 * Definitions for different subsystems that can log messages
 * Note that these are exclusive bits so that you can specify multiple
 * subsystems to filter
 */
typedef enum pn_log_subsystem_t {
    PN_SUBSYSTEM_NONE    = 0,
    PN_SUBSYSTEM_IO      = 1,
    PN_SUBSYSTEM_EVENT   = 2,
    PN_SUBSYSTEM_AMQP    = 4,
    PN_SUBSYSTEM_SSL     = 8,
    PN_SUBSYSTEM_SASL    = 16,
    PN_SUBSYSTEM_BINDING = 32,
    PN_SUBSYSTEM_ALL     = 65535
} pn_log_subsystem_t; /* We hint to the compiler it can use 16 bits for this value */

/**
 * Definitions for different severities of log messages
 * Note that these are exclusive bits so that you can specify multiple
 * severities to filter
 */
typedef enum pn_log_level_t {
    PN_LEVEL_NONE     = 0,
    PN_LEVEL_CRITICAL = 1,    /* Something is wrong and can't be fixed - probably a library bug */
    PN_LEVEL_ERROR    = 2,    /* Something went wrong */
    PN_LEVEL_WARNING  = 4,    /* Something unusual happened but not necessarily an error */
    PN_LEVEL_INFO     = 8,    /* Something that might be interesting happened */
    PN_LEVEL_DEBUG    = 16,   /* Something you might want to know about happened */
    PN_LEVEL_TRACE    = 32,   /* Detail about something that happened */
    PN_LEVEL_FRAME    = 64,   /* Protocol frame traces */
    PN_LEVEL_RAW      = 128,  /* Raw protocol bytes */
    PN_LEVEL_ALL      = 65535 /* Every possible severity */
} pn_log_level_t; /* We hint to the compiler that it can use 16 bits for this value */

/**
 * Callback for sinking logger messages.
 */
typedef void (*pn_log_sink_t)(intptr_t sink_context, pn_log_subsystem_t subsystem, pn_log_level_t severity, const char *message);

/*
 * Return the default library logger
 */
PN_EXTERN pn_logger_t *pn_default_logger(void);

/**
 * Create a new logger
 */
PN_EXTERN pn_logger_t *pn_logger(void);

/**
 * Free an existing logger
 */
PN_EXTERN void pn_logger_free(pn_logger_t *logger);

/**
 * Get a human readable name for a logger severity
 */
PN_EXTERN const char *pn_logger_level_name(pn_log_level_t severity);

/**
 * Get a human readable name for a logger subsystem
 */
PN_EXTERN const char *pn_logger_subsystem_name(pn_log_subsystem_t subsystem);

/**
 * Set a logger's tracing flags.
 *
 * Set trace flags to control what a logger logs.
 *
 * The trace flags for a logger control what sort of information is
 * logged. See pn_log_severity_t and pn_log_subsystem_t for more details.
 *
 * @param[in] logger the logger
 */
PN_EXTERN void pn_logger_set_mask(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t severity);

/**
 * Clear a logger's tracing flags.
 *
 * Clear trace flags to control what a logger logs.
 *
 * The trace flags for a logger control what sort of information is
 * logged. See pn_log_severity_t and pn_log_subsystem_t for more details.
 *
 * @param[in] logger the logger
 */
PN_EXTERN void pn_logger_reset_mask(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t severity);

/**
 * Set the tracing function used by a logger.
 *
 * The tracing function is called to perform logging. Overriding this
 * function allows embedding applications to divert the engine's
 * logging to a place of their choice.
 *
 * @param[in] logger the logger
 * @param[in] sink the tracing function
 * @param[in] sink_context user specified context to pass into each call of the tracing callback
 */
PN_EXTERN void pn_logger_set_log_sink(pn_logger_t *logger, pn_log_sink_t sink, intptr_t sink_context);

/**
 * Get the tracing function used by a logger.
 *
 * @param[in] logger the logger
 * @return the tracing sink function used by a logger
 */
PN_EXTERN pn_log_sink_t pn_logger_get_log_sink(pn_logger_t *logger);

/**
 * Get the sink context used by a logger.
 *
 * @param[in] logger the logger
 * @return the sink context used by a logger
 */
PN_EXTERN intptr_t pn_logger_get_log_sink_context(pn_logger_t *logger);

/**
 *  * Log a printf formatted using the logger
 *
 * This is mainly for use in proton internals , but will allow application log messages to
 * be processed the same way.
 *
 * @param[in] logger the logger
 * @param[in] fmt the printf formatted message to be logged
 */
PN_EXTERN void pn_logger_logf(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t severity, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
