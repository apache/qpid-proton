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

/**
 * @file
 * @copybrief logger
 * @copydetails logger
 *
 * @defgroup logger Logger
 * @ingroup core
 */

#include <proton/import_export.h>
#include <proton/object.h>

#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup logger
 * @{
 *
 * Facility for logging messages
 *
 * The proton library has internal logging which provides information about the
 * functioning of the library. This Logger system (see @ref pn_logger_t) allows applications
 * to customize how the logging is recorded and output.
 *
 * Each logged message has an attached level (see @ref pn_log_level_t) which
 * indicates how important the message is; and also has an attached subsystem
 * (see @ref pn_log_subsystem_t) which indicates which part of proton is producing#
 * the log message. The levels go from "Critical" which indicates a condition the library cannot
 * recover from.
 *
 * Applications receive log messages by registering a callback function (@ref pn_log_sink_t). This
 * receives the logged message and other information and allows the application to consume the
 * logged messages as it wants. Applications can filter the messages that they receive by setting
 * bit masks in the logger. They can set the logger filtering both by subsystem and by level.
 * Additionally, since the callback contains both the subsystem and level information for each
 * logged message, applications can impose further, more complex, filters of their own.
 *
 * Each application will have a default logger which can be retrieved with @ref pn_default_logger.
 * The default logger is used as the template for every other logger that is created. So an efficient
 * way to configure logging is to configure the default logger at the very start of the application
 * before any other loggers get created.
 *
 * Loggers are associated with different proton objects, primarily the transport object
 * (@ref pn_transport_t) and each logger controls the logging for the associated object. This means
 * that for example in order to control the logging for an AMQP connection you need to acquire the
 * logger object from the transport object using @ref pn_transport_logger and to configure that.
 *
 * Initially the defaults are to log every subsystem but not any level (except 'Critical' which is
 * always logged). This means in effect that it is only necessary to turn on the log levels that
 * are interesting and all subsystems will log. Of course you can turn off subsystems that are not
 * interesting or are too verbose.
 *
 * There is also a default log sink if the application does not register their own, but logging
 * is turned on - this will output the log message to standard error.
 */

/**
 * The logger object allows library logging to be controlled
 */
typedef struct pn_logger_t pn_logger_t;

/**
 * Definitions for different subsystems that can log messages.
 * Note that these are exclusive bits so that you can specify multiple
 * subsystems to filter
 */
typedef enum pn_log_subsystem_t {
    PN_SUBSYSTEM_NONE    = 0,    /**< No subsystem */
    PN_SUBSYSTEM_MEMORY  = 1,    /**< Memory usage */
    PN_SUBSYSTEM_IO      = 2,    /**< Low level Input/Output */
    PN_SUBSYSTEM_EVENT   = 4,    /**< Events */
    PN_SUBSYSTEM_AMQP    = 8,    /**< AMQP protocol processing */
    PN_SUBSYSTEM_SSL     = 16,   /**< TLS/SSL protocol processing */
    PN_SUBSYSTEM_SASL    = 32,   /**< SASL protocol processing */
    PN_SUBSYSTEM_BINDING = 64,   /**< Language binding */
    PN_SUBSYSTEM_ALL     = 65535 /**< Every subsystem */
} pn_log_subsystem_t; /* We hint to the compiler it can use 16 bits for this value */

/**
 * Definitions for different severities of log messages
 * Note that these are exclusive bits so that you can specify multiple
 * severities to filter
 */
typedef enum pn_log_level_t {
    PN_LEVEL_NONE     = 0,    /**< No level */
    PN_LEVEL_CRITICAL = 1,    /**< Something is wrong and can't be fixed - probably a library bug */
    PN_LEVEL_ERROR    = 2,    /**< Something went wrong */
    PN_LEVEL_WARNING  = 4,    /**< Something unusual happened but not necessarily an error */
    PN_LEVEL_INFO     = 8,    /**< Something that might be interesting happened */
    PN_LEVEL_DEBUG    = 16,   /**< Something you might want to know about happened */
    PN_LEVEL_TRACE    = 32,   /**< Detail about something that happened */
    PN_LEVEL_FRAME    = 64,   /**< Protocol frame traces */
    PN_LEVEL_RAW      = 128,  /**< Raw protocol bytes */
    PN_LEVEL_ALL      = 65535 /**< Every possible level */
} pn_log_level_t; /* We hint to the compiler that it can use 16 bits for this value */

/**
 * Callback for sinking logger messages.
 */
typedef void (*pn_log_sink_t)(intptr_t sink_context, pn_log_subsystem_t subsystem, pn_log_level_t severity, const char *message);

/**
 * Return the default library logger
 *
 * @return The global default logger
 */
PN_EXTERN pn_logger_t *pn_default_logger(void);

/**
 * Get a human readable name for a logger severity
 *
 * @param[in] level the logging level
 * @return readable name for that level
 */
PN_EXTERN const char *pn_logger_level_name(pn_log_level_t level);

/**
 * Get a human readable name for a logger subsystem
 *
 * @param[in] subsystem
 * @return readable name for that subsystem
 */
PN_EXTERN const char *pn_logger_subsystem_name(pn_log_subsystem_t subsystem);

/**
 * Set a logger's tracing flags.
 *
 * Set individual trace flags to control what a logger logs.
 *
 * The trace flags for a logger control what sort of information is
 * logged. See @ref pn_log_level_t and @ref pn_log_subsystem_t for more details.
 *
 * Note that log messages with a level of @ref PN_LEVEL_CRITICAL will always be logged.
 * Otherwise log message are only logged if the subsystem and level flags both match a
 * flag in the masks held by the logger.
 *
 * If you don't want to affect the subsystem flags then you can set subsystem to
 * PN_SUBSYSTEM_NONE. likewise level to PN_LEVEL_NONE if you don't want to
 * affect the level flags.
 *
 * @param[in] logger the logger
 * @param[in] subsystem bits representing subsystems to turn on trace for
 * @param[in] level bits representing log levels to turn on trace for
 */
PN_EXTERN void pn_logger_set_mask(pn_logger_t *logger, uint16_t subsystem, uint16_t level);

/**
 * Clear a logger's tracing flags.
 *
 * Clear individual trace flags to control what a logger logs.
 *
 * The trace flags for a logger control what sort of information is
 * logged. See @ref pn_log_level_t and @ref pn_log_subsystem_t for more details.
 *
 * Note that log messages with a level of @ref PN_LEVEL_CRITICAL will always be logged.
 * Otherwise log message are only logged if the subsystem and level flags both match a
 * flag in the masks held by the logger.
 *
 * If you don't want to affect the subsystem flags then you can set subsystem to
 * PN_SUBSYSTEM_NONE. likewise level to PN_LEVEL_NONE if you don't want to
 * affect the level flags.
 *
 * @param[in] logger the logger
 * @param[in] subsystem bits representing subsystems to turn off trace for
 * @param[in] level bits representing log levels to turn off trace for
 */
PN_EXTERN void pn_logger_reset_mask(pn_logger_t *logger, uint16_t subsystem, uint16_t level);

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
 * Log a printf formatted message using the logger
 *
 * This is mainly for use in proton internals , but will allow application log messages to
 * be processed the same way.
 *
 * @param[in] logger the logger
 * @param[in] subsystem the subsystem which is producing this log message
 * @param[in] level the log level of the log message
 * @param[in] fmt the printf formatted message to be logged
 */
PN_EXTERN void pn_logger_logf(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t level, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif
