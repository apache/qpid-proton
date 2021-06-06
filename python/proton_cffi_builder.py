import argparse

from cffi import FFI

cstdlib = """
typedef struct { ...; } va_list;
"""

codec_t = """

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
 
 // void pn_transport_set_tracer(pn_transport_t *transport, pn_tracer_t tracer);
 // pn_tracer_t pn_transport_get_tracer(pn_transport_t *transport);
 
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

logger_private_h = """

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
void pni_logger_vlogf(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t severity, const char *fmt, ...);
void pni_logger_log_data(pn_logger_t *logger, pn_log_subsystem_t subsystem, pn_log_level_t severity, const char *msg, const char *bytes, size_t size);

#define PN_SHOULD_LOG ...

#define PN_LOG ...

#define PN_LOG_DEFAULT ...

#define PN_LOG_DATA ...
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

#define PN_CLASSCLASS ...

#define PN_CLASSDEF ...

#define PN_CLASS ...

#define PN_METACLASS ...

#define PN_STRUCT_CLASSDEF ...

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

#define PN_LEGCTX ...

#define PN_HANDLE ...

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


def run_cffi_compile(output_file):
    ffi_builder = FFI()
    ffi_builder.set_source(
        module_name="_proton_core",
        source="""
        #include <stdarg.h>
        #include <proton/types.h>
        #include <proton/error.h>
        #include <proton/codec.h>
        #include <proton/message.h>
        #include <proton/logger.h>
        #include <proton/condition.h>

#include <proton/import_export.h>
#include <proton/type_compat.h>
        """,

        #  ----------------------------
        libraries=['qpid-proton-core'],
        library_dirs=["/home/ArunaSudhan/OpenSourceProjects/RHOCS/qpid-proton/build/install/lib64/"],
        include_dirs=['/home/ArunaSudhan/OpenSourceProjects/RHOCS/qpid-proton/c/include']
    )

    ffi_builder.cdef(cstdlib + type_h + object_h + error_h + codec_t + message_h)
    ffi_builder.emit_c_code(output_file)
    # ffi_builder.compile(verbose=True)

    ffi_builder.cdef(cid_h)
    ffi_builder.cdef(type_h)
    ffi_builder.cdef(object_h)
    # ffi_builder.cdef(sasl_h)
    # ffi_builder.cdef(error_h)
    # ffi_builder.cdef(codec_t)
    # ffi_builder.cdef(message_h)
    ffi_builder.cdef(logger_h)
    ffi_builder.cdef(logger_private_h)
    # ffi_builder.cdef(condition_h)
    # ffi_builder.cdef(transport_h)
    
    # ffi_builder.emit_c_code(output_file)
    #  ----------------------------
    ffi_builder.compile(verbose=True)


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
