#ifndef PROTON_SASL_H
#define PROTON_SASL_H 1

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

#include <proton/import_export.h>
#include <proton/type_compat.h>
#include <proton/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @file
 * API for the SASL Secure Transport Layer.
 *
 * The SASL layer is responsible for establishing an authenticated
 * and/or encrypted tunnel over which AMQP frames are passed between
 * peers. The peer acting as the SASL Client must provide
 * authentication credentials. The peer acting as the SASL Server must
 * provide authentication against the received credentials.
 *
 * @defgroup sasl SASL
 * @ingroup transport
 * @{
 */

typedef struct pn_sasl_t pn_sasl_t;

/** The result of the SASL negotiation */
typedef enum {
  PN_SASL_NONE=-1,  /** negotiation not completed */
  PN_SASL_OK=0,     /** authentication succeeded */
  PN_SASL_AUTH=1,   /** failed due to bad credentials */
  PN_SASL_SYS=2,    /** failed due to a system error */
  PN_SASL_PERM=3,   /** failed due to unrecoverable error */
  PN_SASL_TEMP=4    /** failed due to transient error */
} pn_sasl_outcome_t;

/** Construct an Authentication and Security Layer object
 *
 * This will return the SASL layer object for the supplied transport
 * object. If there is currently no SASL layer one will be created.
 *
 * On the client side of an AMQP connection this will have the effect
 * of ensuring that the AMQP SASL layer is used for that connection.
 *
 * @return an object representing the SASL layer.
 */
PN_EXTERN pn_sasl_t *pn_sasl(pn_transport_t *transport);

/** Do we support extended SASL negotiation
 *
 * Do we support extended SASL negotiation?
 * All implementations of Proton support ANONYMOUS and EXTERNAL on both
 * client and server sides and PLAIN on the client side.
 *
 * Extended SASL implememtations use an external library (Cyrus SASL)
 * to support other mechanisms beyond these basic ones.
 *
 * @return true if we support extended SASL negotiation, false if we only support basic negotiation.
 */
PN_EXTERN bool pn_sasl_extended(void);

/** Set the outcome of SASL negotiation
 *
 * Used by the server to set the result of the negotiation process.
 *
 * @todo
 */
PN_EXTERN void pn_sasl_done(pn_sasl_t *sasl, pn_sasl_outcome_t outcome);

/** Retrieve the outcome of SASL negotiation.
 *
 * @todo
 */
PN_EXTERN pn_sasl_outcome_t pn_sasl_outcome(pn_sasl_t *sasl);

/** Retrieve the authenticated user
 *
 * This is usually used at the the server end to find the name of the authenticated user.
 * On the client it will merely return whatever user was passed in to the
 * pn_transport_set_user_password() API.
 *
 * If pn_sasl_outcome() returns a value other than PN_SASL_OK, then there will be no user to return.
 * The returned value is only reliable after the PN_TRANSPORT_AUTHENTICATED event has been received.
 *
 * @param[in] sasl the sasl layer
 *
 * @return
 * If the SASL layer was not negotiated then 0 is returned
 * If the ANONYMOUS mechanism is used then the user will be "anonymous"
 * Otherwise a string containing the user is returned.
 */
PN_EXTERN const char *pn_sasl_get_user(pn_sasl_t *sasl);

/** Return the selected SASL mechanism
 *
 * The returned value is only reliable after the PN_TRANSPORT_AUTHENTICATED event has been received.
 *
 * @param[in] sasl the SASL layer
 *
 * @return The authentication mechanism selected by the SASL layer
 */
PN_EXTERN const char *pn_sasl_get_mech(pn_sasl_t *sasl);

/** SASL mechanisms that are to be considered for authentication
 *
 * This can be used on either the client or the server to restrict the SASL
 * mechanisms that may be used to the mechanisms on the list.
 *
 * @param[in] sasl the SASL layer
 * @param[in] mechs space separated list of mechanisms that are allowed for authentication
 */
PN_EXTERN void pn_sasl_allowed_mechs(pn_sasl_t *sasl, const char *mechs);

/** Boolean to allow use of clear text authentication mechanisms
 *
 * By default the SASL layer is configured not to allow mechanisms that disclose
 * the clear text of the password over an unencrypted AMQP connection. This specifically
 * will disallow the use of the PLAIN mechanism without using SSL encryption.
 *
 * This default is to avoid disclosing password information accidentally over an
 * insecure network.
 *
 * If you actually wish to use a clear text password unencrypted then you can use this
 * API to set allow_insecure_mechs to true.
 *
 * @param[in] sasl the SASL layer
 * @param[in] insecure set this to true to allow unencrypted PLAIN authentication.
 *
 */
PN_EXTERN void pn_sasl_set_allow_insecure_mechs(pn_sasl_t *sasl, bool insecure);

/** Return the current value for allow_insecure_mechs
 *
 * @param[in] sasl the SASL layer
 */
PN_EXTERN bool pn_sasl_get_allow_insecure_mechs(pn_sasl_t *sasl);

/**
 * Set the sasl configuration name
 *
 * This is used to construct the SASL configuration filename. In the current implementation
 * it ".conf" is added to the name and the file is looked for in the configuration directory.
 *
 * If not set it will default to "proton-server" for a sasl server and "proton-client"
 * for a client.
 *
 * @param[in] sasl the SASL layer
 * @param[in] name the configuration name
 */
PN_EXTERN void pn_sasl_config_name(pn_sasl_t *sasl, const char *name);

/**
 * Set the sasl configuration path
 *
 * This is used to tell SASL where to look for the configuration file.
 * In the current implementation it can be a colon separated list of directories.
 *
 * The environment variable PN_SASL_CONFIG_PATH can also be used to set this path,
 * but if both methods are used then this pn_sasl_config_path() will take precedence.
 *
 * If not set the underlying implementation default will be used.
 * for a client.
 *
 * @param[in] sasl the SASL layer
 * @param[in] path the configuration path
 */
PN_EXTERN void pn_sasl_config_path(pn_sasl_t *sasl, const char *path);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* sasl.h */
