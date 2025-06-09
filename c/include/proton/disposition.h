#ifndef PROTON_DISPOSITION_H
#define PROTON_DISPOSITION_H 1

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
#include <proton/condition.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * @copybrief pn_disposition_t
 *
 * @addtogroup delivery
 * @{
 */

/**
 * A delivery state.
 *
 * Dispositions record the current state or final outcome of a
 * transfer. Every delivery contains both a local and remote
 * disposition. The local disposition holds the local state of the
 * delivery, and the remote disposition holds the last known remote
 * state of the delivery.
 */
typedef struct pn_disposition_t pn_disposition_t;

/**
 * The PN_RECEIVED delivery state is a non terminal state indicating
 * how much (if any) message data has been received for a delivery.
 */
#define PN_RECEIVED (0x0000000000000023)

/**
 * The PN_ACCEPTED delivery state is a terminal state indicating that
 * the delivery was successfully processed. Once in this state there
 * will be no further state changes prior to the delivery being
 * settled.
 */
#define PN_ACCEPTED (0x0000000000000024)

/**
 * The PN_REJECTED delivery state is a terminal state indicating that
 * the delivery could not be processed due to some error condition.
 * Once in this state there will be no further state changes prior to
 * the delivery being settled.
 */
#define PN_REJECTED (0x0000000000000025)

/**
 * The PN_RELEASED delivery state is a terminal state indicating that
 * the delivery is being returned to the sender. Once in this state
 * there will be no further state changes prior to the delivery being
 * settled.
 */
#define PN_RELEASED (0x0000000000000026)

/**
 * The PN_MODIFIED delivery state is a terminal state indicating that
 * the delivery is being returned to the sender and should be
 * annotated by the sender prior to further delivery attempts. Once in
 * this state there will be no further state changes prior to the
 * delivery being settled.
 */
#define PN_MODIFIED (0x0000000000000027)

/**
 * The PN_DECLARED delivery state is a terminal state
 * indicating that a transaction has been declared and indicating its
 * transaction identifier.
 */
#define PN_DECLARED (0x0000000000000033)

/**
 * The PN_TRANSACTIONAL_STATE delivery state is a non terminal state
 * indicating the transactional state of a delivery.
 */
#define PN_TRANSACTIONAL_STATE (0x0000000000000034)

/**
 * Get the type of a disposition.
 *
 * Defined values are:
 *
 *  - ::PN_RECEIVED
 *  - ::PN_ACCEPTED
 *  - ::PN_REJECTED
 *  - ::PN_RELEASED
 *  - ::PN_MODIFIED
 *
 * @param[in] disposition a disposition object
 * @return the type of the disposition
 */
PN_EXTERN uint64_t pn_disposition_type(pn_disposition_t *disposition);

/**
 * Name of a disposition type for logging and debugging: "received", "accepted" etc.
 */
PN_EXTERN const char *pn_disposition_type_name(uint64_t disposition_type);

/**
 * Access the condition object associated with a disposition.
 *
 * The ::pn_condition_t object retrieved by this operation may be
 * modified prior to updating a delivery. When a delivery is updated,
 * the condition described by the disposition is reported to the peer
 * if applicable to the current delivery state, e.g. states such as
 * ::PN_REJECTED.
 *
 * The pointer returned by this operation is valid until the parent
 * delivery is settled.
 *
 * @param[in] disposition a disposition object
 * @return a pointer to the disposition condition
 */
PN_EXTERN pn_condition_t *pn_disposition_condition(pn_disposition_t *disposition);

/**
 * Access the disposition as a raw pn_data_t.
 *
 * Dispositions are an extension point in the AMQP protocol. The
 * disposition interface provides setters/getters for those
 * dispositions that are predefined by the specification, however
 * access to the raw disposition data is provided so that other
 * dispositions can be used.
 *
 * The ::pn_data_t pointer returned by this operation is valid until
 * the parent delivery is settled.
 *
 * @param[in] disposition a disposition object
 * @return a pointer to the raw disposition data
 */
PN_EXTERN pn_data_t *pn_disposition_data(pn_disposition_t *disposition);

/**
 * Get the section number associated with a disposition.
 *
 * @param[in] disposition a disposition object
 * @return a section number
 */
PN_EXTERN uint32_t pn_disposition_get_section_number(pn_disposition_t *disposition);

/**
 * Set the section number associated with a disposition.
 *
 * @param[in] disposition a disposition object
 * @param[in] section_number a section number
 */
PN_EXTERN void pn_disposition_set_section_number(pn_disposition_t *disposition, uint32_t section_number);

/**
 * Get the section offset associated with a disposition.
 *
 * @param[in] disposition a disposition object
 * @return a section offset
 */
PN_EXTERN uint64_t pn_disposition_get_section_offset(pn_disposition_t *disposition);

/**
 * Set the section offset associated with a disposition.
 *
 * @param[in] disposition a disposition object
 * @param[in] section_offset a section offset
 */
PN_EXTERN void pn_disposition_set_section_offset(pn_disposition_t *disposition, uint64_t section_offset);

/**
 * Check if a disposition has the failed flag set.
 *
 * @param[in] disposition a disposition object
 * @return true if the disposition has the failed flag set, false otherwise
 */
PN_EXTERN bool pn_disposition_is_failed(pn_disposition_t *disposition);

/**
 * Set the failed flag on a disposition.
 *
 * @param[in] disposition a disposition object
 * @param[in] failed the value of the failed flag
 */
PN_EXTERN void pn_disposition_set_failed(pn_disposition_t *disposition, bool failed);

/**
 * Check if a disposition has the undeliverable flag set.
 *
 * @param[in] disposition a disposition object
 * @return true if the disposition has the undeliverable flag set, false otherwise
 */
PN_EXTERN bool pn_disposition_is_undeliverable(pn_disposition_t *disposition);

/**
 * Set the undeliverable flag on a disposition.
 *
 * @param[in] disposition a disposition object
 * @param[in] undeliverable the value of the undeliverable flag
 */
PN_EXTERN void pn_disposition_set_undeliverable(pn_disposition_t *disposition, bool undeliverable);

/**
 * Access the annotations associated with a disposition.
 *
 * The ::pn_data_t object retrieved by this operation may be modified
 * prior to updating a delivery. When a delivery is updated, the
 * annotations described by the ::pn_data_t are reported to the peer
 * if applicable to the current delivery state, e.g. states such as
 * ::PN_MODIFIED. The ::pn_data_t must be empty or contain a symbol
 * keyed map.
 *
 * The pointer returned by this operation is valid until the parent
 * delivery is settled.
 *
 * @param[in] disposition a disposition object
 * @return the annotations associated with the disposition
 */
PN_EXTERN pn_data_t *pn_disposition_annotations(pn_disposition_t *disposition);

/**
 * A received delivery disposition
 *
 * This represents the non terminal delivery received state.
 */
typedef struct pn_received_disposition_t pn_received_disposition_t;

/**
 * A rejected delivery disposition
 *
 * This represents the terminal delivery rejected state.
 */
typedef struct pn_rejected_disposition_t pn_rejected_disposition_t;

/**
 * A modified delivery disposition
 *
 * This represents the terminal delivery modified state.
 */
typedef struct pn_modified_disposition_t pn_modified_disposition_t;

/**
 * A transaction declared delivery disposition
 *
 * This represents a transaction declared disposition.
 */
typedef struct pn_declared_disposition_t pn_declared_disposition_t;

/**
 * A transactional delivery disposition
 *
 * This represents transactional delivery state.
 */
typedef struct pn_transactional_disposition_t pn_transactional_disposition_t;

/**
 * A custom delivery disposition
 *
 * This can be used to represent a disposition that is not one of the predefined
 * AMQP delivery dispositions.
 */
typedef struct pn_custom_disposition_t pn_custom_disposition_t;

/**
 * Convert a delivery disposition to a custom disposition
 *
 * @param[in] disposition delivery disposition object
 * @return a pointer to the equivalent custom disposition type
 */
PN_EXTERN pn_custom_disposition_t *pn_custom_disposition(pn_disposition_t *disposition);

/**
 * Convert a delivery disposition to a received disposition
 *
 * @param[in] disposition delivery disposition object
 * @return a pointer to the received disposition or NULL
 * if the disposition is not that type
 */
PN_EXTERN pn_received_disposition_t *pn_received_disposition(pn_disposition_t *disposition);

/**
 * Convert a delivery disposition to a rejected disposition
 *
 * @param[in] disposition delivery disposition object
 * @return a pointer to the rejected disposition or NULL
 * if the disposition is not that type
 */
PN_EXTERN pn_rejected_disposition_t *pn_rejected_disposition(pn_disposition_t *disposition);

/**
 * Convert a delivery disposition to a modified disposition
 *
 * @param[in] disposition delivery disposition object
 * @return a pointer to the modified disposition or NULL
 * if the disposition is not that type
 */
PN_EXTERN pn_modified_disposition_t *pn_modified_disposition(pn_disposition_t *disposition);

/**
 * Convert a delivery disposition to a transaction declared disposition
 *
 * @param[in] disposition delivery disposition object
 * @return a pointer to the transaction declared disposition or NULL
 * if the disposition is not that type
 */
PN_EXTERN pn_declared_disposition_t *pn_declared_disposition(pn_disposition_t *disposition);

/**
 * Convert a delivery disposition to a transactional disposition
 *
 * @param[in] disposition delivery disposition object
 * @return a pointer to the transactional disposition or NULL
 * if the disposition is not that type
 */
PN_EXTERN pn_transactional_disposition_t *pn_transactional_disposition(pn_disposition_t *disposition);

/**
 * Access the disposition as a raw pn_data_t.
 *
 * Dispositions are an extension point in the AMQP protocol. The
 * disposition interface provides setters/getters for those
 * dispositions that are predefined by the specification, however
 * access to the raw disposition data is provided so that other
 * dispositions can be used.
 *
 * The ::pn_data_t pointer returned by this operation is valid until
 * the parent delivery is settled.
 *
 * @param[in] disposition a custom disposition object
 * @return a pointer to the raw disposition data
 */
PN_EXTERN pn_data_t *pn_custom_disposition_data(pn_custom_disposition_t *disposition);

/**
 * Get the type of a custom disposition.
 *
 * @param[in] disposition a custom disposition object
 * @return the type of the disposition
 */
PN_EXTERN uint64_t pn_custom_disposition_get_type(pn_custom_disposition_t *disposition);

/**
 * Set the type of a custom disposition.
 *
 * @param[in] disposition a custom disposition object
 * @param[in] type the type of the disposition
 */
PN_EXTERN void pn_custom_disposition_set_type(pn_custom_disposition_t *disposition, uint64_t type);

/**
 * Access the condition object associated with a rejected disposition.
 *
 * The ::pn_condition_t object retrieved by this operation may be
 * modified prior to updating a delivery. When a delivery is updated,
 * the condition described by the disposition is reported to the peer.
 *
 * The pointer returned by this operation is valid until the parent
 * delivery is settled.
 *
 * @param[in] disposition a disposition object
 * @return a pointer to the disposition condition
 */
PN_EXTERN pn_condition_t *pn_rejected_disposition_condition(pn_rejected_disposition_t *disposition);

/**
 * Get the section number associated with a received disposition.
 *
 * @param[in] disposition a disposition object
 * @return a section number
 */
PN_EXTERN uint32_t pn_received_disposition_get_section_number(pn_received_disposition_t *disposition);

/**
 * Set the section number associated with a received disposition.
 *
 * @param[in] disposition a disposition object
 * @param[in] section_number a section number
 */
PN_EXTERN void pn_received_disposition_set_section_number(pn_received_disposition_t *disposition, uint32_t section_number);

/**
 * Get the section offset associated with a received disposition.
 *
 * @param[in] disposition a disposition object
 * @return a section offset
 */
PN_EXTERN uint64_t pn_received_disposition_get_section_offset(pn_received_disposition_t *disposition);

/**
 * Set the section offset associated with a received disposition.
 *
 * @param[in] disposition a disposition object
 * @param[in] section_offset a section offset
 */
PN_EXTERN void pn_received_disposition_set_section_offset(pn_received_disposition_t *disposition, uint64_t section_offset);

/**
 * Check if a modified disposition has the failed flag set.
 *
 * @param[in] disposition a disposition object
 * @return true if the disposition has the failed flag set, false otherwise
 */
PN_EXTERN bool pn_modified_disposition_is_failed(pn_modified_disposition_t *disposition);

/**
 * Set the failed flag on a modified disposition.
 *
 * @param[in] disposition a disposition object
 * @param[in] failed the value of the failed flag
 */
PN_EXTERN void pn_modified_disposition_set_failed(pn_modified_disposition_t *disposition, bool failed);

/**
 * Check if a modified disposition has the undeliverable flag set.
 *
 * @param[in] disposition a disposition object
 * @return true if the disposition has the undeliverable flag set, false otherwise
 */
PN_EXTERN bool pn_modified_disposition_is_undeliverable(pn_modified_disposition_t *disposition);

/**
 * Set the undeliverable flag on a modified disposition.
 *
 * @param[in] disposition a disposition object
 * @param[in] undeliverable the value of the undeliverable flag
 */
PN_EXTERN void pn_modified_disposition_set_undeliverable(pn_modified_disposition_t *disposition, bool undeliverable);

/**
 * Access the annotations associated with a modified disposition.
 *
 * The ::pn_data_t object retrieved by this operation may be modified
 * prior to updating a delivery. When a delivery is updated, the
 * annotations described by the ::pn_data_t are reported to the peer.
 * The ::pn_data_t must be empty or contain a symbol keyed map.
 *
 * The pointer returned by this operation is valid until the parent
 * delivery is settled.
 *
 * @param[in] disposition a disposition object
 * @return the annotations associated with the disposition
 */
PN_EXTERN pn_data_t *pn_modified_disposition_annotations(pn_modified_disposition_t *disposition);

/**
 * Get the transaction id for a transaction declared disposition
 *
 * @param[in] disposition a transaction declared disposition object
 * @return the transaction id
 */
PN_EXTERN pn_bytes_t pn_declared_disposition_get_id(pn_declared_disposition_t *disposition);

/**
 * Set the transaction id for a transaction declared disposition
 *
 * @param[in] disposition a transactional disposition object
 * @param[in] id the transaction id
 */
PN_EXTERN void pn_declared_disposition_set_id(pn_declared_disposition_t *disposition, pn_bytes_t id);

/**
 * Get the transaction id for a transaction declared disposition
 *
 * @param[in] disposition a transactional disposition object
 * @return the transaction id
 */
PN_EXTERN pn_bytes_t pn_transactional_disposition_get_id(pn_transactional_disposition_t *disposition);

/**
 * Set the transaction id for a transactional disposition
 *
 * @param[in] disposition a transactional disposition object
 * @param[in] id the transaction id
 */
PN_EXTERN void pn_transactional_disposition_set_id(pn_transactional_disposition_t *disposition, pn_bytes_t id);

/**
 * Get the provisional outcome of the delivery if the transaction is committed successfully.
 *
 * @param[in] disposition a transactional disposition object
 * @return the provisional outcome of the transaction
 */
PN_EXTERN uint64_t pn_transactional_disposition_get_outcome_type(pn_transactional_disposition_t *disposition);

/**
 * Set the provisional outcome of the del;ivery if the transaction is committed successfully.
 *
 * Only terminal disposition states are allowed (::PN_ACCEPTED, ::PN_REJECTED, ::PN_RELEASED, ::PN_MODIFIED)
 *
 * @param[in] disposition a transactional disposition object
 * @param[in] outcome the provisional outcome of the transaction
 */
PN_EXTERN void pn_transactional_disposition_set_outcome_type(pn_transactional_disposition_t *disposition, uint64_t outcome);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* disposition.h */
