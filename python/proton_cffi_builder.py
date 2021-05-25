from cffi import FFI

import argparse


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
  
    int pn_data_vfill(pn_data_t *data, const char *fmt, ...);
    
    int pn_data_fill(pn_data_t *data, const char *fmt, ...);
    
    int pn_data_vscan(pn_data_t *data, const char *fmt, ...);
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

object_h = """

typedef const void* pn_handle_t;

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
    int pn_error_vformat(pn_error_t *error, int code, const char *fmt, ...);
    int pn_error_format(pn_error_t *error, int code, const char *fmt, ...);
    int pn_error_code(pn_error_t *error);

    const char *pn_error_text(pn_error_t *error);

    int pn_error_copy(pn_error_t *error, pn_error_t *src);

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

        """,
        # libraries=['qpid-proton-core'],
        # library_dirs=["/home/ArunaSudhan/OpenSourceProjects/RHOCS/qpid-proton/build/install/lib64/"],
        # include_dirs=['/home/ArunaSudhan/OpenSourceProjects/RHOCS/qpid-proton/c/include']
    )

    ffi_builder.cdef(  type_h + object_h + error_h + codec_t +  message_h)
    ffi_builder.emit_c_code(output_file)
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