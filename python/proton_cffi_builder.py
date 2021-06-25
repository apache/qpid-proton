import argparse

from cffi import FFI

cstdlib = """
typedef struct { ...; } va_list;
"""

codec_h = """
typedef enum {
  PN_NULL = ...,
  PN_BOOL = ...,
  PN_UBYTE = ...,
  PN_BYTE = ...,
  PN_USHORT = ...,
  PN_SHORT = ...,
  PN_UINT = ...,
  PN_INT = ...,
  PN_CHAR = ...,
  PN_ULONG =  ...,
  PN_LONG =  ...,
  PN_TIMESTAMP =  ...,
  PN_FLOAT =  ...,
  PN_DOUBLE =  ...,
  PN_DECIMAL32 =  ...,
  PN_DECIMAL64 =  ...,
  PN_DECIMAL128 =  ...,
  PN_UUID =  ...,
  PN_BINARY =  ...,
  PN_STRING =  ...,
  PN_SYMBOL =  ...,
  PN_DESCRIBED =  ...,
  PN_ARRAY =  ...,
  PN_LIST =  ...,
  PN_MAP =  ...,
  PN_INVALID = ...
} pn_type_t;


typedef struct { ...; } pn_atom_t;

typedef struct pn_data_t pn_data_t;

const char *pn_type_name(pn_type_t type);
pn_data_t *pn_data(size_t capacity);
void pn_data_free(pn_data_t *data);
int pn_data_errno(pn_data_t *data);
pn_error_t * pn_data_error(pn_data_t *data);
int pn_data_vfill(pn_data_t *data, const char *fmt, va_list ap);
int pn_data_fill(pn_data_t *data, const char *fmt, ...);
int pn_data_vscan(pn_data_t *data, const char *fmt, va_list ap);
int pn_data_scan(pn_data_t *data, const char *fmt, ...);
void pn_data_clear(pn_data_t *data);
size_t pn_data_size(pn_data_t *data);
void pn_data_rewind(pn_data_t *data);
bool pn_data_next(pn_data_t *data);
bool pn_data_prev(pn_data_t *data);
bool pn_data_enter(pn_data_t *data);
bool pn_data_exit(pn_data_t *data);
bool pn_data_lookup(pn_data_t *data, const char *name);
pn_type_t pn_data_type(pn_data_t *data);
int pn_data_print(pn_data_t *data);
int pn_data_format(pn_data_t *data, char *bytes, size_t *size);
ssize_t pn_data_encode(pn_data_t *data, char *bytes, size_t size);
ssize_t pn_data_encoded_size(pn_data_t *data);
ssize_t pn_data_decode(pn_data_t *data, const char *bytes, size_t size);
int pn_data_put_list(pn_data_t *data);
int pn_data_put_map(pn_data_t *data);
int pn_data_put_array(pn_data_t *data, bool described, pn_type_t type);
int pn_data_put_described(pn_data_t *data);
int pn_data_put_null(pn_data_t *data);
int pn_data_put_bool(pn_data_t *data, bool b);
int pn_data_put_ubyte(pn_data_t *data, uint8_t ub);
int pn_data_put_byte(pn_data_t *data, int8_t b);
int pn_data_put_ushort(pn_data_t *data, uint16_t us);
int pn_data_put_short(pn_data_t *data, int16_t s);
int pn_data_put_uint(pn_data_t *data, uint32_t ui);
int pn_data_put_int(pn_data_t *data, int32_t i);
int pn_data_put_char(pn_data_t *data, pn_char_t c);
int pn_data_put_ulong(pn_data_t *data, uint64_t ul);
int pn_data_put_long(pn_data_t *data, int64_t l);
int pn_data_put_timestamp(pn_data_t *data, pn_timestamp_t t);
int pn_data_put_float(pn_data_t *data, float f);
int pn_data_put_double(pn_data_t *data, double d);
int pn_data_put_decimal32(pn_data_t *data, pn_decimal32_t d);
int pn_data_put_decimal64(pn_data_t *data, pn_decimal64_t d);
int pn_data_put_decimal128(pn_data_t *data, pn_decimal128_t d);
int pn_data_put_uuid(pn_data_t *data, pn_uuid_t u);
int pn_data_put_binary(pn_data_t *data, pn_bytes_t bytes);
int pn_data_put_string(pn_data_t *data, pn_bytes_t string);
int pn_data_put_symbol(pn_data_t *data, pn_bytes_t symbol);
int pn_data_put_atom(pn_data_t *data, pn_atom_t atom);
size_t pn_data_get_list(pn_data_t *data);
size_t pn_data_get_map(pn_data_t *data);
size_t pn_data_get_array(pn_data_t *data);
bool pn_data_is_array_described(pn_data_t *data);
pn_type_t pn_data_get_array_type(pn_data_t *data);
bool pn_data_is_described(pn_data_t *data);
bool pn_data_is_null(pn_data_t *data);
bool pn_data_get_bool(pn_data_t *data);
uint8_t pn_data_get_ubyte(pn_data_t *data);
int8_t pn_data_get_byte(pn_data_t *data);
uint16_t pn_data_get_ushort(pn_data_t *data);
int16_t pn_data_get_short(pn_data_t *data);
uint32_t pn_data_get_uint(pn_data_t *data);
int32_t pn_data_get_int(pn_data_t *data);
pn_char_t pn_data_get_char(pn_data_t *data);
uint64_t pn_data_get_ulong(pn_data_t *data);
int64_t pn_data_get_long(pn_data_t *data);
pn_timestamp_t pn_data_get_timestamp(pn_data_t *data);
float pn_data_get_float(pn_data_t *data);
double pn_data_get_double(pn_data_t *data);
pn_decimal32_t pn_data_get_decimal32(pn_data_t *data);
pn_decimal64_t pn_data_get_decimal64(pn_data_t *data);
pn_decimal128_t pn_data_get_decimal128(pn_data_t *data);
pn_uuid_t pn_data_get_uuid(pn_data_t *data);
pn_bytes_t pn_data_get_binary(pn_data_t *data);
pn_bytes_t pn_data_get_string(pn_data_t *data);
pn_bytes_t pn_data_get_symbol(pn_data_t *data);
pn_bytes_t pn_data_get_bytes(pn_data_t *data);
pn_atom_t pn_data_get_atom(pn_data_t *data);
int pn_data_copy(pn_data_t *data, pn_data_t *src);
int pn_data_append(pn_data_t *data, pn_data_t *src);
int pn_data_appendn(pn_data_t *data, pn_data_t *src, int limit);
void pn_data_narrow(pn_data_t *data);
void pn_data_widen(pn_data_t *data);
pn_handle_t pn_data_point(pn_data_t *data);
bool pn_data_restore(pn_data_t *data, pn_handle_t point);
void pn_data_dump(pn_data_t *data);
"""

type_h = """
typedef uint32_t  pn_sequence_t;

typedef uint32_t pn_millis_t;

#define PN_MILLIS_MAX ...

typedef uint32_t pn_seconds_t;

typedef int64_t pn_timestamp_t;

typedef uint32_t pn_char_t;

typedef uint32_t pn_decimal32_t;

typedef uint64_t pn_decimal64_t;

typedef struct {
  char bytes[16];
} pn_decimal128_t;

typedef struct {
  char bytes[16];
} pn_uuid_t;

typedef struct pn_bytes_t {
  size_t size;
  const char *start;
} pn_bytes_t;

pn_bytes_t pn_bytes(size_t size, const char *start);

extern const pn_bytes_t pn_bytes_null;

typedef struct pn_rwbytes_t {
  size_t size;
  char *start;
} pn_rwbytes_t;

pn_rwbytes_t pn_rwbytes(size_t size, char *start);

extern const pn_rwbytes_t pn_rwbytes_null;
typedef int pn_state_t;
typedef struct pn_connection_t pn_connection_t;

typedef struct pn_session_t pn_session_t;
typedef struct pn_link_t pn_link_t;

typedef struct pn_delivery_t pn_delivery_t;

typedef struct pn_collector_t pn_collector_t;
typedef struct pn_listener_t pn_listener_t;

typedef struct pn_transport_t pn_transport_t;

typedef struct pn_proactor_t pn_proactor_t;

typedef struct pn_raw_connection_t pn_raw_connection_t;

typedef struct pn_event_batch_t pn_event_batch_t;

typedef struct pn_handler_t pn_handler_t;
"""


message_h = """
typedef struct pn_message_t pn_message_t;

#define PN_DEFAULT_PRIORITY ...
    
pn_message_t * pn_message(void);
    
void           pn_message_free(pn_message_t *msg);
void           pn_message_clear(pn_message_t *msg);
int            pn_message_errno(pn_message_t *msg);
pn_error_t    *pn_message_error(pn_message_t *msg);
bool           pn_message_is_inferred(pn_message_t *msg);
int            pn_message_set_inferred(pn_message_t *msg, bool inferred);
bool           pn_message_is_durable            (pn_message_t *msg);
int            pn_message_set_durable           (pn_message_t *msg, bool durable);
uint8_t        pn_message_get_priority          (pn_message_t *msg);
int            pn_message_set_priority          (pn_message_t *msg, uint8_t priority);
pn_millis_t    pn_message_get_ttl               (pn_message_t *msg);
int            pn_message_set_ttl               (pn_message_t *msg, pn_millis_t ttl);
bool           pn_message_is_first_acquirer     (pn_message_t *msg);
int            pn_message_set_first_acquirer    (pn_message_t *msg, bool first);
uint32_t       pn_message_get_delivery_count    (pn_message_t *msg);
int            pn_message_set_delivery_count    (pn_message_t *msg, uint32_t count);
pn_data_t *    pn_message_id                    (pn_message_t *msg);
pn_atom_t      pn_message_get_id                (pn_message_t *msg);
int            pn_message_set_id                (pn_message_t *msg, pn_atom_t id);
pn_bytes_t     pn_message_get_user_id           (pn_message_t *msg);
int            pn_message_set_user_id           (pn_message_t *msg, pn_bytes_t user_id);
const char *   pn_message_get_address           (pn_message_t *msg);
int            pn_message_set_address           (pn_message_t *msg, const char *address);
const char *   pn_message_get_subject           (pn_message_t *msg);
int            pn_message_set_subject           (pn_message_t *msg, const char *subject);
const char *   pn_message_get_reply_to          (pn_message_t *msg);
int            pn_message_set_reply_to          (pn_message_t *msg, const char *reply_to);
pn_data_t *    pn_message_correlation_id        (pn_message_t *msg);
pn_atom_t      pn_message_get_correlation_id    (pn_message_t *msg);
int            pn_message_set_correlation_id    (pn_message_t *msg, pn_atom_t id);
const char *   pn_message_get_content_type      (pn_message_t *msg);
int            pn_message_set_content_type      (pn_message_t *msg, const char *type);
const char *   pn_message_get_content_encoding  (pn_message_t *msg);
int            pn_message_set_content_encoding  (pn_message_t *msg, const char *encoding);
pn_timestamp_t pn_message_get_expiry_time       (pn_message_t *msg);
int            pn_message_set_expiry_time       (pn_message_t *msg, pn_timestamp_t time);
pn_timestamp_t pn_message_get_creation_time     (pn_message_t *msg);
int            pn_message_set_creation_time     (pn_message_t *msg, pn_timestamp_t time);
const char *   pn_message_get_group_id          (pn_message_t *msg);
int            pn_message_set_group_id          (pn_message_t *msg, const char *group_id);
pn_sequence_t  pn_message_get_group_sequence    (pn_message_t *msg);
int            pn_message_set_group_sequence    (pn_message_t *msg, pn_sequence_t n);
const char *   pn_message_get_reply_to_group_id (pn_message_t *msg);
int            pn_message_set_reply_to_group_id (pn_message_t *msg, const char *reply_to_group_id);
pn_data_t *pn_message_instructions(pn_message_t *msg);
pn_data_t *pn_message_annotations(pn_message_t *msg);
pn_data_t *pn_message_properties(pn_message_t *msg);
pn_data_t *pn_message_body(pn_message_t *msg);
int pn_message_decode(pn_message_t *msg, const char *bytes, size_t size);
int pn_message_encode(pn_message_t *msg, char *bytes, size_t *size);
ssize_t pn_message_encode2(pn_message_t *msg, pn_rwbytes_t *buf);
struct pn_link_t;
ssize_t pn_message_send(pn_message_t *msg, pn_link_t *sender, pn_rwbytes_t *buf);
int pn_message_data(pn_message_t *msg, pn_data_t *data);
"""

error_h = """
#define PROTON_ERROR_H ...

typedef struct pn_error_t pn_error_t;
    
#define PN_OK ...
#define PN_EOS ...
#define PN_ERR ...
#define PN_OVERFLOW ...
#define PN_UNDERFLOW ...
#define PN_STATE_ERR ...
#define PN_ARG_ERR ...
#define PN_TIMEOUT ...
#define PN_INTR ...
#define PN_INPROGRESS ...
#define PN_OUT_OF_MEMORY ...
#define PN_ABORTED ...

const char *pn_code(int code);
pn_error_t *pn_error(void);
void pn_error_free(pn_error_t *error);
void pn_error_clear(pn_error_t *error);
int pn_error_set(pn_error_t *error, int code, const char *text);
int pn_error_vformat(pn_error_t *error, int code, const char *fmt, va_list ap);
int pn_error_format(pn_error_t *error, int code, const char *fmt, ...);
int pn_error_code(pn_error_t *error);

const char *pn_error_text(pn_error_t *error);

int pn_error_copy(pn_error_t *error, pn_error_t *src);
"""

condition_h = """
#define PROTON_CONDITION_H 1

typedef struct pn_condition_t pn_condition_t;

bool pn_condition_is_set(pn_condition_t *condition);
void pn_condition_clear(pn_condition_t *condition);
const char *pn_condition_get_name(pn_condition_t *condition);
int pn_condition_set_name(pn_condition_t *condition, const char *name);
const char *pn_condition_get_description(pn_condition_t *condition);
int pn_condition_set_description(pn_condition_t *condition, const char *description);
pn_data_t *pn_condition_info(pn_condition_t *condition);
int pn_condition_vformat(pn_condition_t *, const char *name, const char *fmt, va_list ap);
int pn_condition_format(pn_condition_t *, const char *name, const char *fmt, ...);
bool pn_condition_is_redirect(pn_condition_t *condition);
const char *pn_condition_redirect_host(pn_condition_t *condition);
int pn_condition_redirect_port(pn_condition_t *condition);
int pn_condition_copy(pn_condition_t *dest, pn_condition_t *src);
pn_condition_t *pn_condition(void);
void pn_condition_free(pn_condition_t *); 
"""

sasl_h = """
#define PROTON_SASL_H ...

typedef struct pn_sasl_t pn_sasl_t;

typedef enum {
  PN_SASL_NONE = -1,  /** negotiation not completed */
  PN_SASL_OK = 0,     /** authentication succeeded */
  PN_SASL_AUTH = 1,   /** failed due to bad credentials */
  PN_SASL_SYS = 2,    /** failed due to a system error */
  PN_SASL_PERM = 3,   /** failed due to unrecoverable error */
  PN_SASL_TEMP = 4    /** failed due to transient error */
} pn_sasl_outcome_t;

pn_sasl_t *pn_sasl(pn_transport_t *transport);
bool pn_sasl_extended(void);
void pn_sasl_done(pn_sasl_t *sasl, pn_sasl_outcome_t outcome);
pn_sasl_outcome_t pn_sasl_outcome(pn_sasl_t *sasl);
const char *pn_sasl_get_user(pn_sasl_t *sasl);
const char *pn_sasl_get_authorization(pn_sasl_t *sasl);
const char *pn_sasl_get_mech(pn_sasl_t *sasl);
void pn_sasl_allowed_mechs(pn_sasl_t *sasl, const char *mechs);
void pn_sasl_set_allow_insecure_mechs(pn_sasl_t *sasl, bool insecure);
bool pn_sasl_get_allow_insecure_mechs(pn_sasl_t *sasl);
void pn_sasl_config_name(pn_sasl_t *sasl, const char *name);
void pn_sasl_config_path(pn_sasl_t *sasl, const char *path);
"""

transport_h = """
typedef int pn_trace_t;

typedef void (*pn_tracer_t)(pn_transport_t *transport, const char *message);

#define PN_TRACE_OFF ...

#define PN_TRACE_RAW ...

#define PN_TRACE_FRM ...

#define PN_TRACE_DRV ...
 
#define PN_TRACE_EVT ...

pn_transport_t *pn_transport(void);
void pn_transport_set_server(pn_transport_t *transport);
void pn_transport_free(pn_transport_t *transport);
const char *pn_transport_get_user(pn_transport_t *transport);
void pn_transport_require_auth(pn_transport_t *transport, bool required);
bool pn_transport_is_authenticated(pn_transport_t *transport);
void pn_transport_require_encryption(pn_transport_t *transport, bool required);
bool pn_transport_is_encrypted(pn_transport_t *transport);
pn_condition_t *pn_transport_condition(pn_transport_t *transport);
pn_logger_t *pn_transport_logger(pn_transport_t *transport);
pn_error_t *pn_transport_error(pn_transport_t *transport);
int pn_transport_bind(pn_transport_t *transport, pn_connection_t *connection);
int pn_transport_unbind(pn_transport_t *transport);
void pn_transport_trace(pn_transport_t *transport, pn_trace_t trace);
void pn_transport_set_tracer(pn_transport_t *transport, pn_tracer_t tracer);
pn_tracer_t pn_transport_get_tracer(pn_transport_t *transport);
void *pn_transport_get_context(pn_transport_t *transport);
void pn_transport_set_context(pn_transport_t *transport, void *context);
pn_record_t *pn_transport_attachments(pn_transport_t *transport);
void pn_transport_log(pn_transport_t *transport, const char *message);
void pn_transport_vlogf(pn_transport_t *transport, const char *fmt, va_list ap);
void pn_transport_logf(pn_transport_t *transport, const char *fmt, ...);
uint16_t pn_transport_get_channel_max(pn_transport_t *transport);
int pn_transport_set_channel_max(pn_transport_t *transport, uint16_t channel_max);
uint16_t pn_transport_remote_channel_max(pn_transport_t *transport);
uint32_t pn_transport_get_max_frame(pn_transport_t *transport);
void pn_transport_set_max_frame(pn_transport_t *transport, uint32_t size);
uint32_t pn_transport_get_remote_max_frame(pn_transport_t *transport);
pn_millis_t pn_transport_get_idle_timeout(pn_transport_t *transport);
void pn_transport_set_idle_timeout(pn_transport_t *transport, pn_millis_t timeout);
pn_millis_t pn_transport_get_remote_idle_timeout(pn_transport_t *transport);
ssize_t pn_transport_input(pn_transport_t *transport, const char *bytes, size_t available);
ssize_t pn_transport_output(pn_transport_t *transport, char *bytes, size_t size);
ssize_t pn_transport_capacity(pn_transport_t *transport);
char *pn_transport_tail(pn_transport_t *transport);
ssize_t pn_transport_push(pn_transport_t *transport, const char *src, size_t size);
int pn_transport_process(pn_transport_t *transport, size_t size);
int pn_transport_close_tail(pn_transport_t *transport);
ssize_t pn_transport_pending(pn_transport_t *transport);
const char *pn_transport_head(pn_transport_t *transport);
ssize_t pn_transport_peek(pn_transport_t *transport, char *dst, size_t size);
void pn_transport_pop(pn_transport_t *transport, size_t size);
int pn_transport_close_head(pn_transport_t *transport);
bool pn_transport_quiesced(pn_transport_t *transport);
bool pn_transport_head_closed(pn_transport_t *transport);
bool pn_transport_tail_closed(pn_transport_t *transport);
bool pn_transport_closed(pn_transport_t *transport);
int64_t pn_transport_tick(pn_transport_t *transport, int64_t now);
uint64_t pn_transport_get_frames_output(const pn_transport_t *transport);
uint64_t pn_transport_get_frames_input(const pn_transport_t *transport);
pn_connection_t *pn_transport_connection(pn_transport_t *transport);
"""

logger_h = """
typedef struct pn_logger_t pn_logger_t;

typedef enum pn_log_subsystem_t {
    PN_SUBSYSTEM_NONE    = ...,
    PN_SUBSYSTEM_MEMORY  = ...,
    PN_SUBSYSTEM_IO      = ...,
    PN_SUBSYSTEM_EVENT   = ...,
    PN_SUBSYSTEM_AMQP    = ...,
    PN_SUBSYSTEM_SSL     = ...,
    PN_SUBSYSTEM_SASL    = ...,
    PN_SUBSYSTEM_BINDING = ...,
    PN_SUBSYSTEM_ALL     = ...
} pn_log_subsystem_t; 

typedef enum pn_log_level_t {
    PN_LEVEL_NONE     = ...,
    PN_LEVEL_CRITICAL = ...,
    PN_LEVEL_ERROR    = ...,
    PN_LEVEL_WARNING  = ...,
    PN_LEVEL_INFO     = ...,
    PN_LEVEL_DEBUG    = ...,
    PN_LEVEL_TRACE    = ...,
    PN_LEVEL_FRAME    = ...,
    PN_LEVEL_RAW      = ...,
    PN_LEVEL_ALL      = ...
} pn_log_level_t; 

typedef void (*pn_log_sink_t)(intptr_t sink_context, pn_log_subsystem_t subsystem, pn_log_level_t severity, const char *message);

pn_logger_t *pn_default_logger(void);
const char *pn_logger_level_name(pn_log_level_t level);
const char *pn_logger_subsystem_name(pn_log_subsystem_t subsystem);
void pn_logger_set_mask(pn_logger_t *logger, uint16_t subsystem, uint16_t level);
void pn_logger_reset_mask(pn_logger_t *logger, uint16_t subsystem, uint16_t level);
void pn_logger_set_log_sink(pn_logger_t *logger, pn_log_sink_t sink, intptr_t sink_context);
pn_log_sink_t pn_logger_get_log_sink(pn_logger_t *logger);
intptr_t pn_logger_get_log_sink_context(pn_logger_t *logger);
void pn_logger_logf(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t level, const char *fmt, ...);
"""

object_h = """

typedef const void* pn_handle_t;

typedef intptr_t pn_shandle_t;

typedef struct pn_class_t pn_class_t;

typedef struct pn_string_t pn_string_t;

typedef struct pn_list_t pn_list_t;

typedef struct pn_map_t pn_map_t;

typedef struct pn_hash_t pn_hash_t;

typedef void *(*pn_iterator_next_t)(void *state);

typedef struct pn_iterator_t pn_iterator_t;

typedef struct pn_record_t pn_record_t;

struct pn_class_t {
  const char *name;
  const pn_cid_t cid;
  void *(*newinst)(const pn_class_t *, size_t);
  void (*initialize)(void *);
  void (*incref)(void *);
  void (*decref)(void *);
  int (*refcount)(void *);
  void (*finalize)(void *);
  void (*free)(void *);
  const pn_class_t *(*reify)(void *);
  uintptr_t (*hashcode)(void *);
  intptr_t (*compare)(void *, void *);
  int (*inspect)(void *, pn_string_t *);
};

extern const pn_class_t PN_OBJECT[];

extern const pn_class_t PN_VOID[];

extern const pn_class_t PN_WEAKREF[];

pn_cid_t pn_class_id(const pn_class_t *clazz);
const char *pn_class_name(const pn_class_t *clazz);
void *pn_class_new(const pn_class_t *clazz, size_t size);
void *pn_class_incref(const pn_class_t *clazz, void *object);
int pn_class_refcount(const pn_class_t *clazz, void *object);
int pn_class_decref(const pn_class_t *clazz, void *object);
void pn_class_free(const pn_class_t *clazz, void *object);
const pn_class_t *pn_class_reify(const pn_class_t *clazz, void *object);
uintptr_t pn_class_hashcode(const pn_class_t *clazz, void *object);
intptr_t pn_class_compare(const pn_class_t *clazz, void *a, void *b);
bool pn_class_equals(const pn_class_t *clazz, void *a, void *b);
int pn_class_inspect(const pn_class_t *clazz, void *object, pn_string_t *dst);
void *pn_void_new(const pn_class_t *clazz, size_t size);
void pn_void_incref(void *object);
void pn_void_decref(void *object);
int pn_void_refcount(void *object);
uintptr_t pn_void_hashcode(void *object);
intptr_t pn_void_compare(void *a, void *b);
int pn_void_inspect(void *object, pn_string_t *dst);
void *pn_object_new(const pn_class_t *clazz, size_t size);
const pn_class_t *pn_object_reify(void *object);
void pn_object_incref(void *object);
int pn_object_refcount(void *object);
void pn_object_decref(void *object);
void pn_object_free(void *object);
void *pn_incref(void *object);
int pn_decref(void *object);
int pn_refcount(void *object);
void pn_free(void *object);
const pn_class_t *pn_class(void* object);
uintptr_t pn_hashcode(void *object);
intptr_t pn_compare(void *a, void *b);
bool pn_equals(void *a, void *b);
int pn_inspect(void *object, pn_string_t *dst);

#define PN_REFCOUNT ...

pn_list_t *pn_list(const pn_class_t *clazz, size_t capacity);
size_t pn_list_size(pn_list_t *list);
void *pn_list_get(pn_list_t *list, int index);
void pn_list_set(pn_list_t *list, int index, void *value);
int pn_list_add(pn_list_t *list, void *value);
void *pn_list_pop(pn_list_t *list);
ssize_t pn_list_index(pn_list_t *list, void *value);
bool pn_list_remove(pn_list_t *list, void *value);
void pn_list_del(pn_list_t *list, int index, int n);
void pn_list_clear(pn_list_t *list);
void pn_list_iterator(pn_list_t *list, pn_iterator_t *iter);
void pn_list_minpush(pn_list_t *list, void *value);
void *pn_list_minpop(pn_list_t *list);

#define PN_REFCOUNT_KEY ...

#define PN_REFCOUNT_VALUE ...

pn_map_t *pn_map(const pn_class_t *key, const pn_class_t *value,
                           size_t capacity, float load_factor);
size_t pn_map_size(pn_map_t *map);
int pn_map_put(pn_map_t *map, void *key, void *value);
void *pn_map_get(pn_map_t *map, void *key);
void pn_map_del(pn_map_t *map, void *key);
pn_handle_t pn_map_head(pn_map_t *map);
pn_handle_t pn_map_next(pn_map_t *map, pn_handle_t entry);
void *pn_map_key(pn_map_t *map, pn_handle_t entry);
void *pn_map_value(pn_map_t *map, pn_handle_t entry);
pn_hash_t *pn_hash(const pn_class_t *clazz, size_t capacity, float load_factor);
size_t pn_hash_size(pn_hash_t *hash);
int pn_hash_put(pn_hash_t *hash, uintptr_t key, void *value);
void *pn_hash_get(pn_hash_t *hash, uintptr_t key);
void pn_hash_del(pn_hash_t *hash, uintptr_t key);
pn_handle_t pn_hash_head(pn_hash_t *hash);
pn_handle_t pn_hash_next(pn_hash_t *hash, pn_handle_t entry);
uintptr_t pn_hash_key(pn_hash_t *hash, pn_handle_t entry);
void *pn_hash_value(pn_hash_t *hash, pn_handle_t entry);

pn_string_t *pn_string(const char *bytes);
pn_string_t *pn_stringn(const char *bytes, size_t n);
const char *pn_string_get(pn_string_t *string);
size_t pn_string_size(pn_string_t *string);
int pn_string_set(pn_string_t *string, const char *bytes);
int pn_string_setn(pn_string_t *string, const char *bytes, size_t n);
ssize_t pn_string_put(pn_string_t *string, char *dst);
void pn_string_clear(pn_string_t *string);
int pn_string_format(pn_string_t *string, const char *format, ...);
int pn_string_vformat(pn_string_t *string, const char *format, va_list ap);
int pn_string_addf(pn_string_t *string, const char *format, ...);
int pn_string_vaddf(pn_string_t *string, const char *format, va_list ap);
int pn_string_grow(pn_string_t *string, size_t capacity);
char *pn_string_buffer(pn_string_t *string);
size_t pn_string_capacity(pn_string_t *string);
int pn_string_resize(pn_string_t *string, size_t size);
int pn_string_copy(pn_string_t *string, pn_string_t *src);
pn_iterator_t *pn_iterator(void);
void *pn_iterator_start(pn_iterator_t *iterator,
                                  pn_iterator_next_t next, size_t size);
void *pn_iterator_next(pn_iterator_t *iterator);
pn_record_t *pn_record(void);
void pn_record_def(pn_record_t *record, pn_handle_t key, const pn_class_t *clazz);
bool pn_record_has(pn_record_t *record, pn_handle_t key);
void *pn_record_get(pn_record_t *record, pn_handle_t key);
void pn_record_set(pn_record_t *record, pn_handle_t key, void *value);
void pn_record_clear(pn_record_t *record);
"""

cid_h = """
typedef enum {
  CID_pn_object = ...,
  CID_pn_void,
  CID_pn_weakref,
  CID_pn_string,
  CID_pn_list,
  CID_pn_map,
  CID_pn_hash,
  CID_pn_record,
  CID_pn_collector,
  CID_pn_event,
  CID_pn_buffer,
  CID_pn_error,
  CID_pn_data,
  CID_pn_connection,
  CID_pn_session,
  CID_pn_link,
  CID_pn_delivery,
  CID_pn_transport,
  CID_pn_message,
  CID_pn_reactor,
  CID_pn_handler,
  CID_pn_timer,
  CID_pn_task,
  CID_pn_io,
  CID_pn_selector,
  CID_pn_selectable,
  CID_pn_url,
  CID_pn_strdup,
  CID_pn_listener,
  CID_pn_proactor,
  CID_pn_listener_socket,
  CID_pn_raw_connection
} pn_cid_t;
"""

ssl_h = """
typedef struct pn_ssl_domain_t pn_ssl_domain_t;

typedef struct pn_ssl_t pn_ssl_t;

typedef enum {
  PN_SSL_MODE_CLIENT = ...,
  PN_SSL_MODE_SERVER
} pn_ssl_mode_t;

typedef enum {
  PN_SSL_RESUME_UNKNOWN,
  PN_SSL_RESUME_NEW,
  PN_SSL_RESUME_REUSED
} pn_ssl_resume_status_t;

bool pn_ssl_present( void );

pn_ssl_domain_t *pn_ssl_domain(pn_ssl_mode_t mode);

void pn_ssl_domain_free(pn_ssl_domain_t *domain);
int  pn_ssl_domain_set_credentials(pn_ssl_domain_t *domain,
                                            const char *credential_1,
                                            const char *credential_2,
                                            const char *password);
int pn_ssl_domain_set_trusted_ca_db(pn_ssl_domain_t *domain,
                                const char *certificate_db);

typedef enum {
  PN_SSL_VERIFY_NULL = ...,   
  PN_SSL_VERIFY_PEER,
  PN_SSL_ANONYMOUS_PEER,
  PN_SSL_VERIFY_PEER_NAME
} pn_ssl_verify_mode_t;

int pn_ssl_domain_set_peer_authentication(pn_ssl_domain_t *domain,
                                                    const pn_ssl_verify_mode_t mode,
                                                    const char *trusted_CAs);
int pn_ssl_domain_set_protocols(pn_ssl_domain_t *domain, const char *protocols);
int pn_ssl_domain_set_ciphers(pn_ssl_domain_t *domain, const char *ciphers);
int pn_ssl_domain_allow_unsecured_client(pn_ssl_domain_t *domain);
pn_ssl_t *pn_ssl(pn_transport_t *transport);
int pn_ssl_init(pn_ssl_t *ssl,
                          pn_ssl_domain_t *domain,
                          const char *session_id);
bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *buffer, size_t size);
int pn_ssl_get_ssf(pn_ssl_t *ssl);
bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *buffer, size_t size);
pn_ssl_resume_status_t pn_ssl_resume_status(pn_ssl_t *ssl);
int pn_ssl_set_peer_hostname(pn_ssl_t *ssl, const char *hostname);
int pn_ssl_get_peer_hostname(pn_ssl_t *ssl, char *hostname, size_t *bufsize);

const char* pn_ssl_get_remote_subject(pn_ssl_t *ssl);

typedef enum {
  PN_SSL_CERT_SUBJECT_COUNTRY_NAME,
  PN_SSL_CERT_SUBJECT_STATE_OR_PROVINCE,
  PN_SSL_CERT_SUBJECT_CITY_OR_LOCALITY,
  PN_SSL_CERT_SUBJECT_ORGANIZATION_NAME,
  PN_SSL_CERT_SUBJECT_ORGANIZATION_UNIT,
  PN_SSL_CERT_SUBJECT_COMMON_NAME
} pn_ssl_cert_subject_subfield;

typedef enum {
  PN_SSL_SHA1,
  PN_SSL_SHA256,
  PN_SSL_SHA512, 
  PN_SSL_MD5     
} pn_ssl_hash_alg;

int pn_ssl_get_cert_fingerprint(pn_ssl_t *ssl0,
                                          char *fingerprint,
                                          size_t fingerprint_length,
                                          pn_ssl_hash_alg hash_alg);
const char* pn_ssl_get_remote_subject_subfield(pn_ssl_t *ssl0, pn_ssl_cert_subject_subfield field);
"""

disposition_h = """
typedef struct pn_disposition_t pn_disposition_t;

#define PN_RECEIVED ...
#define PN_ACCEPTED ...
#define PN_REJECTED ...
#define PN_RELEASED ...
#define PN_MODIFIED ...

uint64_t pn_disposition_type(pn_disposition_t *disposition);
const char *pn_disposition_type_name(uint64_t disposition_type);
pn_condition_t *pn_disposition_condition(pn_disposition_t *disposition);
uint32_t pn_disposition_get_section_number(pn_disposition_t *disposition);
void pn_disposition_set_section_number(pn_disposition_t *disposition, uint32_t section_number);
uint64_t pn_disposition_get_section_offset(pn_disposition_t *disposition);
void pn_disposition_set_section_offset(pn_disposition_t *disposition, uint64_t section_offset);
bool pn_disposition_is_failed(pn_disposition_t *disposition);
void pn_disposition_set_failed(pn_disposition_t *disposition, bool failed);
bool pn_disposition_is_undeliverable(pn_disposition_t *disposition);
void pn_disposition_set_undeliverable(pn_disposition_t *disposition, bool undeliverable);
pn_data_t *pn_disposition_annotations(pn_disposition_t *disposition);
"""

delivery_h = """
#define PROTON_DELIVERY_H 1

typedef pn_bytes_t pn_delivery_tag_t;

pn_delivery_tag_t pn_dtag(const char *bytes, size_t size);
pn_delivery_t *pn_delivery(pn_link_t *link, pn_delivery_tag_t tag);
void *pn_delivery_get_context(pn_delivery_t *delivery);
void pn_delivery_set_context(pn_delivery_t *delivery, void *context);
pn_record_t *pn_delivery_attachments(pn_delivery_t *delivery);
pn_delivery_tag_t pn_delivery_tag(pn_delivery_t *delivery);
pn_link_t *pn_delivery_link(pn_delivery_t *delivery);
pn_disposition_t *pn_delivery_local(pn_delivery_t *delivery);
uint64_t pn_delivery_local_state(pn_delivery_t *delivery);
pn_disposition_t *pn_delivery_remote(pn_delivery_t *delivery);
uint64_t pn_delivery_remote_state(pn_delivery_t *delivery);
bool pn_delivery_settled(pn_delivery_t *delivery);
size_t pn_delivery_pending(pn_delivery_t *delivery);
bool pn_delivery_partial(pn_delivery_t *delivery);
bool pn_delivery_aborted(pn_delivery_t *delivery);
bool pn_delivery_writable(pn_delivery_t *delivery);
bool pn_delivery_readable(pn_delivery_t *delivery);
bool pn_delivery_updated(pn_delivery_t *delivery);
void pn_delivery_update(pn_delivery_t *delivery, uint64_t state);
void pn_delivery_clear(pn_delivery_t *delivery);
bool pn_delivery_current(pn_delivery_t *delivery);
void pn_delivery_abort(pn_delivery_t *delivery);
void pn_delivery_settle(pn_delivery_t *delivery);
void pn_delivery_dump(pn_delivery_t *delivery);
bool pn_delivery_buffered(pn_delivery_t *delivery);
pn_delivery_t *pn_work_head(pn_connection_t *connection);
pn_delivery_t *pn_work_next(pn_delivery_t *delivery);
"""

event_h = """

typedef struct pn_event_t pn_event_t;

typedef enum {
  PN_EVENT_NONE = ...,
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

const char *pn_event_type_name(pn_event_type_t type);
pn_collector_t *pn_collector(void);
void pn_collector_free(pn_collector_t *collector);
void pn_collector_release(pn_collector_t *collector);
void pn_collector_drain(pn_collector_t *collector);
pn_event_t *pn_collector_put(pn_collector_t *collector,
                                       const pn_class_t *clazz, void *context,
                                       pn_event_type_t type);
pn_event_t *pn_collector_peek(pn_collector_t *collector);
bool pn_collector_pop(pn_collector_t *collector);
pn_event_t *pn_collector_next(pn_collector_t *collector);
pn_event_t *pn_collector_prev(pn_collector_t *collector);
bool pn_collector_more(pn_collector_t *collector);
pn_event_type_t pn_event_type(pn_event_t *event);
const pn_class_t *pn_event_class(pn_event_t *event);
void *pn_event_context(pn_event_t *event);
pn_connection_t *pn_event_connection(pn_event_t *event);
pn_session_t *pn_event_session(pn_event_t *event);
pn_link_t *pn_event_link(pn_event_t *event);
pn_delivery_t *pn_event_delivery(pn_event_t *event);
pn_transport_t *pn_event_transport(pn_event_t *event);
pn_record_t *pn_event_attachments(pn_event_t *event);
struct pn_condition_t *pn_event_condition(pn_event_t *event);
"""

terminus_h = """
typedef struct pn_terminus_t pn_terminus_t;

typedef enum {
  PN_UNSPECIFIED = ... ,
  PN_SOURCE = ... ,
  PN_TARGET = ... ,
  PN_COORDINATOR = ...
} pn_terminus_type_t;

typedef enum {
  PN_NONDURABLE = ... , 
  PN_CONFIGURATION = ... ,
  PN_DELIVERIES = ...
} pn_durability_t;

typedef enum {
  PN_EXPIRE_WITH_LINK,
  PN_EXPIRE_WITH_SESSION,
  PN_EXPIRE_WITH_CONNECTION,
  PN_EXPIRE_NEVER
} pn_expiry_policy_t;

typedef enum {
  PN_DIST_MODE_UNSPECIFIED = 0, /**< the behaviour is defined by the node */
  PN_DIST_MODE_COPY = 1, /**< the receiver gets all messages */
  PN_DIST_MODE_MOVE = 2 /**< the receiver competes for messages */
} pn_distribution_mode_t;

pn_terminus_type_t pn_terminus_get_type(pn_terminus_t *terminus);
int pn_terminus_set_type(pn_terminus_t *terminus, pn_terminus_type_t type);
const char *pn_terminus_get_address(pn_terminus_t *terminus);
int pn_terminus_set_address(pn_terminus_t *terminus, const char *address);
pn_distribution_mode_t pn_terminus_get_distribution_mode(const pn_terminus_t *terminus);
int pn_terminus_set_distribution_mode(pn_terminus_t *terminus, pn_distribution_mode_t mode);
pn_durability_t pn_terminus_get_durability(pn_terminus_t *terminus);
int pn_terminus_set_durability(pn_terminus_t *terminus,
                                         pn_durability_t durability);
pn_expiry_policy_t pn_terminus_get_expiry_policy(pn_terminus_t *terminus);
bool pn_terminus_has_expiry_policy(const pn_terminus_t *terminus);
int pn_terminus_set_expiry_policy(pn_terminus_t *terminus, pn_expiry_policy_t policy);
pn_seconds_t pn_terminus_get_timeout(pn_terminus_t *terminus);
int pn_terminus_set_timeout(pn_terminus_t *terminus, pn_seconds_t timeout);
bool pn_terminus_is_dynamic(pn_terminus_t *terminus);
int pn_terminus_set_dynamic(pn_terminus_t *terminus, bool dynamic);
pn_data_t *pn_terminus_properties(pn_terminus_t *terminus);
pn_data_t *pn_terminus_capabilities(pn_terminus_t *terminus);
pn_data_t *pn_terminus_outcomes(pn_terminus_t *terminus);
pn_data_t *pn_terminus_filter(pn_terminus_t *terminus);
int pn_terminus_copy(pn_terminus_t *terminus, pn_terminus_t *src);
"""

connection_h="""
#define PROTON_CONNECTION_H 1

#define PN_LOCAL_UNINIT ...

#define PN_LOCAL_ACTIVE ...

#define PN_LOCAL_CLOSED ...

#define PN_REMOTE_UNINIT ...

#define PN_REMOTE_ACTIVE ...

#define PN_REMOTE_CLOSED ...

#define PN_LOCAL_MASK ...

#define PN_REMOTE_MASK ...

pn_connection_t *pn_connection(void);
void pn_connection_free(pn_connection_t *connection);
void pn_connection_release(pn_connection_t *connection);
pn_error_t *pn_connection_error(pn_connection_t *connection);
void pn_connection_collect(pn_connection_t *connection, pn_collector_t *collector);
pn_collector_t* pn_connection_collector(pn_connection_t *connection);
void *pn_connection_get_context(pn_connection_t *connection);
void pn_connection_set_context(pn_connection_t *connection, void *context);
pn_record_t *pn_connection_attachments(pn_connection_t *connection);
pn_state_t pn_connection_state(pn_connection_t *connection);
void pn_connection_open(pn_connection_t *connection);
void pn_connection_close(pn_connection_t *connection);
void pn_connection_reset(pn_connection_t *connection);
pn_condition_t *pn_connection_condition(pn_connection_t *connection);
pn_condition_t *pn_connection_remote_condition(pn_connection_t *connection);
const char *pn_connection_get_container(pn_connection_t *connection);
void pn_connection_set_container(pn_connection_t *connection, const char *container);
void pn_connection_set_user(pn_connection_t *connection, const char *user);
void pn_connection_set_password(pn_connection_t *connection, const char *password);
void pn_connection_set_authorization(pn_connection_t *connection, const char *authzid);
const char *pn_connection_get_user(pn_connection_t *connection);
const char *pn_connection_get_authorization(pn_connection_t *connection);
const char *pn_connection_get_hostname(pn_connection_t *connection);
void pn_connection_set_hostname(pn_connection_t *connection, const char *hostname);
const char *pn_connection_remote_container(pn_connection_t *connection);
const char *pn_connection_remote_hostname(pn_connection_t *connection);
pn_data_t *pn_connection_offered_capabilities(pn_connection_t *connection);
pn_data_t *pn_connection_desired_capabilities(pn_connection_t *connection);
pn_data_t *pn_connection_properties(pn_connection_t *connection);
pn_data_t *pn_connection_remote_offered_capabilities(pn_connection_t *connection);
pn_data_t *pn_connection_remote_desired_capabilities(pn_connection_t *connection);
pn_data_t *pn_connection_remote_properties(pn_connection_t *connection);
pn_transport_t *pn_connection_transport(pn_connection_t *connection);
"""

link_h = """
pn_link_t *pn_sender(pn_session_t *session, const char *name);
pn_link_t *pn_receiver(pn_session_t *session, const char *name);
void pn_link_free(pn_link_t *link);
void *pn_link_get_context(pn_link_t *link);
void pn_link_set_context(pn_link_t *link, void *context);
pn_record_t *pn_link_attachments(pn_link_t *link);
const char *pn_link_name(pn_link_t *link);
bool pn_link_is_sender(pn_link_t *link);
bool pn_link_is_receiver(pn_link_t *link);
pn_state_t pn_link_state(pn_link_t *link);
pn_error_t *pn_link_error(pn_link_t *link);
pn_condition_t *pn_link_condition(pn_link_t *link);
pn_condition_t *pn_link_remote_condition(pn_link_t *link);
pn_session_t *pn_link_session(pn_link_t *link);
pn_link_t *pn_link_head(pn_connection_t *connection, pn_state_t state);
pn_link_t *pn_link_next(pn_link_t *link, pn_state_t state);
void pn_link_open(pn_link_t *link);
void pn_link_close(pn_link_t *link);
void pn_link_detach(pn_link_t *link);
pn_terminus_t *pn_link_source(pn_link_t *link);
pn_terminus_t *pn_link_target(pn_link_t *link);
pn_terminus_t *pn_link_remote_source(pn_link_t *link);
pn_terminus_t *pn_link_remote_target(pn_link_t *link);
pn_delivery_t *pn_link_current(pn_link_t *link);
bool pn_link_advance(pn_link_t *link);
int pn_link_credit(pn_link_t *link);
int pn_link_queued(pn_link_t *link);
int pn_link_remote_credit(pn_link_t *link);
bool pn_link_get_drain(pn_link_t *link);
int pn_link_drained(pn_link_t *link);
int pn_link_available(pn_link_t *link);

typedef enum {
  PN_SND_UNSETTLED = ...,
  PN_SND_SETTLED = ... ,
  PN_SND_MIXED = ...
} pn_snd_settle_mode_t;

typedef enum {
  PN_RCV_FIRST = ... ,
  PN_RCV_SECOND = ...
} pn_rcv_settle_mode_t;

pn_snd_settle_mode_t pn_link_snd_settle_mode(pn_link_t *link);
pn_rcv_settle_mode_t pn_link_rcv_settle_mode(pn_link_t *link);
void pn_link_set_snd_settle_mode(pn_link_t *link, pn_snd_settle_mode_t mode);
void pn_link_set_rcv_settle_mode(pn_link_t *link, pn_rcv_settle_mode_t mode);
pn_snd_settle_mode_t pn_link_remote_snd_settle_mode(pn_link_t *link);
pn_rcv_settle_mode_t pn_link_remote_rcv_settle_mode(pn_link_t *link);
int pn_link_unsettled(pn_link_t *link);
pn_delivery_t *pn_unsettled_head(pn_link_t *link);
pn_delivery_t *pn_unsettled_next(pn_delivery_t *delivery);
void pn_link_offered(pn_link_t *sender, int credit);
ssize_t pn_link_send(pn_link_t *sender, const char *bytes, size_t n);
void pn_link_flow(pn_link_t *receiver, int credit);
void pn_link_drain(pn_link_t *receiver, int credit);
void pn_link_set_drain(pn_link_t *receiver, bool drain);
ssize_t pn_link_recv(pn_link_t *receiver, char *bytes, size_t n);
bool pn_link_draining(pn_link_t *receiver);
uint64_t pn_link_max_message_size(pn_link_t *link);
void pn_link_set_max_message_size(pn_link_t *link, uint64_t size);
uint64_t pn_link_remote_max_message_size(pn_link_t *link);
pn_data_t *pn_link_properties(pn_link_t *link);
pn_data_t *pn_link_remote_properties(pn_link_t *link);
"""


session_h ="""
pn_session_t *pn_session(pn_connection_t *connection);

void pn_session_free(pn_session_t *session);
void *pn_session_get_context(pn_session_t *session);
void pn_session_set_context(pn_session_t *session, void *context);
pn_record_t *pn_session_attachments(pn_session_t *session);
pn_state_t pn_session_state(pn_session_t *session);
pn_error_t *pn_session_error(pn_session_t *session);
pn_condition_t *pn_session_condition(pn_session_t *session);
pn_condition_t *pn_session_remote_condition(pn_session_t *session);
pn_connection_t *pn_session_connection(pn_session_t *session);
void pn_session_open(pn_session_t *session);
void pn_session_close(pn_session_t *session);
size_t pn_session_get_incoming_capacity(pn_session_t *session);
void pn_session_set_incoming_capacity(pn_session_t *session, size_t capacity);
size_t pn_session_get_outgoing_window(pn_session_t *session);
void pn_session_set_outgoing_window(pn_session_t *session, size_t window);
size_t pn_session_outgoing_bytes(pn_session_t *session);
size_t pn_session_incoming_bytes(pn_session_t *session);
pn_session_t *pn_session_head(pn_connection_t *connection, pn_state_t state);
pn_session_t *pn_session_next(pn_session_t *session, pn_state_t state);
"""

condition_h = """
#define PROTON_CONDITION_H 1

typedef struct pn_condition_t pn_condition_t;

bool pn_condition_is_set(pn_condition_t *condition);
void pn_condition_clear(pn_condition_t *condition);
const char *pn_condition_get_name(pn_condition_t *condition);
int pn_condition_set_name(pn_condition_t *condition, const char *name);
const char *pn_condition_get_description(pn_condition_t *condition);
int pn_condition_set_description(pn_condition_t *condition, const char *description);
pn_data_t *pn_condition_info(pn_condition_t *condition);
int pn_condition_vformat(pn_condition_t *, const char *name, const char *fmt, ...);
int pn_condition_format(pn_condition_t *, const char *name, const char *fmt, ...);
bool pn_condition_is_redirect(pn_condition_t *condition);
const char *pn_condition_redirect_host(pn_condition_t *condition);
int pn_condition_redirect_port(pn_condition_t *condition);
int pn_condition_copy(pn_condition_t *dest, pn_condition_t *src);
pn_condition_t *pn_condition(void);
void pn_condition_free(pn_condition_t *); 
"""


sasl_h = """
//#define PROTON_SASL_H ...

typedef struct pn_sasl_t pn_sasl_t;

typedef enum {
  PN_SASL_NONE = -1,  /** negotiation not completed */
  PN_SASL_OK = 0,     /** authentication succeeded */
  PN_SASL_AUTH = 1,   /** failed due to bad credentials */
  PN_SASL_SYS = 2,    /** failed due to a system error */
  PN_SASL_PERM = 3,   /** failed due to unrecoverable error */
  PN_SASL_TEMP = 4    /** failed due to transient error */
} pn_sasl_outcome_t;

 pn_sasl_t *pn_sasl(pn_transport_t *transport);
 bool pn_sasl_extended(void);
 void pn_sasl_done(pn_sasl_t *sasl, pn_sasl_outcome_t outcome);
 pn_sasl_outcome_t pn_sasl_outcome(pn_sasl_t *sasl);
 const char *pn_sasl_get_user(pn_sasl_t *sasl);
 const char *pn_sasl_get_authorization(pn_sasl_t *sasl);
 const char *pn_sasl_get_mech(pn_sasl_t *sasl);
 void pn_sasl_allowed_mechs(pn_sasl_t *sasl, const char *mechs);
 void pn_sasl_set_allow_insecure_mechs(pn_sasl_t *sasl, bool insecure);
 bool pn_sasl_get_allow_insecure_mechs(pn_sasl_t *sasl);
 void pn_sasl_config_name(pn_sasl_t *sasl, const char *name);
 void pn_sasl_config_path(pn_sasl_t *sasl, const char *path);

"""

transport_h = """

typedef int pn_trace_t;

typedef void (*pn_tracer_t)(pn_transport_t *transport, const char *message);


#define PN_TRACE_OFF ...

#define PN_TRACE_RAW ...

#define PN_TRACE_FRM ...

#define PN_TRACE_DRV ...
 
#define PN_TRACE_EVT ...

 pn_transport_t *pn_transport(void);
 void pn_transport_set_server(pn_transport_t *transport);
 void pn_transport_free(pn_transport_t *transport);
 const char *pn_transport_get_user(pn_transport_t *transport);
 void pn_transport_require_auth(pn_transport_t *transport, bool required);
 bool pn_transport_is_authenticated(pn_transport_t *transport);
 void pn_transport_require_encryption(pn_transport_t *transport, bool required);
 bool pn_transport_is_encrypted(pn_transport_t *transport);
 pn_condition_t *pn_transport_condition(pn_transport_t *transport);
 pn_logger_t *pn_transport_logger(pn_transport_t *transport);
 pn_error_t *pn_transport_error(pn_transport_t *transport);
 int pn_transport_bind(pn_transport_t *transport, pn_connection_t *connection);
 int pn_transport_unbind(pn_transport_t *transport);
 void pn_transport_trace(pn_transport_t *transport, pn_trace_t trace);
 void pn_transport_set_tracer(pn_transport_t *transport, pn_tracer_t tracer);
 pn_tracer_t pn_transport_get_tracer(pn_transport_t *transport);
 void *pn_transport_get_context(pn_transport_t *transport);
 void pn_transport_set_context(pn_transport_t *transport, void *context);
 pn_record_t *pn_transport_attachments(pn_transport_t *transport);
 void pn_transport_log(pn_transport_t *transport, const char *message);
 void pn_transport_vlogf(pn_transport_t *transport, const char *fmt, ...);
 void pn_transport_logf(pn_transport_t *transport, const char *fmt, ...);
 uint16_t pn_transport_get_channel_max(pn_transport_t *transport);
 int pn_transport_set_channel_max(pn_transport_t *transport, uint16_t channel_max);
 uint16_t pn_transport_remote_channel_max(pn_transport_t *transport);
 uint32_t pn_transport_get_max_frame(pn_transport_t *transport);
 void pn_transport_set_max_frame(pn_transport_t *transport, uint32_t size);
 uint32_t pn_transport_get_remote_max_frame(pn_transport_t *transport);
 pn_millis_t pn_transport_get_idle_timeout(pn_transport_t *transport);
 void pn_transport_set_idle_timeout(pn_transport_t *transport, pn_millis_t timeout);
 pn_millis_t pn_transport_get_remote_idle_timeout(pn_transport_t *transport);
 ssize_t pn_transport_input(pn_transport_t *transport, const char *bytes, size_t available);
 ssize_t pn_transport_output(pn_transport_t *transport, char *bytes, size_t size);
 ssize_t pn_transport_capacity(pn_transport_t *transport);
 char *pn_transport_tail(pn_transport_t *transport);
 ssize_t pn_transport_push(pn_transport_t *transport, const char *src, size_t size);
 int pn_transport_process(pn_transport_t *transport, size_t size);
 int pn_transport_close_tail(pn_transport_t *transport);
 ssize_t pn_transport_pending(pn_transport_t *transport);
 const char *pn_transport_head(pn_transport_t *transport);
 ssize_t pn_transport_peek(pn_transport_t *transport, char *dst, size_t size);
 void pn_transport_pop(pn_transport_t *transport, size_t size);
 int pn_transport_close_head(pn_transport_t *transport);
 bool pn_transport_quiesced(pn_transport_t *transport);
 bool pn_transport_head_closed(pn_transport_t *transport);
 bool pn_transport_tail_closed(pn_transport_t *transport);
 bool pn_transport_closed(pn_transport_t *transport);
 int64_t pn_transport_tick(pn_transport_t *transport, int64_t now);
 uint64_t pn_transport_get_frames_output(const pn_transport_t *transport);
 uint64_t pn_transport_get_frames_input(const pn_transport_t *transport);
 pn_connection_t *pn_transport_connection(pn_transport_t *transport);




"""


logger_h = """

typedef struct pn_logger_t pn_logger_t;

typedef enum pn_log_subsystem_t {
    PN_SUBSYSTEM_NONE    = ...,
    PN_SUBSYSTEM_MEMORY  = ...,
    PN_SUBSYSTEM_IO      = ...,
    PN_SUBSYSTEM_EVENT   = ...,
    PN_SUBSYSTEM_AMQP    = ...,
    PN_SUBSYSTEM_SSL     = ...,
    PN_SUBSYSTEM_SASL    = ...,
    PN_SUBSYSTEM_BINDING = ...,
    PN_SUBSYSTEM_ALL     = ...
} pn_log_subsystem_t; 

typedef enum pn_log_level_t {
    PN_LEVEL_NONE     = ...,
    PN_LEVEL_CRITICAL = ...,
    PN_LEVEL_ERROR    = ...,
    PN_LEVEL_WARNING  = ...,
    PN_LEVEL_INFO     = ...,
    PN_LEVEL_DEBUG    = ...,
    PN_LEVEL_TRACE    = ...,
    PN_LEVEL_FRAME    = ...,
    PN_LEVEL_RAW      = ...,
    PN_LEVEL_ALL      = ...
} pn_log_level_t; 

typedef void (*pn_log_sink_t)(intptr_t sink_context, pn_log_subsystem_t subsystem, pn_log_level_t severity, const char *message);

pn_logger_t *pn_default_logger(void);
const char *pn_logger_level_name(pn_log_level_t level);
const char *pn_logger_subsystem_name(pn_log_subsystem_t subsystem);
void pn_logger_set_mask(pn_logger_t *logger, uint16_t subsystem, uint16_t level);
void pn_logger_reset_mask(pn_logger_t *logger, uint16_t subsystem, uint16_t level);
void pn_logger_set_log_sink(pn_logger_t *logger, pn_log_sink_t sink, intptr_t sink_context);
pn_log_sink_t pn_logger_get_log_sink(pn_logger_t *logger);
intptr_t pn_logger_get_log_sink_context(pn_logger_t *logger);
void pn_logger_logf(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t level, const char *fmt, ...);
"""

object_h = """

// #define PN_CLASSCLASS ...
//   
// #define PN_CLASSDEF ...
//   
// #define PN_CLASS ...
//   
// #define PN_METACLASS ...
//   
// #define PN_STRUCT_CLASSDEF ...

typedef const void* pn_handle_t;

typedef intptr_t pn_shandle_t;

typedef struct pn_class_t pn_class_t;

typedef struct pn_string_t pn_string_t;

typedef struct pn_list_t pn_list_t;

typedef struct pn_map_t pn_map_t;

typedef struct pn_hash_t pn_hash_t;

typedef void *(*pn_iterator_next_t)(void *state);

typedef struct pn_iterator_t pn_iterator_t;

typedef struct pn_record_t pn_record_t;



struct pn_class_t {
  const char *name;
  const pn_cid_t cid;
  void *(*newinst)(const pn_class_t *, size_t);
  void (*initialize)(void *);
  void (*incref)(void *);
  void (*decref)(void *);
  int (*refcount)(void *);
  void (*finalize)(void *);
  void (*free)(void *);
  const pn_class_t *(*reify)(void *);
  uintptr_t (*hashcode)(void *);
  intptr_t (*compare)(void *, void *);
  int (*inspect)(void *, pn_string_t *);
};

extern const pn_class_t PN_OBJECT[];

extern const pn_class_t PN_VOID[];

extern const pn_class_t PN_WEAKREF[];


pn_cid_t pn_class_id(const pn_class_t *clazz);
const char *pn_class_name(const pn_class_t *clazz);
void *pn_class_new(const pn_class_t *clazz, size_t size);


void *pn_class_incref(const pn_class_t *clazz, void *object);
int pn_class_refcount(const pn_class_t *clazz, void *object);
int pn_class_decref(const pn_class_t *clazz, void *object);

void pn_class_free(const pn_class_t *clazz, void *object);

const pn_class_t *pn_class_reify(const pn_class_t *clazz, void *object);
uintptr_t pn_class_hashcode(const pn_class_t *clazz, void *object);
intptr_t pn_class_compare(const pn_class_t *clazz, void *a, void *b);
bool pn_class_equals(const pn_class_t *clazz, void *a, void *b);
int pn_class_inspect(const pn_class_t *clazz, void *object, pn_string_t *dst);

void *pn_void_new(const pn_class_t *clazz, size_t size);
void pn_void_incref(void *object);
void pn_void_decref(void *object);
int pn_void_refcount(void *object);
uintptr_t pn_void_hashcode(void *object);
intptr_t pn_void_compare(void *a, void *b);
int pn_void_inspect(void *object, pn_string_t *dst);

void *pn_object_new(const pn_class_t *clazz, size_t size);
const pn_class_t *pn_object_reify(void *object);
void pn_object_incref(void *object);
int pn_object_refcount(void *object);
void pn_object_decref(void *object);
void pn_object_free(void *object);

void *pn_incref(void *object);
int pn_decref(void *object);
int pn_refcount(void *object);
void pn_free(void *object);
const pn_class_t *pn_class(void* object);
uintptr_t pn_hashcode(void *object);
intptr_t pn_compare(void *a, void *b);
bool pn_equals(void *a, void *b);
int pn_inspect(void *object, pn_string_t *dst);

#define PN_REFCOUNT ...

pn_list_t *pn_list(const pn_class_t *clazz, size_t capacity);
size_t pn_list_size(pn_list_t *list);
void *pn_list_get(pn_list_t *list, int index);
void pn_list_set(pn_list_t *list, int index, void *value);
int pn_list_add(pn_list_t *list, void *value);
void *pn_list_pop(pn_list_t *list);
ssize_t pn_list_index(pn_list_t *list, void *value);
bool pn_list_remove(pn_list_t *list, void *value);
void pn_list_del(pn_list_t *list, int index, int n);
void pn_list_clear(pn_list_t *list);
void pn_list_iterator(pn_list_t *list, pn_iterator_t *iter);
void pn_list_minpush(pn_list_t *list, void *value);
void *pn_list_minpop(pn_list_t *list);

#define PN_REFCOUNT_KEY ...

#define PN_REFCOUNT_VALUE ...

pn_map_t *pn_map(const pn_class_t *key, const pn_class_t *value,
                           size_t capacity, float load_factor);
size_t pn_map_size(pn_map_t *map);
int pn_map_put(pn_map_t *map, void *key, void *value);
void *pn_map_get(pn_map_t *map, void *key);
void pn_map_del(pn_map_t *map, void *key);
pn_handle_t pn_map_head(pn_map_t *map);
pn_handle_t pn_map_next(pn_map_t *map, pn_handle_t entry);
void *pn_map_key(pn_map_t *map, pn_handle_t entry);
void *pn_map_value(pn_map_t *map, pn_handle_t entry);

pn_hash_t *pn_hash(const pn_class_t *clazz, size_t capacity, float load_factor);
size_t pn_hash_size(pn_hash_t *hash);
int pn_hash_put(pn_hash_t *hash, uintptr_t key, void *value);
void *pn_hash_get(pn_hash_t *hash, uintptr_t key);
void pn_hash_del(pn_hash_t *hash, uintptr_t key);
pn_handle_t pn_hash_head(pn_hash_t *hash);
pn_handle_t pn_hash_next(pn_hash_t *hash, pn_handle_t entry);
uintptr_t pn_hash_key(pn_hash_t *hash, pn_handle_t entry);
void *pn_hash_value(pn_hash_t *hash, pn_handle_t entry);

pn_string_t *pn_string(const char *bytes);
pn_string_t *pn_stringn(const char *bytes, size_t n);
const char *pn_string_get(pn_string_t *string);
size_t pn_string_size(pn_string_t *string);
int pn_string_set(pn_string_t *string, const char *bytes);
int pn_string_setn(pn_string_t *string, const char *bytes, size_t n);
ssize_t pn_string_put(pn_string_t *string, char *dst);
void pn_string_clear(pn_string_t *string);
int pn_string_format(pn_string_t *string, const char *format, ...);

int pn_string_vformat(pn_string_t *string, const char *format, ...);
int pn_string_addf(pn_string_t *string, const char *format, ...);

int pn_string_vaddf(pn_string_t *string, const char *format, ...);
int pn_string_grow(pn_string_t *string, size_t capacity);
char *pn_string_buffer(pn_string_t *string);
size_t pn_string_capacity(pn_string_t *string);
int pn_string_resize(pn_string_t *string, size_t size);
int pn_string_copy(pn_string_t *string, pn_string_t *src);

pn_iterator_t *pn_iterator(void);
void *pn_iterator_start(pn_iterator_t *iterator,
                                  pn_iterator_next_t next, size_t size);
void *pn_iterator_next(pn_iterator_t *iterator);



pn_record_t *pn_record(void);
void pn_record_def(pn_record_t *record, pn_handle_t key, const pn_class_t *clazz);
bool pn_record_has(pn_record_t *record, pn_handle_t key);
void *pn_record_get(pn_record_t *record, pn_handle_t key);
void pn_record_set(pn_record_t *record, pn_handle_t key, void *value);
void pn_record_clear(pn_record_t *record);
"""

cid_h = """

typedef enum {
  CID_pn_object = 1,
  CID_pn_void,
  CID_pn_weakref,

  CID_pn_string,
  CID_pn_list,
  CID_pn_map,
  CID_pn_hash,
  CID_pn_record,

  CID_pn_collector,
  CID_pn_event,

  CID_pn_buffer,
  CID_pn_error,
  CID_pn_data,

  CID_pn_connection,
  CID_pn_session,
  CID_pn_link,
  CID_pn_delivery,
  CID_pn_transport,

  CID_pn_message,

  CID_pn_reactor,
  CID_pn_handler,
  CID_pn_timer,
  CID_pn_task,

  CID_pn_io,
  CID_pn_selector,
  CID_pn_selectable,

  CID_pn_url,
  CID_pn_strdup,

  CID_pn_listener,
  CID_pn_proactor,

  CID_pn_listener_socket,
  CID_pn_raw_connection
} pn_cid_t;
"""


ssl_h = """
typedef struct pn_ssl_domain_t pn_ssl_domain_t;

typedef struct pn_ssl_t pn_ssl_t;

typedef enum {
  PN_SSL_MODE_CLIENT = ...,
  PN_SSL_MODE_SERVER
} pn_ssl_mode_t;

typedef enum {
  PN_SSL_RESUME_UNKNOWN,
  PN_SSL_RESUME_NEW,
  PN_SSL_RESUME_REUSED
} pn_ssl_resume_status_t;

bool pn_ssl_present( void );

pn_ssl_domain_t *pn_ssl_domain(pn_ssl_mode_t mode);

void pn_ssl_domain_free(pn_ssl_domain_t *domain);
int  pn_ssl_domain_set_credentials(pn_ssl_domain_t *domain,
                                            const char *credential_1,
                                            const char *credential_2,
                                            const char *password);
int pn_ssl_domain_set_trusted_ca_db(pn_ssl_domain_t *domain,
                                const char *certificate_db);

typedef enum {
  PN_SSL_VERIFY_NULL = ...,   
  PN_SSL_VERIFY_PEER,
  PN_SSL_ANONYMOUS_PEER,
  PN_SSL_VERIFY_PEER_NAME
} pn_ssl_verify_mode_t;

int pn_ssl_domain_set_peer_authentication(pn_ssl_domain_t *domain,
                                                    const pn_ssl_verify_mode_t mode,
                                                    const char *trusted_CAs);
int pn_ssl_domain_set_protocols(pn_ssl_domain_t *domain, const char *protocols);
int pn_ssl_domain_set_ciphers(pn_ssl_domain_t *domain, const char *ciphers);
int pn_ssl_domain_allow_unsecured_client(pn_ssl_domain_t *domain);

pn_ssl_t *pn_ssl(pn_transport_t *transport);
int pn_ssl_init(pn_ssl_t *ssl,
                          pn_ssl_domain_t *domain,
                          const char *session_id);
bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *buffer, size_t size);

int pn_ssl_get_ssf(pn_ssl_t *ssl);
bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *buffer, size_t size);
pn_ssl_resume_status_t pn_ssl_resume_status(pn_ssl_t *ssl);
int pn_ssl_set_peer_hostname(pn_ssl_t *ssl, const char *hostname);
int pn_ssl_get_peer_hostname(pn_ssl_t *ssl, char *hostname, size_t *bufsize);

const char* pn_ssl_get_remote_subject(pn_ssl_t *ssl);

typedef enum {
  PN_SSL_CERT_SUBJECT_COUNTRY_NAME,
  PN_SSL_CERT_SUBJECT_STATE_OR_PROVINCE,
  PN_SSL_CERT_SUBJECT_CITY_OR_LOCALITY,
  PN_SSL_CERT_SUBJECT_ORGANIZATION_NAME,
  PN_SSL_CERT_SUBJECT_ORGANIZATION_UNIT,
  PN_SSL_CERT_SUBJECT_COMMON_NAME
} pn_ssl_cert_subject_subfield;

typedef enum {
  PN_SSL_SHA1,
  PN_SSL_SHA256,
  PN_SSL_SHA512, 
  PN_SSL_MD5     
} pn_ssl_hash_alg;

int pn_ssl_get_cert_fingerprint(pn_ssl_t *ssl0,
                                          char *fingerprint,
                                          size_t fingerprint_length,
                                          pn_ssl_hash_alg hash_alg);
const char* pn_ssl_get_remote_subject_subfield(pn_ssl_t *ssl0, pn_ssl_cert_subject_subfield field);

"""

disposition_h = """
typedef struct pn_disposition_t pn_disposition_t;


"""


delivery_h = """
#define PROTON_DELIVERY_H 1

typedef pn_bytes_t pn_delivery_tag_t;


 pn_delivery_tag_t pn_dtag(const char *bytes, size_t size);
 pn_delivery_t *pn_delivery(pn_link_t *link, pn_delivery_tag_t tag);
 void *pn_delivery_get_context(pn_delivery_t *delivery);
 void pn_delivery_set_context(pn_delivery_t *delivery, void *context);
 pn_record_t *pn_delivery_attachments(pn_delivery_t *delivery);
 pn_delivery_tag_t pn_delivery_tag(pn_delivery_t *delivery);
 pn_link_t *pn_delivery_link(pn_delivery_t *delivery);
 pn_disposition_t *pn_delivery_local(pn_delivery_t *delivery);
 uint64_t pn_delivery_local_state(pn_delivery_t *delivery);
 pn_disposition_t *pn_delivery_remote(pn_delivery_t *delivery);
 uint64_t pn_delivery_remote_state(pn_delivery_t *delivery);
 bool pn_delivery_settled(pn_delivery_t *delivery);
 size_t pn_delivery_pending(pn_delivery_t *delivery);
 bool pn_delivery_partial(pn_delivery_t *delivery);
 bool pn_delivery_aborted(pn_delivery_t *delivery);
 bool pn_delivery_writable(pn_delivery_t *delivery);
 bool pn_delivery_readable(pn_delivery_t *delivery);
 bool pn_delivery_updated(pn_delivery_t *delivery);
 void pn_delivery_update(pn_delivery_t *delivery, uint64_t state);
 void pn_delivery_clear(pn_delivery_t *delivery);
 bool pn_delivery_current(pn_delivery_t *delivery);
 void pn_delivery_abort(pn_delivery_t *delivery);
 void pn_delivery_settle(pn_delivery_t *delivery);
 void pn_delivery_dump(pn_delivery_t *delivery);
 bool pn_delivery_buffered(pn_delivery_t *delivery);
 pn_delivery_t *pn_work_head(pn_connection_t *connection);
 pn_delivery_t *pn_work_next(pn_delivery_t *delivery);
"""




# TODO:
def run_cffi_compile(output_file):
    ffi_builder = FFI()
    ffi_builder.set_source(
        module_name="_proton_core",
        source="""
        #include <proton/import_export.h>
        #include <proton/type_compat.h>
        #include <stdarg.h>
        #include <stddef.h>
        #include <proton/types.h>
        #include <proton/object.h>
        #include <proton/error.h>
        #include <proton/codec.h>
        #include <proton/message.h>
        #include <proton/logger.h>
        #include <proton/condition.h>
        #include <proton/transport.h>
        #include <proton/sasl.h>
        #include <proton/ssl.h>
        #include <proton/disposition.h>
        #include <proton/delivery.h>
        #include <proton/event.h>
        #include <proton/terminus.h>
        #include <proton/connection.h>
        #include <proton/link.h>
        #include <proton/session.h>

        extern const pn_class_t PN_PYREF[];

        #define CID_pn_pyref CID_pn_void
        #define pn_pyref_new NULL
        #define pn_pyref_initialize NULL
        #define pn_pyref_finalize NULL
        #define pn_pyref_free NULL
        #define pn_pyref_hashcode pn_void_hashcode
        #define pn_pyref_compare pn_void_compare
        #define pn_pyref_inspect pn_void_inspect

  static void pn_pyref_incref(void *object) {
    PyObject* p = (PyObject*) object;
    // SWIG_PYTHON_THREAD_BEGIN_BLOCK;
    //Py_XINCREF(p);
    // SWIG_PYTHON_THREAD_END_BLOCK;
  }

  static void pn_pyref_decref(void *object) {
    PyObject* p = (PyObject*) object;
    // SWIG_PYTHON_THREAD_BEGIN_BLOCK;
    //Py_XDECREF(p);
    // SWIG_PYTHON_THREAD_END_BLOCK;
  }

  static int pn_pyref_refcount(void *object) {
    return 1;
  }

  static const pn_class_t *pn_pyref_reify(void *object) {
    return PN_PYREF;
  }

        static const char _PN_HANDLE_PNI_PYTRACER;
        static const pn_handle_t PNI_PYTRACER = (pn_handle_t) &_PN_HANDLE_PNI_PYTRACER; 

        const pn_class_t PN_PYREF[] = {PN_METACLASS(pn_pyref)};

        #define PN_LEGCTX (pn_handle_t 0)


        void pn_pytracer(pn_transport_t *transport, const char *message) {
          pn_tracer_t pytracer = (void *) pn_record_get(pn_transport_attachments(transport), PNI_PYTRACER);
          (*pytracer)(transport, message);
        }

        void *pn_transport_get_pytracer(pn_transport_t *transport) {
          pn_record_t *record = pn_transport_attachments(transport);
          void *obj = (void *)pn_record_get(record, PNI_PYTRACER);
          if (obj) {
            return obj;
          } else {
            NULL;
          }
        }

        void pn_transport_set_pytracer(pn_transport_t *transport, void *obj) {
          pn_record_t *record = pn_transport_attachments(transport);
          pn_record_def(record, PNI_PYTRACER, PN_PYREF);
          pn_record_set(record, PNI_PYTRACER, obj);
          pn_transport_set_tracer(transport, pn_pytracer);
        }


        """,

        #  ----------------------------
        # libraries=['qpid-proton-core'],
        # library_dirs=["/home/ArunaSudhan/OpenSourceProjects/RHOCS/qpid-proton/build/install/lib64/"],
        # include_dirs=['/home/ArunaSudhan/OpenSourceProjects/RHOCS/qpid-proton/c/include']
    )
    ffi_builder.cdef(cstdlib)
    ffi_builder.cdef(cid_h)
    ffi_builder.cdef(type_h)
    #  Error is raised from the object h file parsing
    ffi_builder.cdef(object_h)
    ffi_builder.cdef(sasl_h)
    ffi_builder.cdef(error_h)
    ffi_builder.cdef(codec_h)
    ffi_builder.cdef(message_h)
    ffi_builder.cdef(logger_h)
    ffi_builder.cdef(condition_h)
    ffi_builder.cdef(transport_h)
    ffi_builder.cdef(ssl_h)
    ffi_builder.cdef(disposition_h)
    ffi_builder.cdef(delivery_h)
    ffi_builder.cdef(event_h)
    ffi_builder.cdef(terminus_h)
    ffi_builder.cdef(connection_h)
    ffi_builder.cdef(session_h)
    ffi_builder.cdef(link_h)

    ffi_builder.cdef(
      """

      extern const pn_class_t PN_PYREF[];


      static const char _PN_HANDLE_PNI_PYTRACER;
      static const pn_handle_t PNI_PYTRACER = (pn_handle_t) &_PN_HANDLE_PNI_PYTRACER; 

      void pn_pytracer(pn_transport_t *transport, const char *message);
      void *pn_transport_get_pytracer(pn_transport_t *transport);
      void pn_transport_set_pytracer(pn_transport_t *transport, void *obj);
      
      // callback
      //extern "Python" void *pn_void2py(*object);
      """
    )

    ffi_builder.emit_c_code(output_file)
    #  ----------------------------
    # ffi_builder.compile(verbose=True)



def main():
    parser = argparse.ArgumentParser(
        description="Compiling c function using cffi"
    )
    parser.add_argument(
        "output_file",
        type=str,
        help="ouput file to write the compiled code"
    )
    args = parser.parse_args()
    run_cffi_compile(args.output_file)


if __name__ == "__main__":
    main()
