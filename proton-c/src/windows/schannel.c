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

/*
 * SChannel is designed to encrypt and decrypt data in place.  So a
 * given buffer is expected to sometimes contain encrypted data,
 * sometimes decrypted data, and occasionally both.  Outgoing buffers
 * need to reserve space for the TLS header and trailer.  Read
 * operations need to ignore the same headers and trailers from
 * incoming buffers.  Outgoing is simple because we choose record
 * boundaries.  Incoming is complicated by handling incomplete TLS
 * records, and buffering contiguous data for the app layer that may
 * span many records.  A lazy double buffering system is used for
 * the latter.
 */

#include <proton/ssl.h>
#include <proton/engine.h>
#include "engine/engine-internal.h"
#include "platform.h"
#include "util.h"

#include <assert.h>

// security.h needs to see this to distinguish from kernel use.
#include <windows.h>
#define SECURITY_WIN32
#include <security.h>
#include <Schnlsp.h>
#undef SECURITY_WIN32


/** @file
 * SSL/TLS support API.
 *
 * This file contains an SChannel-based implemention of the SSL/TLS API for Windows platforms.
 */

static void ssl_log_error(const char *fmt, ...);
static void ssl_log(pn_ssl_t *ssl, const char *fmt, ...);
static void ssl_log_error_status(HRESULT status, const char *fmt, ...);

/*
 * win_credential_t: SChannel context that must accompany TLS connections.
 *
 * SChannel attempts session resumption for shared CredHandle objects.
 * To mimic openssl behavior, server CredHandle handles must be shared
 * by derived connections, client CredHandle handles must be unique
 * when app's session_id is null and tracked for reuse otherwise
 * (TODO).
 *
 * Ref counted by parent ssl_domain_t and each derived connection.
 */
struct win_credential_t {
  pn_ssl_mode_t mode;
  PCCERT_CONTEXT cert_context;  // Particulars of the certificate (if any)
  CredHandle cred_handle;       // Bound session parameters, certificate, CAs, verification_mode
};

#define win_credential_compare NULL
#define win_credential_inspect NULL
#define win_credential_hashcode NULL

static void win_credential_initialize(void *object)
{
  win_credential_t *c = (win_credential_t *) object;
  SecInvalidateHandle(&c->cred_handle);
  c->cert_context = 0;
}

static void win_credential_finalize(void *object)
{
  win_credential_t *c = (win_credential_t *) object;
  if (SecIsValidHandle(&c->cred_handle))
    FreeCredentialsHandle(&c->cred_handle);
  if (c->cert_context)
    CertFreeCertificateContext(c->cert_context);
}

static win_credential_t *win_credential(pn_ssl_mode_t m)
{
  static const pn_cid_t CID_win_credential = CID_pn_void;
  static const pn_class_t clazz = PN_CLASS(win_credential);
  win_credential_t *c = (win_credential_t *) pn_class_new(&clazz, sizeof(win_credential_t));
  c->mode = m;
  return c;
}

static int win_credential_load_cert(win_credential_t *cred, const char *store_name, const char *cert_name, const char *passwd)
{
  if (!store_name)
    return -2;

  char *buf = NULL;
  DWORD sys_store_type = 0;
  HCERTSTORE cert_store = 0;

  if (store_name) {
    if (strncmp(store_name, "ss:", 3) == 0) {
      store_name += 3;
      sys_store_type = CERT_SYSTEM_STORE_CURRENT_USER;
    }
    else if (strncmp(store_name, "lmss:", 5) == 0) {
      store_name += 5;
      sys_store_type = CERT_SYSTEM_STORE_LOCAL_MACHINE;
    }
  }

  if (sys_store_type) {
    // Opening a system store, names are not case sensitive.
    // Map confusing GUI name to actual registry store name.
    if (pni_eq_nocase(store_name, "personal"))
      store_name= "my";
    cert_store = CertOpenStore(CERT_STORE_PROV_SYSTEM_A, 0, NULL,
                                CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG |
                                sys_store_type, store_name);
    if (!cert_store) {
      ssl_log_error_status(GetLastError(), "Failed to open system certificate store %s", store_name);
      return -3;
    }
  } else {
    // PKCS#12 file
    HANDLE cert_file = CreateFile(store_name, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == cert_file) {
      HRESULT status = GetLastError();
      ssl_log_error_status(status, "Failed to open the file holding the private key: %s", store_name);
      return -4;
    }
    DWORD nread = 0L;
    const DWORD file_size = GetFileSize(cert_file, NULL);
    if (INVALID_FILE_SIZE != file_size)
      buf = (char *) malloc(file_size);
    if (!buf || !ReadFile(cert_file, buf, file_size, &nread, NULL)
        || file_size != nread) {
      HRESULT status = GetLastError();
      CloseHandle(cert_file);
      free(buf);
      ssl_log_error_status(status, "Reading the private key from file failed %s", store_name);
      return -5;
    }
    CloseHandle(cert_file);

    CRYPT_DATA_BLOB blob;
    blob.cbData = nread;
    blob.pbData = (BYTE *) buf;

    wchar_t *pwUCS2 = NULL;
    int pwlen = 0;
    if (passwd) {
      // convert passwd to null terminated wchar_t (Windows UCS2)
      pwlen = strlen(passwd);
      pwUCS2 = (wchar_t *) calloc(pwlen + 1, sizeof(wchar_t));
      int nwc = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, passwd, pwlen, &pwUCS2[0], pwlen);
      if (!nwc) {
        ssl_log_error_status(GetLastError(), "Error converting password from UTF8");
        free(buf);
        free(pwUCS2);
        return -6;
      }
    }

    cert_store = PFXImportCertStore(&blob, pwUCS2, 0);
    if (pwUCS2) {
      SecureZeroMemory(pwUCS2, pwlen * sizeof(wchar_t));
      free(pwUCS2);
    }
    if (cert_store == NULL) {
      ssl_log_error_status(GetLastError(), "Failed to import the file based certificate store");
      free(buf);
      return -7;
    }
  }

  // find friendly name that matches cert_name, or sole certificate
  PCCERT_CONTEXT tmpctx = NULL;
  PCCERT_CONTEXT found_ctx = NULL;
  int cert_count = 0;
  int name_len = cert_name ? strlen(cert_name) : 0;
  char *fn = name_len ? (char *) malloc(name_len + 1) : 0;
  while (tmpctx = CertEnumCertificatesInStore(cert_store, tmpctx)) {
    cert_count++;
    if (cert_name) {
      DWORD len = CertGetNameString(tmpctx, CERT_NAME_FRIENDLY_DISPLAY_TYPE,
                                    0, NULL, NULL, 0);
      if (len != name_len + 1)
        continue;
      CertGetNameString(tmpctx, CERT_NAME_FRIENDLY_DISPLAY_TYPE,
                        0, NULL, fn, len);
      if (!strcmp(cert_name, fn)) {
        found_ctx = tmpctx;
        tmpctx= NULL;
        break;
      }
    } else {
      // Test for single certificate
      if (cert_count == 1) {
        found_ctx = CertDuplicateCertificateContext(tmpctx);
      } else {
        ssl_log_error("Multiple certificates to choose from certificate store %s\n", store_name);
        found_ctx = NULL;
        break;
      }
    }
  }

  if (tmpctx) {
    CertFreeCertificateContext(tmpctx);
    tmpctx = false;
  }
  if (!found_ctx && cert_name && cert_count == 1)
    ssl_log_error("Could not find certificate %s in store %s\n", cert_name, store_name);
  cred->cert_context = found_ctx;

  free(buf);
  free(fn);
  CertCloseStore(cert_store, 0);
  return found_ctx ? 0 : -8;
}


static CredHandle win_credential_cred_handle(win_credential_t *cred, pn_ssl_verify_mode_t verify_mode,
                                             const char *session_id, SECURITY_STATUS *status)
{
  if (cred->mode == PN_SSL_MODE_SERVER && SecIsValidHandle(&cred->cred_handle)) {
    *status = SEC_E_OK;
    return cred->cred_handle;  // Server always reuses cached value
  }
  // TODO: if (is_client && session_id != NULL) create or use cached value based on
  // session_id+server_host_name (per domain? reclaimed after X hours?)

  CredHandle tmp_handle;
  SecInvalidateHandle(&tmp_handle);
  TimeStamp expiry;  // Not used
  SCHANNEL_CRED descriptor;
  memset(&descriptor, 0, sizeof(descriptor));

  descriptor.dwVersion = SCHANNEL_CRED_VERSION;
  descriptor.dwFlags = SCH_CRED_NO_DEFAULT_CREDS;
  if (cred->mode == PN_SSL_MODE_CLIENT && verify_mode == PN_SSL_ANONYMOUS_PEER)
    descriptor.dwFlags |= SCH_CRED_MANUAL_CRED_VALIDATION;
  if (cred->cert_context != NULL) {
    // assign the certificate into the credentials
    descriptor.paCred = &cred->cert_context;
    descriptor.cCreds = 1;
  }

  ULONG direction = (cred->mode == PN_SSL_MODE_SERVER) ? SECPKG_CRED_INBOUND : SECPKG_CRED_OUTBOUND;
  *status = AcquireCredentialsHandle(NULL, UNISP_NAME, direction, NULL,
                                               &descriptor, NULL, NULL, &tmp_handle, &expiry);
  if (cred->mode == PN_SSL_MODE_SERVER && *status == SEC_E_OK)
    cred->cred_handle = tmp_handle;

  return tmp_handle;
}

static bool win_credential_has_certificate(win_credential_t *cred)
{
  if (!cred) return false;
  return (cred->cert_context != NULL);
}

#define SSL_DATA_SIZE 16384
#define SSL_BUF_SIZE (SSL_DATA_SIZE + 5 + 2048 + 32)

typedef enum { UNKNOWN_CONNECTION, SSL_CONNECTION, CLEAR_CONNECTION } connection_mode_t;
typedef struct pn_ssl_session_t pn_ssl_session_t;

struct pn_ssl_domain_t {
  int ref_count;
  pn_ssl_mode_t mode;
  bool has_ca_db;       // true when CA database configured
  pn_ssl_verify_mode_t verify_mode;
  bool allow_unsecured;
  win_credential_t *cred;
};

typedef enum { CREATED, CLIENT_HELLO, NEGOTIATING,
               RUNNING, SHUTTING_DOWN, SSL_CLOSED } ssl_state_t;

struct pn_ssl_t {
  pn_transport_t   *transport;
  pn_io_layer_t    *io_layer;
  pn_ssl_domain_t  *domain;
  const char    *session_id;
  const char *peer_hostname;
  ssl_state_t state;

  bool queued_shutdown;
  bool ssl_closed;            // shutdown complete, or SSL error
  ssize_t app_input_closed;   // error code returned by upper layer process input
  ssize_t app_output_closed;  // error code returned by upper layer process output

  // OpenSSL hides the protocol envelope bytes, SChannel has them in-line.
  char *sc_outbuf;     // SChannel output buffer
  size_t sc_out_size;
  size_t sc_out_count;
  char *network_outp;   // network ready bytes within sc_outbuf
  size_t network_out_pending;

  char *sc_inbuf;      // SChannel input buffer
  size_t sc_in_size;
  size_t sc_in_count;
  bool sc_in_incomplete;

  char *inbuf_extra;    // Still encrypted data from following Record(s)
  size_t extra_count;

  char *in_data;          // Just the plaintext data part of sc_inbuf, decrypted in place
  size_t in_data_size;
  size_t in_data_count;
  bool decrypting;
  size_t max_data_size;  // computed in the handshake

  pn_bytes_t app_inbytes; // Virtual decrypted datastream, presented to app layer

  pn_buffer_t *inbuf2;    // Second input buf if longer contiguous bytes needed
  bool double_buffered;

  bool sc_input_shutdown;

  pn_trace_t trace;

  CredHandle cred_handle;
  CtxtHandle ctxt_handle;
  SecPkgContext_StreamSizes sc_sizes;
  win_credential_t *cred;
};

struct pn_ssl_session_t {
  const char       *id;
// TODO
  pn_ssl_session_t *ssn_cache_next;
  pn_ssl_session_t *ssn_cache_prev;
};


static ssize_t process_input_ssl( pn_io_layer_t *io_layer, const char *input_data, size_t len);
static ssize_t process_output_ssl( pn_io_layer_t *io_layer, char *input_data, size_t len);
static ssize_t process_input_unknown( pn_io_layer_t *io_layer, const char *input_data, size_t len);
static ssize_t process_output_unknown( pn_io_layer_t *io_layer, char *input_data, size_t len);
static ssize_t process_input_done(pn_io_layer_t *io_layer, const char *input_data, size_t len);
static ssize_t process_output_done(pn_io_layer_t *io_layer, char *input_data, size_t len);
static connection_mode_t check_for_ssl_connection( const char *data, size_t len );
static pn_ssl_session_t *ssn_cache_find( pn_ssl_domain_t *, const char * );
static void ssl_session_free( pn_ssl_session_t *);
static size_t buffered_output( pn_io_layer_t *io_layer );
static size_t buffered_input( pn_io_layer_t *io_layer );
static void start_ssl_shutdown(pn_ssl_t *ssl);
static void rewind_sc_inbuf(pn_ssl_t *ssl);
static bool grow_inbuf2(pn_ssl_t *ssl, size_t minimum_size);


// @todo: used to avoid littering the code with calls to printf...
static void ssl_log_error(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fflush(stderr);
}

// @todo: used to avoid littering the code with calls to printf...
static void ssl_log(pn_ssl_t *ssl, const char *fmt, ...)
{
  if (PN_TRACE_DRV & ssl->trace) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
  }
}

static void ssl_log_error_status(HRESULT status, const char *fmt, ...)
{
  char buf[512];
  va_list ap;

  if (fmt) {
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
  }

  if (FormatMessage(FORMAT_MESSAGE_MAX_WIDTH_MASK | FORMAT_MESSAGE_FROM_SYSTEM,
                    0, status, 0, buf, sizeof(buf), 0))
    ssl_log_error(" : %s\n", buf);
  else
    fprintf(stderr, "pn internal Windows error: %lu\n", GetLastError());

  fflush(stderr);
}

static void ssl_log_clear_data(pn_ssl_t *ssl, const char *data, size_t len)
{
  if (PN_TRACE_RAW & ssl->trace) {
    fprintf(stderr, "SSL decrypted data: \"");
    pn_fprint_data( stderr, data, len );
    fprintf(stderr, "\"\n");
  }
}

static size_t _pni_min(size_t a, size_t b)
{
  return (a < b) ? a : b;
}

// unrecoverable SSL failure occured, notify transport and generate error code.
static int ssl_failed(pn_ssl_t *ssl, const char *reason)
{
  char buf[512] = "Unknown error.";
  if (!reason) {
    HRESULT status = GetLastError();

    FormatMessage(FORMAT_MESSAGE_MAX_WIDTH_MASK | FORMAT_MESSAGE_FROM_SYSTEM,
                  0, status, 0, buf, sizeof(buf), 0);
    reason = buf;
  }
  ssl->ssl_closed = true;
  ssl->app_input_closed = ssl->app_output_closed = PN_EOS;
  ssl->state = SSL_CLOSED;
  pn_do_error(ssl->transport, "amqp:connection:framing-error", "SSL Failure: %s", reason);
  return PN_EOS;
}

/* match the DNS name pattern from the peer certificate against our configured peer
   hostname */
static bool match_dns_pattern( const char *hostname,
                               const char *pattern, int plen )
{
  return false; // TODO
}


static pn_ssl_session_t *ssn_cache_find( pn_ssl_domain_t *domain, const char *id )
{
// TODO:
  return NULL;
}

static void ssl_session_free( pn_ssl_session_t *ssn)
{
  if (ssn) {
    if (ssn->id) free( (void *)ssn->id );
    free( ssn );
  }
}


/** Public API - visible to application code */

pn_ssl_domain_t *pn_ssl_domain( pn_ssl_mode_t mode )
{
  pn_ssl_domain_t *domain = (pn_ssl_domain_t *) calloc(1, sizeof(pn_ssl_domain_t));
  if (!domain) return NULL;

  domain->ref_count = 1;
  domain->mode = mode;
  switch(mode) {
  case PN_SSL_MODE_CLIENT:
  case PN_SSL_MODE_SERVER:
    break;

  default:
    ssl_log_error("Invalid mode for pn_ssl_mode_t: %d\n", mode);
    free(domain);
    return NULL;
  }
  domain->cred = win_credential(mode);
  return domain;
}

void pn_ssl_domain_free( pn_ssl_domain_t *domain )
{
  if (!domain) return;

  if (--domain->ref_count == 0) {
    pn_decref(domain->cred);
    free(domain);
  }
}


int pn_ssl_domain_set_credentials( pn_ssl_domain_t *domain,
                               const char *certificate_file,
                               const char *private_key_file,
                               const char *password)
{
  if (!domain) return -1;

  if (win_credential_has_certificate(domain->cred)) {
    // Need a new win_credential_t to hold new certificate
    pn_decref(domain->cred);
    domain->cred = win_credential(domain->mode);
    if (!domain->cred)
      return -1;
  }
  return win_credential_load_cert(domain->cred, certificate_file, private_key_file, password);
}


int pn_ssl_domain_set_trusted_ca_db(pn_ssl_domain_t *domain,
                                    const char *certificate_db)
{
  if (!domain) return -1;

  if (certificate_db && !pni_eq_nocase(certificate_db, "ss:root") &&
      !pni_eq_nocase(certificate_db, "lmss:root"))
    // TODO: handle more than just the main system trust store
    return -1;
  if (!certificate_db && !domain->has_ca_db)
    return 0;   // No change
  if (certificate_db && domain->has_ca_db)
    return 0;   // NO change

  win_credential_t *new_cred = win_credential(domain->mode);
  if (!new_cred)
    return -1;
  new_cred->cert_context = CertDuplicateCertificateContext(domain->cred->cert_context);
  pn_decref(domain->cred);
  domain->cred = new_cred;

  domain->has_ca_db = (certificate_db != NULL);
  return 0;
}


int pn_ssl_domain_set_peer_authentication(pn_ssl_domain_t *domain,
                                          const pn_ssl_verify_mode_t mode,
                                          const char *trusted_CAs)
{
  if (!domain) return -1;

  switch (mode) {
  case PN_SSL_VERIFY_PEER:
    ssl_log_error("Error: optional peer name checking not yet implemented\n");  // But coming soon
    return -1;

  case PN_SSL_VERIFY_PEER_NAME:
    if (!domain->has_ca_db) {
      ssl_log_error("Error: cannot verify peer without a trusted CA configured.\n"
                    "       Use pn_ssl_domain_set_trusted_ca_db()\n");
      return -1;
    }
    if (domain->mode == PN_SSL_MODE_SERVER) {
      ssl_log_error("Client certificates not yet supported.\n"); // But coming soon
      return -1;
    }
    break;

  case PN_SSL_ANONYMOUS_PEER:   // hippie free love mode... :)
    break;

  default:
    ssl_log_error("Invalid peer authentication mode given.\n");
    return -1;
  }

  domain->verify_mode = mode;
  win_credential_t *new_cred = win_credential(domain->mode);
  if (!new_cred)
    return -1;
  new_cred->cert_context = CertDuplicateCertificateContext(domain->cred->cert_context);
  pn_decref(domain->cred);
  domain->cred = new_cred;

  return 0;
}

int pn_ssl_init(pn_ssl_t *ssl, pn_ssl_domain_t *domain, const char *session_id)
{
  if (!ssl || !domain || ssl->domain) return -1;
  if (ssl->state != CREATED) return -1;

  ssl->domain = domain;
  domain->ref_count++;
  if (domain->allow_unsecured) {
    ssl->io_layer->process_input = process_input_unknown;
    ssl->io_layer->process_output = process_output_unknown;
  } else {
    ssl->io_layer->process_input = process_input_ssl;
    ssl->io_layer->process_output = process_output_ssl;
  }

  if (session_id && domain->mode == PN_SSL_MODE_CLIENT)
    ssl->session_id = pn_strdup(session_id);

  ssl->cred = domain->cred;
  pn_incref(domain->cred);

  SECURITY_STATUS status = SEC_E_OK;
  ssl->cred_handle = win_credential_cred_handle(ssl->cred, ssl->domain->verify_mode,
                                                ssl->session_id, &status);
  if (status != SEC_E_OK) {
    ssl_log_error_status(status, "Credentials handle failure");
    return -1;
  }

  ssl->state = (domain->mode == PN_SSL_MODE_CLIENT) ? CLIENT_HELLO : NEGOTIATING;
  return 0;
}


int pn_ssl_domain_allow_unsecured_client(pn_ssl_domain_t *domain)
{
  if (!domain) return -1;
  if (domain->mode != PN_SSL_MODE_SERVER) {
    ssl_log_error("Cannot permit unsecured clients - not a server.\n");
    return -1;
  }
  domain->allow_unsecured = true;
  return 0;
}


bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *buffer, size_t size )
{
  *buffer = '\0';
  snprintf( buffer, size, "%s", "TODO: cipher_name" );
  return true;
}

bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *buffer, size_t size )
{
  *buffer = '\0';
  snprintf( buffer, size, "%s", "TODO: protocol name" );
  return true;
}


void pn_ssl_free( pn_ssl_t *ssl)
{
  if (!ssl) return;
  ssl_log( ssl, "SSL socket freed.\n" );
  // clean up Windows per TLS session data before releasing the domain count
  if (SecIsValidHandle(&ssl->ctxt_handle))
    DeleteSecurityContext(&ssl->ctxt_handle);
  if (ssl->cred) {
    if (ssl->domain->mode == PN_SSL_MODE_CLIENT && ssl->session_id == NULL) {
      // Responsible for unshared handle
      if (SecIsValidHandle(&ssl->cred_handle))
        FreeCredentialsHandle(&ssl->cred_handle);
    }
    pn_decref(ssl->cred);
  }

  if (ssl->domain) pn_ssl_domain_free(ssl->domain);
  if (ssl->session_id) free((void *)ssl->session_id);
  if (ssl->peer_hostname) free((void *)ssl->peer_hostname);
  if (ssl->sc_inbuf) free((void *)ssl->sc_inbuf);
  if (ssl->sc_outbuf) free((void *)ssl->sc_outbuf);
  if (ssl->inbuf2) pn_buffer_free(ssl->inbuf2);

  free(ssl);
}

pn_ssl_t *pn_ssl(pn_transport_t *transport)
{
  if (!transport) return NULL;
  if (transport->ssl) return transport->ssl;

  pn_ssl_t *ssl = (pn_ssl_t *) calloc(1, sizeof(pn_ssl_t));
  if (!ssl) return NULL;
  ssl->sc_out_size = ssl->sc_in_size = SSL_BUF_SIZE;

  ssl->sc_outbuf = (char *)malloc(ssl->sc_out_size);
  if (!ssl->sc_outbuf) {
    free(ssl);
    return NULL;
  }
  ssl->sc_inbuf = (char *)malloc(ssl->sc_in_size);
  if (!ssl->sc_inbuf) {
    free(ssl->sc_outbuf);
    free(ssl);
    return NULL;
  }

  ssl->inbuf2 = pn_buffer(0);
  if (!ssl->inbuf2) {
    free(ssl->sc_inbuf);
    free(ssl->sc_outbuf);
    free(ssl);
    return NULL;
  }

  ssl->transport = transport;
  transport->ssl = ssl;

  ssl->io_layer = &transport->io_layers[PN_IO_SSL];
  ssl->io_layer->context = ssl;
  ssl->io_layer->process_input = pn_io_layer_input_passthru;
  ssl->io_layer->process_output = pn_io_layer_output_passthru;
  ssl->io_layer->process_tick = pn_io_layer_tick_passthru;
  ssl->io_layer->buffered_output = buffered_output;
  ssl->io_layer->buffered_input = buffered_input;

  ssl->trace = (transport->disp) ? transport->disp->trace : PN_TRACE_OFF;
  SecInvalidateHandle(&ssl->cred_handle);
  SecInvalidateHandle(&ssl->ctxt_handle);
  ssl->state = CREATED;
  ssl->decrypting = true;

  return ssl;
}

void pn_ssl_trace(pn_ssl_t *ssl, pn_trace_t trace)
{
  ssl->trace = trace;
}


pn_ssl_resume_status_t pn_ssl_resume_status( pn_ssl_t *ssl )
{
  // TODO
  return PN_SSL_RESUME_UNKNOWN;
}


int pn_ssl_set_peer_hostname( pn_ssl_t *ssl, const char *hostname )
{
  if (!ssl) return -1;

  if (ssl->peer_hostname) free((void *)ssl->peer_hostname);
  ssl->peer_hostname = NULL;
  if (hostname) {
    ssl->peer_hostname = pn_strdup(hostname);
    if (!ssl->peer_hostname) return -2;
  }
  return 0;
}

int pn_ssl_get_peer_hostname( pn_ssl_t *ssl, char *hostname, size_t *bufsize )
{
  if (!ssl) return -1;
  if (!ssl->peer_hostname) {
    *bufsize = 0;
    if (hostname) *hostname = '\0';
    return 0;
  }
  unsigned len = strlen(ssl->peer_hostname);
  if (hostname) {
    if (len >= *bufsize) return -1;
    strcpy( hostname, ssl->peer_hostname );
  }
  *bufsize = len;
  return 0;
}


/** SChannel specific: */

const char *tls_version_check(pn_ssl_t *ssl)
{
  SecPkgContext_ConnectionInfo info;
  QueryContextAttributes(&ssl->ctxt_handle, SECPKG_ATTR_CONNECTION_INFO, &info);
  // Ascending bit patterns denote newer SSL/TLS protocol versions.
  // SP_PROT_TLS1_0_SERVER is not defined until VS2010.
  return (info.dwProtocol < SP_PROT_TLS1_SERVER) ?
    "peer does not support TLS 1.0 security" : NULL;
}

static void ssl_encrypt(pn_ssl_t *ssl, char *app_data, size_t count)
{
  // Get SChannel to encrypt exactly one Record.
  SecBuffer buffs[4];
  buffs[0].cbBuffer = ssl->sc_sizes.cbHeader;
  buffs[0].BufferType = SECBUFFER_STREAM_HEADER;
  buffs[0].pvBuffer = ssl->sc_outbuf;
  buffs[1].cbBuffer = count;
  buffs[1].BufferType = SECBUFFER_DATA;
  buffs[1].pvBuffer = app_data;
  buffs[2].cbBuffer = ssl->sc_sizes.cbTrailer;
  buffs[2].BufferType = SECBUFFER_STREAM_TRAILER;
  buffs[2].pvBuffer = &app_data[count];
  buffs[3].cbBuffer = 0;
  buffs[3].BufferType = SECBUFFER_EMPTY;
  buffs[3].pvBuffer = 0;
  SecBufferDesc buff_desc;
  buff_desc.ulVersion = SECBUFFER_VERSION;
  buff_desc.cBuffers = 4;
  buff_desc.pBuffers = buffs;
  SECURITY_STATUS status = EncryptMessage(&ssl->ctxt_handle, 0, &buff_desc, 0);
  assert(status == SEC_E_OK);

  // EncryptMessage encrypts the data in place. The header and trailer
  // areas were reserved previously and must now be included in the updated
  // count of bytes to write to the peer.
  ssl->sc_out_count = buffs[0].cbBuffer + buffs[1].cbBuffer + buffs[2].cbBuffer;
  ssl->network_outp = ssl->sc_outbuf;
  ssl->network_out_pending = ssl->sc_out_count;
  ssl_log(ssl, "ssl_encrypt %d network bytes\n", ssl->network_out_pending);
}

// Returns true if decryption succeeded (even for empty content)
static bool ssl_decrypt(pn_ssl_t *ssl)
{
  // Get SChannel to decrypt input.  May have an incomplete Record,
  // exactly one, or more than one.  Check also for session ending,
  // session renegotiation.

  SecBuffer recv_buffs[4];
  recv_buffs[0].cbBuffer = ssl->sc_in_count;
  recv_buffs[0].BufferType = SECBUFFER_DATA;
  recv_buffs[0].pvBuffer = ssl->sc_inbuf;
  recv_buffs[1].BufferType = SECBUFFER_EMPTY;
  recv_buffs[2].BufferType = SECBUFFER_EMPTY;
  recv_buffs[3].BufferType = SECBUFFER_EMPTY;
  SecBufferDesc buff_desc;
  buff_desc.ulVersion = SECBUFFER_VERSION;
  buff_desc.cBuffers = 4;
  buff_desc.pBuffers = recv_buffs;
  SECURITY_STATUS status = DecryptMessage(&ssl->ctxt_handle, &buff_desc, 0, NULL);

  if (status == SEC_E_INCOMPLETE_MESSAGE) {
    // Less than a full Record, come back later with more network data
    ssl->sc_in_incomplete = true;
    return false;
  }

  ssl->decrypting = false;

  if (status != SEC_E_OK) {
    rewind_sc_inbuf(ssl);
    switch (status) {
    case SEC_I_CONTEXT_EXPIRED:
      // TLS shutdown alert record.  Ignore all subsequent input.
      ssl->state = SHUTTING_DOWN;
      ssl->sc_input_shutdown = true;
      return false;

    case SEC_I_RENEGOTIATE:
      // TODO.  Fall through for now.
    default:
      ssl_failed(ssl, 0);
      return false;
    }
  }

  ssl->decrypting = false;
  // have a decrypted Record and possible (still-encrypted) data of
  // one (or more) later Recordss.  Adjust pointers accordingly.
  for (int i = 0; i < 4; i++) {
    switch (recv_buffs[i].BufferType) {
    case SECBUFFER_DATA:
      ssl->in_data = (char *) recv_buffs[i].pvBuffer;
      ssl->in_data_size = ssl->in_data_count = recv_buffs[i].cbBuffer;
      break;
    case SECBUFFER_EXTRA:
      ssl->inbuf_extra = (char *)recv_buffs[i].pvBuffer;
      ssl->extra_count = recv_buffs[i].cbBuffer;
      break;
    default:
      // SECBUFFER_STREAM_HEADER:
      // SECBUFFER_STREAM_TRAILER:
      break;
    }
  }
  return true;
}

static void client_handshake_init(pn_ssl_t *ssl)
{
  // Tell SChannel to create the first handshake token (ClientHello)
  // and place it in sc_outbuf
  SEC_CHAR *host = const_cast<SEC_CHAR *>(ssl->peer_hostname);
  ULONG ctxt_requested = ISC_REQ_STREAM | ISC_REQ_USE_SUPPLIED_CREDS;
  ULONG ctxt_attrs;

  SecBuffer send_buffs[2];
  send_buffs[0].cbBuffer = ssl->sc_out_size;
  send_buffs[0].BufferType = SECBUFFER_TOKEN;
  send_buffs[0].pvBuffer = ssl->sc_outbuf;
  send_buffs[1].cbBuffer = 0;
  send_buffs[1].BufferType = SECBUFFER_EMPTY;
  send_buffs[1].pvBuffer = 0;
  SecBufferDesc send_buff_desc;
  send_buff_desc.ulVersion = SECBUFFER_VERSION;
  send_buff_desc.cBuffers = 2;
  send_buff_desc.pBuffers = send_buffs;
  SECURITY_STATUS status = InitializeSecurityContext(&ssl->cred_handle,
                               NULL, host, ctxt_requested, 0, 0, NULL, 0,
                               &ssl->ctxt_handle, &send_buff_desc,
                               &ctxt_attrs, NULL);

  if (status == SEC_I_CONTINUE_NEEDED) {
    ssl->sc_out_count = send_buffs[0].cbBuffer;
    ssl->network_out_pending = ssl->sc_out_count;
    // the token is the whole quantity to send
    ssl->network_outp = ssl->sc_outbuf;
    ssl_log(ssl, "Sending client hello %d bytes\n", ssl->network_out_pending);
  } else {
    ssl_log_error_status(status, "InitializeSecurityContext failed");
    ssl_failed(ssl, 0);
  }
}

static void client_handshake( pn_ssl_t* ssl) {
  // Feed SChannel ongoing responses from the server until the handshake is complete.
  SEC_CHAR *host = const_cast<SEC_CHAR *>(ssl->peer_hostname);
  ULONG ctxt_requested = ISC_REQ_STREAM | ISC_REQ_USE_SUPPLIED_CREDS;
  ULONG ctxt_attrs;
  size_t max = 0;

  // token_buffs describe the buffer that's coming in. It should have
  // a token from the SSL server, or empty if sending final shutdown alert.
  bool shutdown = ssl->state == SHUTTING_DOWN;
  SecBuffer token_buffs[2];
  token_buffs[0].cbBuffer = shutdown ? 0 : ssl->sc_in_count;
  token_buffs[0].BufferType = SECBUFFER_TOKEN;
  token_buffs[0].pvBuffer = shutdown ? 0 : ssl->sc_inbuf;
  token_buffs[1].cbBuffer = 0;
  token_buffs[1].BufferType = SECBUFFER_EMPTY;
  token_buffs[1].pvBuffer = 0;
  SecBufferDesc token_buff_desc;
  token_buff_desc.ulVersion = SECBUFFER_VERSION;
  token_buff_desc.cBuffers = 2;
  token_buff_desc.pBuffers = token_buffs;

  // send_buffs will hold information to forward to the peer.
  SecBuffer send_buffs[2];
  send_buffs[0].cbBuffer = ssl->sc_out_size;
  send_buffs[0].BufferType = SECBUFFER_TOKEN;
  send_buffs[0].pvBuffer = ssl->sc_outbuf;
  send_buffs[1].cbBuffer = 0;
  send_buffs[1].BufferType = SECBUFFER_EMPTY;
  send_buffs[1].pvBuffer = 0;
  SecBufferDesc send_buff_desc;
  send_buff_desc.ulVersion = SECBUFFER_VERSION;
  send_buff_desc.cBuffers = 2;
  send_buff_desc.pBuffers = send_buffs;

  SECURITY_STATUS status = InitializeSecurityContext(&ssl->cred_handle,
                               &ssl->ctxt_handle, host, ctxt_requested, 0, 0,
                               &token_buff_desc, 0, NULL, &send_buff_desc,
                               &ctxt_attrs, NULL);
  switch (status) {
  case SEC_E_INCOMPLETE_MESSAGE:
    // Not enough - get more data from the server then try again.
    // Leave input buffers untouched.
    ssl_log(ssl, "client handshake: incomplete record\n");
    ssl->sc_in_incomplete = true;
    return;

  case SEC_I_CONTINUE_NEEDED:
    // Successful handshake step, requiring data to be sent to peer.
    // TODO: check if server has requested a client certificate
    ssl->sc_out_count = send_buffs[0].cbBuffer;
    // the token is the whole quantity to send
    ssl->network_out_pending = ssl->sc_out_count;
    ssl->network_outp = ssl->sc_outbuf;
    ssl_log(ssl, "client handshake token %d bytes\n", ssl->network_out_pending);
    break;

  case SEC_E_OK:
    // Handshake complete.
    if (shutdown) {
      if (send_buffs[0].cbBuffer > 0) {
        ssl->sc_out_count = send_buffs[0].cbBuffer;
        // the token is the whole quantity to send
        ssl->network_out_pending = ssl->sc_out_count;
        ssl->network_outp = ssl->sc_outbuf;
        ssl_log(ssl, "client shutdown token %d bytes\n", ssl->network_out_pending);
      } else {
        ssl->state = SSL_CLOSED;
      }
      // we didn't touch sc_inbuf, no need to reset
      return;
    }
    if (send_buffs[0].cbBuffer != 0) {
      ssl_failed(ssl, "unexpected final server token");
      break;
    }
    if (const char *err = tls_version_check(ssl)) {
      ssl_failed(ssl, err);
      break;
    }
    if (token_buffs[1].BufferType == SECBUFFER_EXTRA && token_buffs[1].cbBuffer > 0) {
      // This seems to work but not documented, plus logic differs from decrypt message
      // since the pvBuffer value is not set.  Grrr.
      ssl->extra_count = token_buffs[1].cbBuffer;
      ssl->inbuf_extra = ssl->sc_inbuf + (ssl->sc_in_count - ssl->extra_count);
    }

    QueryContextAttributes(&ssl->ctxt_handle,
                             SECPKG_ATTR_STREAM_SIZES, &ssl->sc_sizes);
    max = ssl->sc_sizes.cbMaximumMessage + ssl->sc_sizes.cbHeader + ssl->sc_sizes.cbTrailer;
    if (max > ssl->sc_out_size) {
      ssl_log_error("Buffer size mismatch have %d, need %d\n", (int) ssl->sc_out_size, (int) max);
      ssl->state = SHUTTING_DOWN;
      ssl->app_input_closed = ssl->app_output_closed = PN_ERR;
      start_ssl_shutdown(ssl);
      pn_do_error(ssl->transport, "amqp:connection:framing-error", "SSL Failure: buffer size");
      break;
    }

    ssl->state = RUNNING;
    ssl->max_data_size = max - ssl->sc_sizes.cbHeader - ssl->sc_sizes.cbTrailer;
    ssl_log(ssl, "client handshake successful %d max record size\n", max);
    break;

  case SEC_I_CONTEXT_EXPIRED:
    // ended before we got going
  default:
    ssl_log(ssl, "client handshake failed %d\n", (int) status);
    ssl_failed(ssl, 0);
    break;
  }
  ssl->decrypting = false;
  rewind_sc_inbuf(ssl);
}


static void server_handshake(pn_ssl_t* ssl)
{
  // Feed SChannel ongoing handshake records from the client until the handshake is complete.
  ULONG ctxt_requested = ASC_REQ_STREAM;
  // TODO:  if server and verifying client certificate, ctxtRequested |= ASC_REQ_MUTUAL_AUTH;
  ULONG ctxt_attrs;
  size_t max = 0;

  // token_buffs describe the buffer that's coming in. It should have
  // a token from the SSL client except if shutting down or renegotiating.
  bool shutdown = ssl->state == SHUTTING_DOWN;
  SecBuffer token_buffs[2];
  token_buffs[0].cbBuffer = shutdown ? 0 : ssl->sc_in_count;
  token_buffs[0].BufferType = SECBUFFER_TOKEN;
  token_buffs[0].pvBuffer = shutdown ? 0 : ssl->sc_inbuf;
  token_buffs[1].cbBuffer = 0;
  token_buffs[1].BufferType = SECBUFFER_EMPTY;
  token_buffs[1].pvBuffer = 0;
  SecBufferDesc token_buff_desc;
  token_buff_desc.ulVersion = SECBUFFER_VERSION;
  token_buff_desc.cBuffers = 2;
  token_buff_desc.pBuffers = token_buffs;

  // send_buffs will hold information to forward to the peer.
  SecBuffer send_buffs[2];
  send_buffs[0].cbBuffer = ssl->sc_out_size;
  send_buffs[0].BufferType = SECBUFFER_TOKEN;
  send_buffs[0].pvBuffer = ssl->sc_outbuf;
  send_buffs[1].cbBuffer = 0;
  send_buffs[1].BufferType = SECBUFFER_EMPTY;
  send_buffs[1].pvBuffer = 0;
  SecBufferDesc send_buff_desc;
  send_buff_desc.ulVersion = SECBUFFER_VERSION;
  send_buff_desc.cBuffers = 2;
  send_buff_desc.pBuffers = send_buffs;
  PCtxtHandle ctxt_handle_ptr = (SecIsValidHandle(&ssl->ctxt_handle)) ? &ssl->ctxt_handle : 0;

  SECURITY_STATUS status = AcceptSecurityContext(&ssl->cred_handle, ctxt_handle_ptr,
                               &token_buff_desc, ctxt_requested, 0, &ssl->ctxt_handle,
                               &send_buff_desc, &ctxt_attrs, NULL);

  bool outbound_token = false;
  switch(status) {
  case SEC_E_INCOMPLETE_MESSAGE:
    // Not enough - get more data from the client then try again.
    // Leave input buffers untouched.
    ssl_log(ssl, "server handshake: incomplete record\n");
    ssl->sc_in_incomplete = true;
    return;

  case SEC_I_CONTINUE_NEEDED:
    outbound_token = true;
    break;

  case SEC_E_OK:
    // Handshake complete.
    if (shutdown) {
      if (send_buffs[0].cbBuffer > 0) {
        ssl->sc_out_count = send_buffs[0].cbBuffer;
        // the token is the whole quantity to send
        ssl->network_out_pending = ssl->sc_out_count;
        ssl->network_outp = ssl->sc_outbuf;
        ssl_log(ssl, "server shutdown token %d bytes\n", ssl->network_out_pending);
      } else {
        ssl->state = SSL_CLOSED;
      }
      // we didn't touch sc_inbuf, no need to reset
      return;
    }
    if (const char *err = tls_version_check(ssl)) {
      ssl_failed(ssl, err);
      break;
    }
    // Handshake complete.
    // TODO: manual check of certificate chain here, if bad: ssl_failed() + break;
    QueryContextAttributes(&ssl->ctxt_handle,
                             SECPKG_ATTR_STREAM_SIZES, &ssl->sc_sizes);
    max = ssl->sc_sizes.cbMaximumMessage + ssl->sc_sizes.cbHeader + ssl->sc_sizes.cbTrailer;
    if (max > ssl->sc_out_size) {
      ssl_log_error("Buffer size mismatch have %d, need %d\n", (int) ssl->sc_out_size, (int) max);
      ssl->state = SHUTTING_DOWN;
      ssl->app_input_closed = ssl->app_output_closed = PN_ERR;
      start_ssl_shutdown(ssl);
      pn_do_error(ssl->transport, "amqp:connection:framing-error", "SSL Failure: buffer size");
      break;
    }

    if (send_buffs[0].cbBuffer != 0)
      outbound_token = true;

    ssl->state = RUNNING;
    ssl->max_data_size = max - ssl->sc_sizes.cbHeader - ssl->sc_sizes.cbTrailer;
    ssl_log(ssl, "server handshake successful %d max record size\n", max);
    break;

  case SEC_I_CONTEXT_EXPIRED:
    // ended before we got going
  default:
    ssl_log(ssl, "server handshake failed %d\n", (int) status);
    ssl_failed(ssl, 0);
    break;
  }

  if (outbound_token) {
    // Successful handshake step, requiring data to be sent to peer.
    assert(ssl->network_out_pending == 0);
    ssl->sc_out_count = send_buffs[0].cbBuffer;
    // the token is the whole quantity to send
    ssl->network_out_pending = ssl->sc_out_count;
    ssl->network_outp = ssl->sc_outbuf;
    ssl_log(ssl, "server handshake token %d bytes\n", ssl->network_out_pending);
  }

  if (token_buffs[1].BufferType == SECBUFFER_EXTRA && token_buffs[1].cbBuffer > 0 &&
      !ssl->ssl_closed) {
    // remaining data after the consumed TLS record(s)
    ssl->extra_count = token_buffs[1].cbBuffer;
    ssl->inbuf_extra = ssl->sc_inbuf + (ssl->sc_in_count - ssl->extra_count);
  }

  ssl->decrypting = false;
  rewind_sc_inbuf(ssl);
}

static void ssl_handshake(pn_ssl_t* ssl)
{
  if (ssl->domain->mode == PN_SSL_MODE_CLIENT)
    client_handshake(ssl);
  else {
    server_handshake(ssl);
  }
}

static bool grow_inbuf2(pn_ssl_t *ssl, size_t minimum_size) {
  size_t old_capacity = pn_buffer_capacity(ssl->inbuf2);
  size_t new_capacity = old_capacity ? old_capacity * 2 : 1024;

  while (new_capacity < minimum_size)
    new_capacity *= 2;

  uint32_t max_frame = pn_transport_get_max_frame(ssl->transport);
  if (max_frame != 0) {
    if (old_capacity >= max_frame) {
      //  already big enough
      ssl_log(ssl, "Application expecting %d bytes (> negotiated maximum frame)\n", new_capacity);
      ssl_failed(ssl, "TLS: transport maximimum frame size error");
      return false;
    }
  }

  size_t extra_bytes = new_capacity - pn_buffer_size(ssl->inbuf2);
  int err = pn_buffer_ensure(ssl->inbuf2, extra_bytes);
  if (err) {
    ssl_log(ssl, "TLS memory allocation failed for %d bytes\n", max_frame);
    ssl_failed(ssl, "TLS memory allocation failed");
    return false;
  }
  return true;
}


// Peer initiated a session end by sending us a shutdown alert (and we should politely
// reciprocate), or else we are initiating the session end (and will not bother to wait
// for the peer shutdown alert). Stop processing input immediately, and stop processing
// output once this is sent.

static void start_ssl_shutdown(pn_ssl_t *ssl)
{
  assert(ssl->network_out_pending == 0);
  if (ssl->queued_shutdown)
    return;
  ssl->queued_shutdown = true;
  ssl_log(ssl, "Shutting down SSL connection...\n");

  DWORD shutdown = SCHANNEL_SHUTDOWN;
  SecBuffer shutBuff;
  shutBuff.cbBuffer = sizeof(DWORD);
  shutBuff.BufferType = SECBUFFER_TOKEN;
  shutBuff.pvBuffer = &shutdown;
  SecBufferDesc desc;
  desc.ulVersion = SECBUFFER_VERSION;
  desc.cBuffers = 1;
  desc.pBuffers = &shutBuff;
  ApplyControlToken(&ssl->ctxt_handle, &desc);

  // Next handshake will generate the shudown alert token
  ssl_handshake(ssl);
}

static int setup_ssl_connection(pn_ssl_t *ssl)
{
  ssl_log( ssl, "SSL connection detected.\n");
  ssl->io_layer->process_input = process_input_ssl;
  ssl->io_layer->process_output = process_output_ssl;
  return 0;
}

static void rewind_sc_inbuf(pn_ssl_t *ssl)
{
  // Decrypted bytes have been drained or double buffered.  Prepare for the next SSL Record.
  assert(ssl->in_data_count == 0);
  if (ssl->decrypting)
    return;
  ssl->decrypting = true;
  if (ssl->inbuf_extra) {
    // A previous read picked up more than one Record.  Move it to the beginning.
    memmove(ssl->sc_inbuf, ssl->inbuf_extra, ssl->extra_count);
    ssl->sc_in_count = ssl->extra_count;
    ssl->inbuf_extra = 0;
    ssl->extra_count = 0;
  } else {
    ssl->sc_in_count = 0;
  }
}

static void app_inbytes_add(pn_ssl_t *ssl)
{
  if (!ssl->app_inbytes.start) {
    ssl->app_inbytes.start = ssl->in_data;
    ssl->app_inbytes.size = ssl->in_data_count;
    return;
  }

  if (ssl->double_buffered) {
    if (pn_buffer_available(ssl->inbuf2) == 0) {
      if (!grow_inbuf2(ssl, 1024))
        // could not add room
        return;
    }
    size_t count = _pni_min(ssl->in_data_count, pn_buffer_available(ssl->inbuf2));
    pn_buffer_append(ssl->inbuf2, ssl->in_data, count);
    ssl->in_data += count;
    ssl->in_data_count -= count;
    ssl->app_inbytes = pn_buffer_bytes(ssl->inbuf2);
  } else {
    assert(ssl->app_inbytes.size == 0);
    ssl->app_inbytes.start = ssl->in_data;
    ssl->app_inbytes.size = ssl->in_data_count;
  }
}


static void app_inbytes_progress(pn_ssl_t *ssl, size_t minimum)
{
  // Make more decrypted data available, if possible.  Otherwise, move
  // unread bytes to front of inbuf2 to make room for next bulk decryption.
  // SSL may have chopped up data that app layer expects to be
  // contiguous.  Start, continue or stop double buffering here.
  if (ssl->double_buffered) {
    if (ssl->app_inbytes.size == 0) {
      // no straggler bytes, optimistically stop for now
      ssl->double_buffered = false;
      pn_buffer_clear(ssl->inbuf2);
      ssl->app_inbytes.start = ssl->in_data;
      ssl->app_inbytes.size = ssl->in_data_count;
    } else {
      pn_bytes_t ib2 = pn_buffer_bytes(ssl->inbuf2);
      assert(ssl->app_inbytes.size <= ib2.size);
      size_t consumed = ib2.size - ssl->app_inbytes.size;
      if (consumed > 0) {
        memmove((void *)ib2.start, ib2.start + consumed, ssl->app_inbytes.size);
        pn_buffer_trim(ssl->inbuf2, 0, consumed);
      }
      if (!pn_buffer_available(ssl->inbuf2)) {
        if (!grow_inbuf2(ssl, minimum))
          // could not add room
          return;
      }
      size_t count = _pni_min(ssl->in_data_count, pn_buffer_available(ssl->inbuf2));
      pn_buffer_append(ssl->inbuf2, ssl->in_data, count);
      ssl->in_data += count;
      ssl->in_data_count -= count;
      ssl->app_inbytes = pn_buffer_bytes(ssl->inbuf2);
    }
  } else {
    if (ssl->app_inbytes.size) {
      // start double buffering the left over bytes
      ssl->double_buffered = true;
      pn_buffer_clear(ssl->inbuf2);
      if (!pn_buffer_available(ssl->inbuf2)) {
        if (!grow_inbuf2(ssl, minimum))
          // could not add room
          return;
      }
      size_t count = _pni_min(ssl->in_data_count, pn_buffer_available(ssl->inbuf2));
      pn_buffer_append(ssl->inbuf2, ssl->in_data, count);
      ssl->in_data += count;
      ssl->in_data_count -= count;
      ssl->app_inbytes = pn_buffer_bytes(ssl->inbuf2);
    } else {
      // already pointing at all available bytes until next decrypt
    }
  }
  if (ssl->in_data_count == 0)
    rewind_sc_inbuf(ssl);
}


static void app_inbytes_advance(pn_ssl_t *ssl, size_t consumed)
{
  if (consumed == 0) {
    // more contiguous bytes required
    app_inbytes_progress(ssl, ssl->app_inbytes.size + 1);
    return;
  }
  assert(consumed <= ssl->app_inbytes.size);
  ssl->app_inbytes.start += consumed;
  ssl->app_inbytes.size -= consumed;
  if (!ssl->double_buffered) {
    ssl->in_data += consumed;
    ssl->in_data_count -= consumed;
  }
  if (ssl->app_inbytes.size == 0)
    app_inbytes_progress(ssl, 0);
}

static void read_closed(pn_ssl_t *ssl, ssize_t error)
{
  if (ssl->app_input_closed)
    return;
  if (ssl->state == RUNNING && !error) {
    pn_io_layer_t *io_next = ssl->io_layer->next;
    // Signal end of stream
    ssl->app_input_closed = io_next->process_input(io_next, ssl->app_inbytes.start, 0);
  }
  if (!ssl->app_input_closed)
    ssl->app_input_closed = error ? error : PN_ERR;

  if (ssl->app_output_closed) {
    // both sides of app closed, and no more app output pending:
    ssl->state = SHUTTING_DOWN;
    if (ssl->network_out_pending == 0 && !ssl->queued_shutdown) {
      start_ssl_shutdown(ssl);
    }
  }
}


// Read up to "available" bytes from the network, decrypt it and pass plaintext to application.

static ssize_t process_input_ssl(pn_io_layer_t *io_layer, const char *input_data, size_t available)
{
  pn_ssl_t *ssl = (pn_ssl_t *)io_layer->context;
  ssl_log( ssl, "process_input_ssl( data size=%d )\n",available );
  ssize_t consumed = 0;
  ssize_t forwarded = 0;
  bool new_app_input;

  if (available == 0) {
    // No more inbound network data
    read_closed(ssl,0);
    return 0;
  }

  do {
    if (ssl->sc_input_shutdown) {
      // TLS protocol shutdown detected on input
      read_closed(ssl,0);
      return consumed;
    }

    // sc_inbuf should be ready for new or additional network encrypted bytes.
    // i.e. no straggling decrypted bytes pending.
    assert(ssl->in_data_count == 0 && ssl->decrypting);
    new_app_input = false;
    size_t count;

    if (ssl->state != RUNNING) {
      count = _pni_min(ssl->sc_in_size - ssl->sc_in_count, available);
    } else {
      // look for TLS record boundaries
      if (ssl->sc_in_count < 5) {
        ssl->sc_in_incomplete = true;
        size_t hc = _pni_min(available, 5 - ssl->sc_in_count);
        memmove(ssl->sc_inbuf + ssl->sc_in_count, input_data, hc);
        ssl->sc_in_count += hc;
        input_data += hc;
        available -= hc;
        consumed += hc;
        if (ssl->sc_in_count < 5 || available == 0)
          break;
      }

      // Top up sc_inbuf from network input_data hoping for a complete TLS Record
      // We try to guess the length as an optimization, but let SChannel
      // ultimately decide if there is spoofing going on.
      unsigned char low = (unsigned char) ssl->sc_inbuf[4];
      unsigned char high = (unsigned char) ssl->sc_inbuf[3];
      size_t rec_len = high * 256 + low + 5;
      if (rec_len < 5 || rec_len == ssl->sc_in_count || rec_len > ssl->sc_in_size)
        rec_len = ssl->sc_in_size;

      count = _pni_min(rec_len - ssl->sc_in_count, available);
    }

    if (count > 0) {
      memmove(ssl->sc_inbuf + ssl->sc_in_count, input_data, count);
      ssl->sc_in_count += count;
      input_data += count;
      available -= count;
      consumed += count;
      ssl->sc_in_incomplete = false;
    }

    // Try to decrypt another TLS Record.

    if (ssl->sc_in_count > 0 && ssl->state <= SHUTTING_DOWN) {
      if (ssl->state == NEGOTIATING) {
        ssl_handshake(ssl);
      } else {
        if (ssl_decrypt(ssl)) {
          // Ignore TLS Record with 0 length data (does not mean EOS)
          if (ssl->in_data_size > 0) {
            new_app_input = true;
            app_inbytes_add(ssl);
          } else {
            assert(ssl->decrypting == false);
            rewind_sc_inbuf(ssl);
          }
        }
        ssl_log(ssl, "Next decryption, %d left over\n", available);
      }
    }

    if (ssl->state == SHUTTING_DOWN) {
      if (ssl->network_out_pending == 0 && !ssl->queued_shutdown) {
        start_ssl_shutdown(ssl);
      }
    } else if (ssl->state == SSL_CLOSED) {
      return consumed ? consumed : -1;
    }

    // Consume or discard the decrypted bytes
    if (new_app_input && (ssl->state == RUNNING || ssl->state == SHUTTING_DOWN)) {
      // present app_inbytes to io_next only if it has new content
      while (ssl->app_inbytes.size > 0) {
        if (!ssl->app_input_closed) {
          pn_io_layer_t *io_next = ssl->io_layer->next;
          ssize_t count = io_next->process_input(io_next, ssl->app_inbytes.start, ssl->app_inbytes.size);
          if (count > 0) {
            forwarded += count;
            // advance() can increase app_inbytes.size if double buffered
            app_inbytes_advance(ssl, count);
            ssl_log(ssl, "Application consumed %d bytes from peer\n", (int) count);
          } else if (count == 0) {
            size_t old_size = ssl->app_inbytes.size;
            app_inbytes_advance(ssl, 0);
            if (ssl->app_inbytes.size == old_size) {
              break;  // no additional contiguous decrypted data available, get more network data
            }
          } else {
            // count < 0
            ssl_log(ssl, "Application layer closed its input, error=%d (discarding %d bytes)\n",
                 (int) count, (int)ssl->app_inbytes.size);
            app_inbytes_advance(ssl, ssl->app_inbytes.size);    // discard
            read_closed(ssl, count);
          }
        } else {
          ssl_log(ssl, "Input closed discard %d bytes\n",
               (int)ssl->app_inbytes.size);
          app_inbytes_advance(ssl, ssl->app_inbytes.size);      // discard
        }
      }
    }
  } while (available || (ssl->sc_in_count && !ssl->sc_in_incomplete));

  if (ssl->app_input_closed && ssl->state >= SHUTTING_DOWN) {
    consumed = ssl->app_input_closed;
    ssl->io_layer->process_input = process_input_done;
  }
  ssl_log(ssl, "process_input_ssl() returning %d, forwarded %d\n", (int) consumed, (int) forwarded);
  return consumed;
}

static ssize_t process_output_ssl( pn_io_layer_t *io_layer, char *buffer, size_t max_len)
{
  pn_ssl_t *ssl = (pn_ssl_t *)io_layer->context;
  if (!ssl) return PN_EOS;
  ssl_log( ssl, "process_output_ssl( max_len=%d )\n",max_len );

  ssize_t written = 0;
  ssize_t total_app_bytes = 0;
  bool work_pending;

  if (ssl->state == CLIENT_HELLO) {
    // output buffers eclusively for internal handshake use until negotiation complete
    client_handshake_init(ssl);
    if (ssl->state == SSL_CLOSED)
      return PN_EOS;
    ssl->state = NEGOTIATING;
  }

  do {
    work_pending = false;

    if (ssl->network_out_pending > 0) {
      size_t wcount = _pni_min(ssl->network_out_pending, max_len);
      memmove(buffer, ssl->network_outp, wcount);
      ssl->network_outp += wcount;
      ssl->network_out_pending -= wcount;
      buffer += wcount;
      max_len -= wcount;
      written += wcount;
    }

    if (ssl->network_out_pending == 0 && ssl->state == RUNNING  && !ssl->app_output_closed) {
      // refill the buffer with app data and encrypt it

      char *app_data = ssl->sc_outbuf + ssl->sc_sizes.cbHeader;
      char *app_outp = app_data;
      size_t remaining = ssl->max_data_size;
      ssize_t app_bytes;
      do {
        pn_io_layer_t *io_next = ssl->io_layer->next;
        app_bytes = io_next->process_output(io_next, app_outp, remaining);
        if (app_bytes > 0) {
          app_outp += app_bytes;
          remaining -= app_bytes;
          ssl_log( ssl, "Gathered %d bytes from app to send to peer\n", app_bytes );
        } else {
          if (app_bytes < 0) {
            ssl_log(ssl, "Application layer closed its output, error=%d (%d bytes pending send)\n",
                 (int) app_bytes, (int) ssl->network_out_pending);
            ssl->app_output_closed = app_bytes;
            if (ssl->app_input_closed)
              ssl->state = SHUTTING_DOWN;
          } else if (total_app_bytes == 0 && ssl->app_input_closed) {
            // We've drained all the App layer can provide
            ssl_log(ssl, "Application layer blocked on input, closing\n");
            ssl->state = SHUTTING_DOWN;
            ssl->app_output_closed = PN_ERR;
          }
        }
      } while (app_bytes > 0);
      if (app_outp > app_data) {
        work_pending = (max_len > 0);
        ssl_encrypt(ssl, app_data, app_outp - app_data);
      }
    }

    if (ssl->network_out_pending == 0 && ssl->state == SHUTTING_DOWN) {
      if (!ssl->queued_shutdown) {
        start_ssl_shutdown(ssl);
        work_pending = true;
      } else {
        ssl->state = SSL_CLOSED;
      }
    }
  } while (work_pending);

  if (written == 0 && ssl->state == SSL_CLOSED) {
    written = ssl->app_output_closed ? ssl->app_output_closed : PN_EOS;
    ssl->io_layer->process_output = process_output_done;
  }
  ssl_log(ssl, "process_output_ssl() returning %d\n", (int) written);
  return written;
}


static int setup_cleartext_connection( pn_ssl_t *ssl )
{
  ssl_log( ssl, "Cleartext connection detected.\n");
  ssl->io_layer->process_input = pn_io_layer_input_passthru;
  ssl->io_layer->process_output = pn_io_layer_output_passthru;
  return 0;
}


// until we determine if the client is using SSL or not:

static ssize_t process_input_unknown(pn_io_layer_t *io_layer, const char *input_data, size_t len)
{
  pn_ssl_t *ssl = (pn_ssl_t *)io_layer->context;
  switch (check_for_ssl_connection( input_data, len )) {
  case SSL_CONNECTION:
    setup_ssl_connection( ssl );
    return ssl->io_layer->process_input( ssl->io_layer, input_data, len );
  case CLEAR_CONNECTION:
    setup_cleartext_connection( ssl );
    return ssl->io_layer->process_input( ssl->io_layer, input_data, len );
  default:
    return 0;
  }
}

static ssize_t process_output_unknown(pn_io_layer_t *io_layer, char *input_data, size_t len)
{
  // do not do output until we know if SSL is used or not
  return 0;
}

static connection_mode_t check_for_ssl_connection( const char *data, size_t len )
{
  if (len >= 5) {
    const unsigned char *buf = (unsigned char *)data;
    /*
     * SSLv2 Client Hello format
     * http://www.mozilla.org/projects/security/pki/nss/ssl/draft02.html
     *
     * Bytes 0-1: RECORD-LENGTH
     * Byte    2: MSG-CLIENT-HELLO (1)
     * Byte    3: CLIENT-VERSION-MSB
     * Byte    4: CLIENT-VERSION-LSB
     *
     * Allowed versions:
     * 2.0 - SSLv2
     * 3.0 - SSLv3
     * 3.1 - TLS 1.0
     * 3.2 - TLS 1.1
     * 3.3 - TLS 1.2
     *
     * The version sent in the Client-Hello is the latest version supported by
     * the client. NSS may send version 3.x in an SSLv2 header for
     * maximum compatibility.
     */
    int isSSL2Handshake = buf[2] == 1 &&   // MSG-CLIENT-HELLO
      ((buf[3] == 3 && buf[4] <= 3) ||    // SSL 3.0 & TLS 1.0-1.2 (v3.1-3.3)
       (buf[3] == 2 && buf[4] == 0));     // SSL 2

    /*
     * SSLv3/TLS Client Hello format
     * RFC 2246
     *
     * Byte    0: ContentType (handshake - 22)
     * Bytes 1-2: ProtocolVersion {major, minor}
     *
     * Allowed versions:
     * 3.0 - SSLv3
     * 3.1 - TLS 1.0
     * 3.2 - TLS 1.1
     * 3.3 - TLS 1.2
     */
    int isSSL3Handshake = buf[0] == 22 &&  // handshake
      (buf[1] == 3 && buf[2] <= 3);       // SSL 3.0 & TLS 1.0-1.2 (v3.1-3.3)

    if (isSSL2Handshake || isSSL3Handshake) {
      return SSL_CONNECTION;
    } else {
      return CLEAR_CONNECTION;
    }
  }
  return UNKNOWN_CONNECTION;
}

static ssize_t process_input_done(pn_io_layer_t *io_layer, const char *input_data, size_t len)
{
  return PN_EOS;
}

static ssize_t process_output_done(pn_io_layer_t *io_layer, char *input_data, size_t len)
{
  return PN_EOS;
}

// return # output bytes sitting in this layer
static size_t buffered_output(pn_io_layer_t *io_layer)
{
  size_t count = 0;
  pn_ssl_t *ssl = (pn_ssl_t *)io_layer->context;
  if (ssl) {
    count += ssl->network_out_pending;
    if (count == 0 && ssl->state == SHUTTING_DOWN && ssl->queued_shutdown)
      count++;
  }
  return count;
}

// return # input bytes sitting in this layer
static size_t buffered_input( pn_io_layer_t *io_layer )
{
  size_t count = 0;
  pn_ssl_t *ssl = (pn_ssl_t *)io_layer->context;
  if (ssl) {
    count += ssl->in_data_count;
  }
  return count;
}
