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

typedef struct pn_bytes_t
{
  size_t size;
  const char *start;
} pn_bytes_t;
typedef uint32_t pn_char_t;
typedef struct pn_collector_t pn_collector_t;
typedef struct pn_condition_t pn_condition_t;
typedef struct pn_connection_t pn_connection_t;
typedef struct pn_data_t pn_data_t;
typedef struct 
{
  char bytes[16];
} pn_decimal128_t;
typedef uint32_t pn_decimal32_t;
typedef uint64_t pn_decimal64_t;
typedef struct pn_delivery_t pn_delivery_t;
typedef pn_bytes_t pn_delivery_tag_t;
typedef struct pn_disposition_t pn_disposition_t;
typedef enum 
{
  PN_DIST_MODE_UNSPECIFIED = 0,
  PN_DIST_MODE_COPY = 1,
  PN_DIST_MODE_MOVE = 2
} pn_distribution_mode_t;
typedef enum 
{
  PN_NONDURABLE = 0,
  PN_CONFIGURATION = 1,
  PN_DELIVERIES = 2
} pn_durability_t;
typedef struct pn_error_t pn_error_t;
typedef struct pn_event_t pn_event_t;
typedef enum 
{
  PN_EVENT_NONE = 0,
  PN_REACTOR_INIT,
  PN_REACTOR_QUIESCED,
  PN_REACTOR_FINAL,
  PN_TIMER_TASK,
  PN_CONNECTION_INIT,
  PN_CONNECTION_BOUND,
  PN_CONNECTION_UNBOUND,
  PN_CONNECTION_LOCAL_OPEN,
  PN_CONNECTION_REMOTE_OPEN,
  PN_CONNECTION_LOCAL_CLOSE,
  PN_CONNECTION_REMOTE_CLOSE,
  PN_CONNECTION_FINAL,
  PN_SESSION_INIT,
  PN_SESSION_LOCAL_OPEN,
  PN_SESSION_REMOTE_OPEN,
  PN_SESSION_LOCAL_CLOSE,
  PN_SESSION_REMOTE_CLOSE,
  PN_SESSION_FINAL,
  PN_LINK_INIT,
  PN_LINK_LOCAL_OPEN,
  PN_LINK_REMOTE_OPEN,
  PN_LINK_LOCAL_CLOSE,
  PN_LINK_REMOTE_CLOSE,
  PN_LINK_LOCAL_DETACH,
  PN_LINK_REMOTE_DETACH,
  PN_LINK_FLOW,
  PN_LINK_FINAL,
  PN_DELIVERY,
  PN_TRANSPORT,
  PN_TRANSPORT_AUTHENTICATED,
  PN_TRANSPORT_ERROR,
  PN_TRANSPORT_HEAD_CLOSED,
  PN_TRANSPORT_TAIL_CLOSED,
  PN_TRANSPORT_CLOSED,
  PN_SELECTABLE_INIT,
  PN_SELECTABLE_UPDATED,
  PN_SELECTABLE_READABLE,
  PN_SELECTABLE_WRITABLE,
  PN_SELECTABLE_ERROR,
  PN_SELECTABLE_EXPIRED,
  PN_SELECTABLE_FINAL,
  PN_CONNECTION_WAKE,
  PN_LISTENER_ACCEPT,
  PN_LISTENER_CLOSE,
  PN_PROACTOR_INTERRUPT,
  PN_PROACTOR_TIMEOUT,
  PN_PROACTOR_INACTIVE,
  PN_LISTENER_OPEN,
  PN_RAW_CONNECTION_CONNECTED,
  PN_RAW_CONNECTION_CLOSED_READ,
  PN_RAW_CONNECTION_CLOSED_WRITE,
  PN_RAW_CONNECTION_DISCONNECTED,
  PN_RAW_CONNECTION_NEED_READ_BUFFERS,
  PN_RAW_CONNECTION_NEED_WRITE_BUFFERS,
  PN_RAW_CONNECTION_READ,
  PN_RAW_CONNECTION_WRITTEN,
  PN_RAW_CONNECTION_WAKE,
  PN_RAW_CONNECTION_DRAIN_BUFFERS
} pn_event_type_t;
typedef enum 
{
  PN_EXPIRE_WITH_LINK,
  PN_EXPIRE_WITH_SESSION,
  PN_EXPIRE_WITH_CONNECTION,
  PN_EXPIRE_NEVER
} pn_expiry_policy_t;
typedef struct pn_link_t pn_link_t;
typedef struct pn_message_t pn_message_t;
typedef uint32_t pn_millis_t;
typedef enum
{
  PN_RCV_FIRST = 0,
  PN_RCV_SECOND = 1
} pn_rcv_settle_mode_t;
typedef struct pn_record_t pn_record_t;
typedef enum 
{
  PN_SASL_NONE = -1,
  PN_SASL_OK = 0,
  PN_SASL_AUTH = 1,
  PN_SASL_SYS = 2,
  PN_SASL_PERM = 3,
  PN_SASL_TEMP = 4
} pn_sasl_outcome_t;
typedef struct pn_sasl_t pn_sasl_t;
typedef uint32_t pn_seconds_t;
typedef uint32_t pn_sequence_t;
typedef struct pn_session_t pn_session_t;
typedef enum 
{
  PN_SND_UNSETTLED = 0,
  PN_SND_SETTLED = 1,
  PN_SND_MIXED = 2
} pn_snd_settle_mode_t;
typedef enum 
{
  PN_SSL_CERT_SUBJECT_COUNTRY_NAME,
  PN_SSL_CERT_SUBJECT_STATE_OR_PROVINCE,
  PN_SSL_CERT_SUBJECT_CITY_OR_LOCALITY,
  PN_SSL_CERT_SUBJECT_ORGANIZATION_NAME,
  PN_SSL_CERT_SUBJECT_ORGANIZATION_UNIT,
  PN_SSL_CERT_SUBJECT_COMMON_NAME
} pn_ssl_cert_subject_subfield;
typedef struct pn_ssl_domain_t pn_ssl_domain_t;
typedef enum 
{
  PN_SSL_SHA1,
  PN_SSL_SHA256,
  PN_SSL_SHA512,
  PN_SSL_MD5
} pn_ssl_hash_alg;
typedef enum 
{
  PN_SSL_MODE_CLIENT = 1,
  PN_SSL_MODE_SERVER
} pn_ssl_mode_t;
typedef enum 
{
  PN_SSL_RESUME_UNKNOWN,
  PN_SSL_RESUME_NEW,
  PN_SSL_RESUME_REUSED
} pn_ssl_resume_status_t;
typedef struct pn_ssl_t pn_ssl_t;
typedef enum 
{
  PN_SSL_VERIFY_NULL = 0,
  PN_SSL_VERIFY_PEER,
  PN_SSL_ANONYMOUS_PEER,
  PN_SSL_VERIFY_PEER_NAME
} pn_ssl_verify_mode_t;
typedef int pn_state_t;
typedef struct pn_terminus_t pn_terminus_t;
typedef enum 
{
  PN_UNSPECIFIED = 0,
  PN_SOURCE = 1,
  PN_TARGET = 2,
  PN_COORDINATOR = 3
} pn_terminus_type_t;
typedef int64_t pn_timestamp_t;
typedef int pn_trace_t;
typedef struct pn_transport_t pn_transport_t;
typedef enum 
{
  PN_NULL = 1,
  PN_BOOL = 2,
  PN_UBYTE = 3,
  PN_BYTE = 4,
  PN_USHORT = 5,
  PN_SHORT = 6,
  PN_UINT = 7,
  PN_INT = 8,
  PN_CHAR = 9,
  PN_ULONG = 10,
  PN_LONG = 11,
  PN_TIMESTAMP = 12,
  PN_FLOAT = 13,
  PN_DOUBLE = 14,
  PN_DECIMAL32 = 15,
  PN_DECIMAL64 = 16,
  PN_DECIMAL128 = 17,
  PN_UUID = 18,
  PN_BINARY = 19,
  PN_STRING = 20,
  PN_SYMBOL = 21,
  PN_DESCRIBED = 22,
  PN_ARRAY = 23,
  PN_LIST = 24,
  PN_MAP = 25,
  PN_INVALID = -1
} pn_type_t;
typedef struct 
{
  char bytes[16];
} pn_uuid_t;
typedef struct {
  pn_type_t type;
  union {
    _Bool as_bool;
    uint8_t as_ubyte;
    int8_t as_byte;
    uint16_t as_ushort;
    int16_t as_short;
    uint32_t as_uint;
    int32_t as_int;
    pn_char_t as_char;
    uint64_t as_ulong;
    int64_t as_long;
    pn_timestamp_t as_timestamp;
    float as_float;
    double as_double;
    pn_decimal32_t as_decimal32;
    pn_decimal64_t as_decimal64;
    pn_decimal128_t as_decimal128;
    pn_uuid_t as_uuid;
    pn_bytes_t as_bytes;
  } u;
} pn_atom_t;
typedef pn_atom_t pn_msgid_t;
typedef void (*pn_tracer_t)(pn_transport_t *transport, const char *message);

pn_collector_t *pn_collector(void);
void pn_collector_free(pn_collector_t *collector);
_Bool pn_collector_more(pn_collector_t *collector);
pn_event_t *pn_collector_peek(pn_collector_t *collector);
_Bool pn_collector_pop(pn_collector_t *collector);
void pn_collector_release(pn_collector_t *collector);

void pn_condition_clear(pn_condition_t *condition);
const char *pn_condition_get_description(pn_condition_t *condition);
const char *pn_condition_get_name(pn_condition_t *condition);
pn_data_t *pn_condition_info(pn_condition_t *condition);
_Bool pn_condition_is_set(pn_condition_t *condition);
int pn_condition_set_description(pn_condition_t *condition, const char *description);
int pn_condition_set_name(pn_condition_t *condition, const char *name);

pn_connection_t *pn_connection(void);
pn_record_t *pn_connection_attachments(pn_connection_t *connection);
void pn_connection_close(pn_connection_t *connection);
void pn_connection_collect(pn_connection_t *connection, pn_collector_t *collector);
pn_condition_t *pn_connection_condition(pn_connection_t *connection);
pn_data_t *pn_connection_desired_capabilities(pn_connection_t *connection);
pn_error_t *pn_connection_error(pn_connection_t *connection);
const char *pn_connection_get_authorization(pn_connection_t *connection);
const char *pn_connection_get_container(pn_connection_t *connection);
const char *pn_connection_get_hostname(pn_connection_t *connection);
const char *pn_connection_get_user(pn_connection_t *connection);
pn_data_t *pn_connection_offered_capabilities(pn_connection_t *connection);
void pn_connection_open(pn_connection_t *connection);
pn_data_t *pn_connection_properties(pn_connection_t *connection);
void pn_connection_release(pn_connection_t *connection);
pn_condition_t *pn_connection_remote_condition(pn_connection_t *connection);
const char *pn_connection_remote_container(pn_connection_t *connection);
pn_data_t *pn_connection_remote_desired_capabilities(pn_connection_t *connection);
const char *pn_connection_remote_hostname(pn_connection_t *connection);
pn_data_t *pn_connection_remote_offered_capabilities(pn_connection_t *connection);
pn_data_t *pn_connection_remote_properties(pn_connection_t *connection);
void pn_connection_set_authorization(pn_connection_t *connection, const char *authzid);
void pn_connection_set_container(pn_connection_t *connection, const char *container);
void pn_connection_set_hostname(pn_connection_t *connection, const char *hostname);
void pn_connection_set_password(pn_connection_t *connection, const char *password);
void pn_connection_set_user(pn_connection_t *connection, const char *user);
pn_state_t pn_connection_state(pn_connection_t *connection);
pn_transport_t *pn_connection_transport(pn_connection_t *connection);

pn_data_t *pn_data(size_t capacity);
void pn_data_clear(pn_data_t *data);
int pn_data_copy(pn_data_t *data, pn_data_t *src);
ssize_t pn_data_decode(pn_data_t *data, const char *bytes, size_t size);
void pn_data_dump(pn_data_t *data);
ssize_t pn_data_encode(pn_data_t *data, char *bytes, size_t size);
ssize_t pn_data_encoded_size(pn_data_t *data);
_Bool pn_data_enter(pn_data_t *data);
pn_error_t *pn_data_error(pn_data_t *data);
_Bool pn_data_exit(pn_data_t *data);
void pn_data_free(pn_data_t *data);
size_t pn_data_get_array(pn_data_t *data);
pn_type_t pn_data_get_array_type(pn_data_t *data);
pn_bytes_t pn_data_get_binary(pn_data_t *data);
_Bool pn_data_get_bool(pn_data_t *data);
int8_t pn_data_get_byte(pn_data_t *data);
pn_char_t pn_data_get_char(pn_data_t *data);
pn_decimal128_t pn_data_get_decimal128(pn_data_t *data);
pn_decimal32_t pn_data_get_decimal32(pn_data_t *data);
pn_decimal64_t pn_data_get_decimal64(pn_data_t *data);
double pn_data_get_double(pn_data_t *data);
float pn_data_get_float(pn_data_t *data);
int32_t pn_data_get_int(pn_data_t *data);
size_t pn_data_get_list(pn_data_t *data);
int64_t pn_data_get_long(pn_data_t *data);
size_t pn_data_get_map(pn_data_t *data);
int16_t pn_data_get_short(pn_data_t *data);
pn_bytes_t pn_data_get_string(pn_data_t *data);
pn_bytes_t pn_data_get_symbol(pn_data_t *data);
pn_timestamp_t pn_data_get_timestamp(pn_data_t *data);
uint8_t pn_data_get_ubyte(pn_data_t *data);
uint32_t pn_data_get_uint(pn_data_t *data);
uint64_t pn_data_get_ulong(pn_data_t *data);
uint16_t pn_data_get_ushort(pn_data_t *data);
pn_uuid_t pn_data_get_uuid(pn_data_t *data);
_Bool pn_data_is_array_described(pn_data_t *data);
_Bool pn_data_is_described(pn_data_t *data);
_Bool pn_data_is_null(pn_data_t *data);
_Bool pn_data_lookup(pn_data_t *data, const char *name);
void pn_data_narrow(pn_data_t *data);
_Bool pn_data_next(pn_data_t *data);
_Bool pn_data_prev(pn_data_t *data);
int pn_data_put_array(pn_data_t *data, _Bool described, pn_type_t type);
int pn_data_put_binary(pn_data_t *data, pn_bytes_t bytes);
int pn_data_put_bool(pn_data_t *data, _Bool b);
int pn_data_put_byte(pn_data_t *data, int8_t b);
int pn_data_put_char(pn_data_t *data, pn_char_t c);
int pn_data_put_decimal128(pn_data_t *data, pn_decimal128_t d);
int pn_data_put_decimal32(pn_data_t *data, pn_decimal32_t d);
int pn_data_put_decimal64(pn_data_t *data, pn_decimal64_t d);
int pn_data_put_described(pn_data_t *data);
int pn_data_put_double(pn_data_t *data, double d);
int pn_data_put_float(pn_data_t *data, float f);
int pn_data_put_int(pn_data_t *data, int32_t i);
int pn_data_put_list(pn_data_t *data);
int pn_data_put_long(pn_data_t *data, int64_t l);
int pn_data_put_map(pn_data_t *data);
int pn_data_put_null(pn_data_t *data);
int pn_data_put_short(pn_data_t *data, int16_t s);
int pn_data_put_string(pn_data_t *data, pn_bytes_t string);
int pn_data_put_symbol(pn_data_t *data, pn_bytes_t symbol);
int pn_data_put_timestamp(pn_data_t *data, pn_timestamp_t t);
int pn_data_put_ubyte(pn_data_t *data, uint8_t ub);
int pn_data_put_uint(pn_data_t *data, uint32_t ui);
int pn_data_put_ulong(pn_data_t *data, uint64_t ul);
int pn_data_put_ushort(pn_data_t *data, uint16_t us);
int pn_data_put_uuid(pn_data_t *data, pn_uuid_t u);
void pn_data_rewind(pn_data_t *data);
pn_type_t pn_data_type(pn_data_t *data);
void pn_data_widen(pn_data_t *data);

int pn_decref(void *object);
char *pn_tostring(void *object);

pn_delivery_t *pn_delivery(pn_link_t *link, pn_delivery_tag_t tag);
void pn_delivery_abort(pn_delivery_t *delivery);
_Bool pn_delivery_aborted(pn_delivery_t *delivery);
pn_record_t *pn_delivery_attachments(pn_delivery_t *delivery);
pn_link_t *pn_delivery_link(pn_delivery_t *delivery);
pn_disposition_t *pn_delivery_local(pn_delivery_t *delivery);
uint64_t pn_delivery_local_state(pn_delivery_t *delivery);
_Bool pn_delivery_partial(pn_delivery_t *delivery);
size_t pn_delivery_pending(pn_delivery_t *delivery);
_Bool pn_delivery_readable(pn_delivery_t *delivery);
pn_disposition_t *pn_delivery_remote(pn_delivery_t *delivery);
uint64_t pn_delivery_remote_state(pn_delivery_t *delivery);
void pn_delivery_settle(pn_delivery_t *delivery);
_Bool pn_delivery_settled(pn_delivery_t *delivery);
pn_delivery_tag_t pn_delivery_tag(pn_delivery_t *delivery);
void pn_delivery_update(pn_delivery_t *delivery, uint64_t state);
_Bool pn_delivery_updated(pn_delivery_t *delivery);
_Bool pn_delivery_writable(pn_delivery_t *delivery);

pn_data_t *pn_disposition_annotations(pn_disposition_t *disposition);
pn_condition_t *pn_disposition_condition(pn_disposition_t *disposition);
pn_data_t *pn_disposition_data(pn_disposition_t *disposition);
uint32_t pn_disposition_get_section_number(pn_disposition_t *disposition);
uint64_t pn_disposition_get_section_offset(pn_disposition_t *disposition);
_Bool pn_disposition_is_failed(pn_disposition_t *disposition);
_Bool pn_disposition_is_undeliverable(pn_disposition_t *disposition);
void pn_disposition_set_failed(pn_disposition_t *disposition, _Bool failed);
void pn_disposition_set_section_number(pn_disposition_t *disposition, uint32_t section_number);
void pn_disposition_set_section_offset(pn_disposition_t *disposition, uint64_t section_offset);
void pn_disposition_set_undeliverable(pn_disposition_t *disposition, _Bool undeliverable);
uint64_t pn_disposition_type(pn_disposition_t *disposition);

int pn_error_code(pn_error_t *error);
const char *pn_error_text(pn_error_t *error);

pn_connection_t *pn_event_connection(pn_event_t *event);
void *pn_event_context(pn_event_t *event);
pn_delivery_t *pn_event_delivery(pn_event_t *event);
pn_link_t *pn_event_link(pn_event_t *event);
pn_session_t *pn_event_session(pn_event_t *event);
pn_transport_t *pn_event_transport(pn_event_t *event);
pn_event_type_t pn_event_type(pn_event_t *event);
const char *pn_event_type_name(pn_event_type_t type);

void *pn_incref(void *object);

_Bool pn_link_advance(pn_link_t *link);
pn_record_t *pn_link_attachments(pn_link_t *link);
int pn_link_available(pn_link_t *link);
void pn_link_close(pn_link_t *link);
pn_condition_t *pn_link_condition(pn_link_t *link);
int pn_link_credit(pn_link_t *link);
pn_delivery_t *pn_link_current(pn_link_t *link);
void pn_link_detach(pn_link_t *link);
void pn_link_drain(pn_link_t *receiver, int credit);
int pn_link_drained(pn_link_t *link);
_Bool pn_link_draining(pn_link_t *receiver);
pn_error_t *pn_link_error(pn_link_t *link);
void pn_link_flow(pn_link_t *receiver, int credit);
void pn_link_free(pn_link_t *link);
_Bool pn_link_get_drain(pn_link_t *link);
pn_link_t *pn_link_head(pn_connection_t *connection, pn_state_t state);
_Bool pn_link_is_receiver(pn_link_t *link);
_Bool pn_link_is_sender(pn_link_t *link);
uint64_t pn_link_max_message_size(pn_link_t *link);
const char *pn_link_name(pn_link_t *link);
pn_link_t *pn_link_next(pn_link_t *link, pn_state_t state);
void pn_link_offered(pn_link_t *sender, int credit);
void pn_link_open(pn_link_t *link);
pn_data_t *pn_link_properties(pn_link_t *link);
int pn_link_queued(pn_link_t *link);
pn_rcv_settle_mode_t pn_link_rcv_settle_mode(pn_link_t *link);
ssize_t pn_link_recv(pn_link_t *receiver, char *bytes, size_t n);
pn_condition_t *pn_link_remote_condition(pn_link_t *link);
uint64_t pn_link_remote_max_message_size(pn_link_t *link);
pn_data_t *pn_link_remote_properties(pn_link_t *link);
pn_rcv_settle_mode_t pn_link_remote_rcv_settle_mode(pn_link_t *link);
pn_snd_settle_mode_t pn_link_remote_snd_settle_mode(pn_link_t *link);
pn_terminus_t *pn_link_remote_source(pn_link_t *link);
pn_terminus_t *pn_link_remote_target(pn_link_t *link);
ssize_t pn_link_send(pn_link_t *sender, const char *bytes, size_t n);
pn_session_t *pn_link_session(pn_link_t *link);
void pn_link_set_drain(pn_link_t *receiver, _Bool drain);
void pn_link_set_max_message_size(pn_link_t *link, uint64_t size);
void pn_link_set_rcv_settle_mode(pn_link_t *link, pn_rcv_settle_mode_t mode);
void pn_link_set_snd_settle_mode(pn_link_t *link, pn_snd_settle_mode_t mode);
pn_snd_settle_mode_t pn_link_snd_settle_mode(pn_link_t *link);
pn_terminus_t *pn_link_source(pn_link_t *link);
pn_state_t pn_link_state(pn_link_t *link);
pn_terminus_t *pn_link_target(pn_link_t *link);
int pn_link_unsettled(pn_link_t *link);

pn_message_t *pn_message(void);
pn_data_t *pn_message_annotations(pn_message_t *msg);
pn_data_t *pn_message_body(pn_message_t *msg);
void pn_message_clear(pn_message_t *msg);
int pn_message_decode(pn_message_t *msg, const char *bytes, size_t size);
pn_error_t *pn_message_error(pn_message_t *msg);
void pn_message_free(pn_message_t *msg);
const char *pn_message_get_address(pn_message_t *msg);
const char *pn_message_get_content_encoding(pn_message_t *msg);
const char *pn_message_get_content_type(pn_message_t *msg);
pn_msgid_t pn_message_get_correlation_id(pn_message_t *msg);
pn_timestamp_t pn_message_get_creation_time(pn_message_t *msg);
uint32_t pn_message_get_delivery_count(pn_message_t *msg);
pn_timestamp_t pn_message_get_expiry_time(pn_message_t *msg);
const char *pn_message_get_group_id(pn_message_t *msg);
pn_sequence_t pn_message_get_group_sequence(pn_message_t *msg);
pn_msgid_t pn_message_get_id(pn_message_t *msg);
uint8_t pn_message_get_priority(pn_message_t *msg);
const char *pn_message_get_reply_to(pn_message_t *msg);
const char *pn_message_get_reply_to_group_id(pn_message_t *msg);
const char *pn_message_get_subject(pn_message_t *msg);
pn_millis_t pn_message_get_ttl(pn_message_t *msg);
pn_bytes_t pn_message_get_user_id(pn_message_t *msg);
pn_data_t *pn_message_instructions(pn_message_t *msg);
_Bool pn_message_is_durable(pn_message_t *msg);
_Bool pn_message_is_first_acquirer(pn_message_t *msg);
_Bool pn_message_is_inferred(pn_message_t *msg);
pn_data_t *pn_message_properties(pn_message_t *msg);
int pn_message_set_address(pn_message_t *msg, const char *address);
int pn_message_set_content_encoding(pn_message_t *msg, const char *encoding);
int pn_message_set_content_type(pn_message_t *msg, const char *type);
int pn_message_set_correlation_id(pn_message_t *msg, pn_msgid_t id);
int pn_message_set_creation_time(pn_message_t *msg, pn_timestamp_t time);
int pn_message_set_delivery_count(pn_message_t *msg, uint32_t count);
int pn_message_set_durable(pn_message_t *msg, _Bool durable);
int pn_message_set_expiry_time(pn_message_t *msg, pn_timestamp_t time);
int pn_message_set_first_acquirer(pn_message_t *msg, _Bool first);
int pn_message_set_group_id(pn_message_t *msg, const char *group_id);
int pn_message_set_group_sequence(pn_message_t *msg, pn_sequence_t n);
int pn_message_set_id(pn_message_t *msg, pn_msgid_t id);
int pn_message_set_inferred(pn_message_t *msg, _Bool inferred);
int pn_message_set_priority(pn_message_t *msg, uint8_t priority);
int pn_message_set_reply_to(pn_message_t *msg, const char *reply_to);
int pn_message_set_reply_to_group_id(pn_message_t *msg, const char *reply_to_group_id);
int pn_message_set_subject(pn_message_t *msg, const char *subject);
int pn_message_set_ttl(pn_message_t *msg, pn_millis_t ttl);
int pn_message_set_user_id(pn_message_t *msg, pn_bytes_t user_id);

pn_link_t *pn_receiver(pn_session_t *session, const char *name);

pn_sasl_t *pn_sasl(pn_transport_t *transport);
void pn_sasl_allowed_mechs(pn_sasl_t *sasl, const char *mechs);
void pn_sasl_config_name(pn_sasl_t *sasl, const char *name);
void pn_sasl_config_path(pn_sasl_t *sasl, const char *path);
void pn_sasl_done(pn_sasl_t *sasl, pn_sasl_outcome_t outcome);
_Bool pn_sasl_extended(void);
_Bool pn_sasl_get_allow_insecure_mechs(pn_sasl_t *sasl);
const char *pn_sasl_get_authorization(pn_sasl_t *sasl);
const char *pn_sasl_get_mech(pn_sasl_t *sasl);
const char *pn_sasl_get_user(pn_sasl_t *sasl);
pn_sasl_outcome_t pn_sasl_outcome(pn_sasl_t *sasl);
void pn_sasl_set_allow_insecure_mechs(pn_sasl_t *sasl, _Bool insecure);

pn_link_t *pn_sender(pn_session_t *session, const char *name);

pn_session_t *pn_session(pn_connection_t *connection);
pn_record_t *pn_session_attachments(pn_session_t *session);
void pn_session_close(pn_session_t *session);
pn_condition_t *pn_session_condition(pn_session_t *session);
pn_connection_t *pn_session_connection(pn_session_t *session);
void pn_session_free(pn_session_t *session);
size_t pn_session_get_incoming_capacity(pn_session_t *session);
size_t pn_session_get_outgoing_window(pn_session_t *session);
pn_session_t *pn_session_head(pn_connection_t *connection, pn_state_t state);
size_t pn_session_incoming_bytes(pn_session_t *session);
pn_session_t *pn_session_next(pn_session_t *session, pn_state_t state);
void pn_session_open(pn_session_t *session);
size_t pn_session_outgoing_bytes(pn_session_t *session);
pn_condition_t *pn_session_remote_condition(pn_session_t *session);
void pn_session_set_incoming_capacity(pn_session_t *session, size_t capacity);
void pn_session_set_outgoing_window(pn_session_t *session, size_t window);
pn_state_t pn_session_state(pn_session_t *session);

pn_ssl_t *pn_ssl(pn_transport_t *transport);
pn_ssl_domain_t *pn_ssl_domain(pn_ssl_mode_t mode);
int pn_ssl_domain_allow_unsecured_client(pn_ssl_domain_t *domain);
void pn_ssl_domain_free(pn_ssl_domain_t *domain);
int pn_ssl_domain_set_ciphers(pn_ssl_domain_t *domain, const char *ciphers);
int pn_ssl_domain_set_credentials(pn_ssl_domain_t *domain, const char *credential_1, const char *credential_2, const char *password);
int pn_ssl_domain_set_peer_authentication(pn_ssl_domain_t *domain, const pn_ssl_verify_mode_t mode, const char *trusted_CAs);
int pn_ssl_domain_set_protocols(pn_ssl_domain_t *domain, const char *protocols);
int pn_ssl_domain_set_trusted_ca_db(pn_ssl_domain_t *domain, const char *certificate_db);
int pn_ssl_get_cert_fingerprint(pn_ssl_t *ssl0, char *fingerprint, size_t fingerprint_length, pn_ssl_hash_alg hash_alg);
_Bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *buffer, size_t size);
_Bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *buffer, size_t size);
const char *pn_ssl_get_remote_subject(pn_ssl_t *ssl);
const char *pn_ssl_get_remote_subject_subfield(pn_ssl_t *ssl0, pn_ssl_cert_subject_subfield field);
int pn_ssl_init(pn_ssl_t *ssl, pn_ssl_domain_t *domain, const char *session_id);
_Bool pn_ssl_present(void);
pn_ssl_resume_status_t pn_ssl_resume_status(pn_ssl_t *ssl);
int pn_ssl_set_peer_hostname(pn_ssl_t *ssl, const char *hostname);

pn_data_t *pn_terminus_capabilities(pn_terminus_t *terminus);
int pn_terminus_copy(pn_terminus_t *terminus, pn_terminus_t *src);
pn_data_t *pn_terminus_filter(pn_terminus_t *terminus);
const char *pn_terminus_get_address(pn_terminus_t *terminus);
pn_distribution_mode_t pn_terminus_get_distribution_mode(const pn_terminus_t *terminus);
pn_durability_t pn_terminus_get_durability(pn_terminus_t *terminus);
pn_expiry_policy_t pn_terminus_get_expiry_policy(pn_terminus_t *terminus);
pn_seconds_t pn_terminus_get_timeout(pn_terminus_t *terminus);
pn_terminus_type_t pn_terminus_get_type(pn_terminus_t *terminus);
_Bool pn_terminus_is_dynamic(pn_terminus_t *terminus);
pn_data_t *pn_terminus_outcomes(pn_terminus_t *terminus);
pn_data_t *pn_terminus_properties(pn_terminus_t *terminus);
int pn_terminus_set_address(pn_terminus_t *terminus, const char *address);
int pn_terminus_set_distribution_mode(pn_terminus_t *terminus, pn_distribution_mode_t mode);
int pn_terminus_set_durability(pn_terminus_t *terminus, pn_durability_t durability);
int pn_terminus_set_dynamic(pn_terminus_t *terminus, _Bool dynamic);
int pn_terminus_set_expiry_policy(pn_terminus_t *terminus, pn_expiry_policy_t policy);
int pn_terminus_set_timeout(pn_terminus_t *terminus, pn_seconds_t timeout);
int pn_terminus_set_type(pn_terminus_t *terminus, pn_terminus_type_t type);

pn_transport_t *pn_transport(void);
pn_record_t *pn_transport_attachments(pn_transport_t *transport);
int pn_transport_bind(pn_transport_t *transport, pn_connection_t *connection);
ssize_t pn_transport_capacity(pn_transport_t *transport);
int pn_transport_close_head(pn_transport_t *transport);
int pn_transport_close_tail(pn_transport_t *transport);
_Bool pn_transport_closed(pn_transport_t *transport);
pn_condition_t *pn_transport_condition(pn_transport_t *transport);
pn_connection_t *pn_transport_connection(pn_transport_t *transport);
pn_error_t *pn_transport_error(pn_transport_t *transport);
uint16_t pn_transport_get_channel_max(pn_transport_t *transport);
uint64_t pn_transport_get_frames_input(const pn_transport_t *transport);
uint64_t pn_transport_get_frames_output(const pn_transport_t *transport);
pn_millis_t pn_transport_get_idle_timeout(pn_transport_t *transport);
uint32_t pn_transport_get_max_frame(pn_transport_t *transport);
pn_millis_t pn_transport_get_remote_idle_timeout(pn_transport_t *transport);
uint32_t pn_transport_get_remote_max_frame(pn_transport_t *transport);
const char *pn_transport_get_user(pn_transport_t *transport);
_Bool pn_transport_is_authenticated(pn_transport_t *transport);
_Bool pn_transport_is_encrypted(pn_transport_t *transport);
void pn_transport_log(pn_transport_t *transport, const char *message);
ssize_t pn_transport_peek(pn_transport_t *transport, char *dst, size_t size);
ssize_t pn_transport_pending(pn_transport_t *transport);
void pn_transport_pop(pn_transport_t *transport, size_t size);
ssize_t pn_transport_push(pn_transport_t *transport, const char *src, size_t size);
uint16_t pn_transport_remote_channel_max(pn_transport_t *transport);
void pn_transport_require_auth(pn_transport_t *transport, _Bool required);
void pn_transport_require_encryption(pn_transport_t *transport, _Bool required);
int pn_transport_set_channel_max(pn_transport_t *transport, uint16_t channel_max);
void pn_transport_set_idle_timeout(pn_transport_t *transport, pn_millis_t timeout);
void pn_transport_set_max_frame(pn_transport_t *transport, uint32_t size);
void pn_transport_set_server(pn_transport_t *transport);
void pn_transport_set_tracer(pn_transport_t *transport, pn_tracer_t tracer);
int64_t pn_transport_tick(pn_transport_t *transport, int64_t now);
void pn_transport_trace(pn_transport_t *transport, pn_trace_t trace);
int pn_transport_unbind(pn_transport_t *transport);

// Dispositions defined in C macros
// results of pn_disposition_type
#define PN_RECEIVED ...
#define PN_ACCEPTED ...
#define PN_REJECTED ...
#define PN_RELEASED ...
#define PN_MODIFIED ...

// Default message priority
#define PN_DEFAULT_PRIORITY ...

// Returned errors
#define PN_OK ...
#define PN_EOS ...
#define PN_OVERFLOW ...
#define PN_TIMEOUT ...
#define PN_INTR ...

#define PN_LOCAL_UNINIT  ...
#define PN_LOCAL_ACTIVE  ...
#define PN_LOCAL_CLOSED  ...
#define PN_REMOTE_UNINIT ...
#define PN_REMOTE_ACTIVE ...
#define PN_REMOTE_CLOSED ...

#define PN_TRACE_OFF ...
#define PN_TRACE_RAW ...
#define PN_TRACE_FRM ...
#define PN_TRACE_DRV ...

// Maybe need to get this from cmake, or modify how the binding does this
#define PN_VERSION_MAJOR ...
#define PN_VERSION_MINOR ...
#define PN_VERSION_POINT ...

// Initialization of library - probably a better way to do this than explicitly, but it works!
void init();

pn_connection_t *pn_cast_pn_connection(void *x);
pn_session_t *pn_cast_pn_session(void *x);
pn_link_t *pn_cast_pn_link(void *x);
pn_delivery_t *pn_cast_pn_delivery(void *x);
pn_transport_t *pn_cast_pn_transport(void *x);

extern "Python" void pn_pyref_incref(void *object);
extern "Python" void pn_pyref_decref(void *object);
extern "Python" void pn_pytracer(pn_transport_t *transport, const char *message);

pn_event_t *pn_collector_put_py(pn_collector_t *collector, void *context, pn_event_type_t type);
ssize_t pn_data_format_py(pn_data_t *data, char *bytes, size_t size);
const char *pn_event_class_name_py(pn_event_t *event);
ssize_t pn_message_encode_py(pn_message_t *msg, char *bytes, size_t size);
void pn_record_def_py(pn_record_t *record);
void *pn_record_get_py(pn_record_t *record);
void pn_record_set_py(pn_record_t *record, void *value);
int pn_ssl_get_peer_hostname_py(pn_ssl_t *ssl, char *hostname, size_t size);

void free(void*);
