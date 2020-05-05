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

#include "core/autodetect.h"
#include "core/engine-internal.h"
#include "core/logger_private.h"
#include "core/util.h"

#include "platform/platform.h"

#include <proton/ssl.h>
#include <proton/engine.h>

#include <assert.h>
#include <stdio.h>

// security.h needs to see this to distinguish from kernel use.
#include <windows.h>
#define SECURITY_WIN32
#include <security.h>
#include <Schnlsp.h>
#include <WinInet.h>
#undef SECURITY_WIN32

extern "C" {

/** @file
 * SSL/TLS support API.
 *
 * This file contains an SChannel-based implemention of the SSL/TLS API for Windows platforms.
 */

static void ssl_log(pn_transport_t *transport, pn_log_level_t sev, const char *fmt, ...);
static void ssl_log_error(const char *fmt, ...);
static void ssl_log_error_status(HRESULT status, const char *fmt, ...);
static HCERTSTORE open_cert_db(const char *store_name, const char *passwd, int *error);

// Thread support.  Some SChannel objects are shared or ref-counted.
// Consistent with the rest of Proton, we assume a connection (and
// therefore its pn_ssl_t) will not be accessed concurrently by
// multiple threads.
class csguard {
  public:
    csguard(CRITICAL_SECTION *cs) : cs_(cs), set_(true) { EnterCriticalSection(cs_); }
    ~csguard() { if (set_) LeaveCriticalSection(cs_); }
    void release() {
        if (set_) {
            set_ = false;
            LeaveCriticalSection(cs_);
        }
    }
  private:
    LPCRITICAL_SECTION cs_;
    bool set_;
};

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
  CRITICAL_SECTION cslock;
  pn_ssl_mode_t mode;
  PCCERT_CONTEXT cert_context;  // Particulars of the certificate (if any)
  CredHandle cred_handle;       // Bound session parameters, certificate, CAs, verification_mode
  HCERTSTORE trust_store;       // Store of root CAs for validating
  HCERTSTORE server_CA_certs;   // CAs advertised by server (may be a duplicate of the trust_store)
  char *trust_store_name;
};

#define win_credential_compare NULL
#define win_credential_inspect NULL
#define win_credential_hashcode NULL

static void win_credential_initialize(void *object)
{
  win_credential_t *c = (win_credential_t *) object;
  InitializeCriticalSectionAndSpinCount(&c->cslock, 4000);
  SecInvalidateHandle(&c->cred_handle);
  c->cert_context = 0;
  c->trust_store = 0;
  c->server_CA_certs = 0;
  c->trust_store_name = 0;
}

static void win_credential_finalize(void *object)
{
  win_credential_t *c = (win_credential_t *) object;
  if (SecIsValidHandle(&c->cred_handle))
    FreeCredentialsHandle(&c->cred_handle);
  if (c->cert_context)
    CertFreeCertificateContext(c->cert_context);
  if (c->trust_store)
    CertCloseStore(c->trust_store, 0);
  if (c->server_CA_certs)
    CertCloseStore(c->server_CA_certs, 0);
  DeleteCriticalSection(&c->cslock);
  free(c->trust_store_name);
}

static win_credential_t *win_credential(pn_ssl_mode_t m)
{
  static const pn_cid_t CID_win_credential = CID_pn_void;
  static const pn_class_t clazz = PN_CLASS(win_credential);
  win_credential_t *c = (win_credential_t *) pn_class_new(&clazz, sizeof(win_credential_t));
  c->mode = m;
  csguard g(&c->cslock);
  pn_incref(c);  // See next comment regarding refcounting and locks
  return c;
}

// Hack strategy for Proton object refcounting.  Just hold the lock for incref.
// Use the next two functions for decref, one with, the other without the lock.
// The refcount is artificially bumped by one in win_credential() so that we
// can use refcount == 1 to actually delete (by calling decref one last time).
static bool win_credential_decref(win_credential_t *c)
{
  // Call with lock held.  Caller MUST call win_credential_delete if this returns true.
  return pn_decref(c) == 1;
}

static void win_credential_delete(win_credential_t *c)
{
  // Call without lock held.
  assert(pn_refcount(c) == 1);
  pn_decref(c);
}

static int win_credential_load_cert(win_credential_t *cred, const char *store_name, const char *cert_name, const char *passwd)
{
  if (!store_name)
    return -2;

  int ec = 0;
  HCERTSTORE cert_store = open_cert_db(store_name, passwd, &ec);
  if (!cert_store)
    return ec;

  // find friendly name that matches cert_name, or sole certificate
  PCCERT_CONTEXT tmpctx = NULL;
  PCCERT_CONTEXT found_ctx = NULL;
  int cert_count = 0;
  int name_len = cert_name ? strlen(cert_name) : 0;
  char *fn = name_len ? (char *) malloc(name_len + 1) : 0;
  while (tmpctx = CertEnumCertificatesInStore(cert_store, tmpctx)) {
    cert_count++;
    if (cert_name && *cert_name) {
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
        ssl_log_error("Multiple certificates to choose from certificate store %s", store_name);
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
    ssl_log_error("Could not find certificate %s in store %s", cert_name, store_name);
  cred->cert_context = found_ctx;

  free(fn);
  CertCloseStore(cert_store, 0);
  return found_ctx ? 0 : -8;
}


// call with win_credential lock held
static CredHandle win_credential_cred_handle(win_credential_t *cred, const char *session_id, SECURITY_STATUS *status)
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
  descriptor.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_MANUAL_CRED_VALIDATION;
  if (cred->cert_context != NULL) {
    // assign the certificate into the credentials
    descriptor.paCred = &cred->cert_context;
    descriptor.cCreds = 1;
  }

  if (cred->mode == PN_SSL_MODE_SERVER) {
    descriptor.dwFlags |= SCH_CRED_NO_SYSTEM_MAPPER;
    if (cred->server_CA_certs) {
      descriptor.hRootStore = cred->server_CA_certs;
    }
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
  CRITICAL_SECTION cslock;
  int ref_count;
  pn_ssl_mode_t mode;
  pn_ssl_verify_mode_t verify_mode;
  bool allow_unsecured;
  win_credential_t *cred;
};

typedef enum { CREATED, CLIENT_HELLO, NEGOTIATING,
               RUNNING, SHUTTING_DOWN, SSL_CLOSED } ssl_state_t;

struct pni_ssl_t {
  const char    *session_id;
  const char *peer_hostname;
  ssl_state_t state;

  bool protocol_detected;
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

  CredHandle cred_handle;
  CtxtHandle ctxt_handle;
  SecPkgContext_StreamSizes sc_sizes;
  pn_ssl_mode_t mode;
  pn_ssl_verify_mode_t verify_mode;
  win_credential_t *cred;
  char *subject;
};

static inline pn_transport_t *get_transport_internal(pn_ssl_t *ssl)
{
  // The external pn_sasl_t is really a pointer to the internal pni_transport_t
  return ((pn_transport_t *)ssl);
}

static inline pni_ssl_t *get_ssl_internal(pn_ssl_t *ssl)
{
  // The external pn_sasl_t is really a pointer to the internal pni_transport_t
  return ssl ? ((pn_transport_t *)ssl)->ssl : NULL;
}

struct pn_ssl_session_t {
  const char       *id;
// TODO
  pn_ssl_session_t *ssn_cache_next;
  pn_ssl_session_t *ssn_cache_prev;
};


static ssize_t process_input_ssl( pn_transport_t *transport, unsigned int layer, const char *input_data, size_t len);
static ssize_t process_output_ssl( pn_transport_t *transport, unsigned int layer, char *input_data, size_t len);
static ssize_t process_input_done(pn_transport_t *transport, unsigned int layer, const char *input_data, size_t len);
static ssize_t process_output_done(pn_transport_t *transport, unsigned int layer, char *input_data, size_t len);
static pn_ssl_session_t *ssn_cache_find( pn_ssl_domain_t *, const char * );
static void ssl_session_free( pn_ssl_session_t *);
static size_t buffered_output( pn_transport_t *transport );
static void start_ssl_shutdown(pn_transport_t *transport);
static void rewind_sc_inbuf(pni_ssl_t *ssl);
static bool grow_inbuf2(pn_transport_t *ssl, size_t minimum_size);
static HRESULT verify_peer(pni_ssl_t *ssl, HCERTSTORE root_store, const char *server_name, bool tracing);

// @todo: used to avoid littering the code with calls to printf...
static void ssl_vlog(pn_transport_t *transport, pn_log_level_t sev, const char *fmt, va_list ap)
{
  pn_logger_t *logger =  transport ? &transport->logger : pn_default_logger();
  if (PN_SHOULD_LOG(logger, PN_SUBSYSTEM_SSL, sev)) {
    pni_logger_vlogf(logger, PN_SUBSYSTEM_SSL, sev, fmt, ap);
  }
}

static void ssl_log(pn_transport_t *transport, pn_log_level_t sev, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  ssl_vlog(transport, sev, fmt, ap);
  va_end(ap);
}

// @todo: used to avoid littering the code with calls to printf...
static void ssl_log_error(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  ssl_vlog(NULL, PN_LEVEL_ERROR, fmt, ap);
  va_end(ap);
}

static void ssl_log_error_status(HRESULT status, const char *fmt, ...)
{
  char buf[512];
  va_list ap;

  if (fmt) {
    va_start(ap, fmt);
    ssl_vlog(NULL, PN_LEVEL_ERROR, fmt, ap);
    va_end(ap);
  }

  if (FormatMessage(FORMAT_MESSAGE_MAX_WIDTH_MASK | FORMAT_MESSAGE_FROM_SYSTEM,
                    0, status, 0, buf, sizeof(buf), 0))
    ssl_log_error("%s", buf);
  else
    ssl_log_error("Internal Windows error: %x for %x", GetLastError(), status);
}

static void ssl_log_clear_data(pn_transport_t *transport, const char *data, size_t len)
{
  PN_LOG_DATA(&transport->logger, PN_SUBSYSTEM_SSL, PN_LEVEL_RAW, "decrypted data", data, len );
}

static size_t _pni_min(size_t a, size_t b)
{
  return (a < b) ? a : b;
}

// unrecoverable SSL failure occured, notify transport and generate error code.
static int ssl_failed(pn_transport_t *transport, const char *reason)
{
  char buf[512] = "Unknown error.";
  if (!reason) {
    HRESULT status = GetLastError();

    FormatMessage(FORMAT_MESSAGE_MAX_WIDTH_MASK | FORMAT_MESSAGE_FROM_SYSTEM,
                  0, status, 0, buf, sizeof(buf), 0);
    reason = buf;
  }
  pni_ssl_t *ssl = transport->ssl;
  ssl->ssl_closed = true;
  ssl->app_input_closed = ssl->app_output_closed = PN_EOS;
  ssl->state = SSL_CLOSED;
  pn_do_error(transport, "amqp:connection:framing-error", "SSL Failure: %s", reason);
  return PN_EOS;
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

bool pn_ssl_present(void)
{
  return true;
}

pn_ssl_domain_t *pn_ssl_domain( pn_ssl_mode_t mode )
{
  switch (mode) {
  case PN_SSL_MODE_CLIENT:
  case PN_SSL_MODE_SERVER:
    break;

  default:
    ssl_log_error("Invalid mode for pn_ssl_mode_t: %d", mode);
    return NULL;
  }

  pn_ssl_domain_t *domain = (pn_ssl_domain_t *) calloc(1, sizeof(pn_ssl_domain_t));
  if (!domain) return NULL;

  InitializeCriticalSectionAndSpinCount(&domain->cslock, 4000);
  {
  csguard(&domain->cslock);
  domain->ref_count = 1;
  domain->mode = mode;
  domain->cred = win_credential(mode);
  }

  if (pn_ssl_domain_set_trusted_ca_db(domain, "ss:root") != 0) {
    pn_ssl_domain_free(domain);
    return NULL;
  }
  return domain;
}

// call with no locks
void pn_ssl_domain_free( pn_ssl_domain_t *domain )
{
  if (!domain) return;
  {
    csguard g(&domain->cslock);
    if (--domain->ref_count)
      return;
  }
  {
    csguard g2(&domain->cred->cslock);
    if (win_credential_decref(domain->cred)) {
      g2.release();
      win_credential_delete(domain->cred);
    }
  }
  DeleteCriticalSection(&domain->cslock);
  free(domain);
}


int pn_ssl_domain_set_credentials( pn_ssl_domain_t *domain,
                               const char *certificate_file,
                               const char *private_key_file,
                               const char *password)
{
  if (!domain) return -1;
  csguard g(&domain->cslock);
  csguard g2(&domain->cred->cslock);

  if (win_credential_has_certificate(domain->cred)) {
    // Need a new win_credential_t to hold new certificate
    if (win_credential_decref(domain->cred)) {
      g2.release();
      win_credential_delete(domain->cred);
    }
    domain->cred = win_credential(domain->mode);
    if (!domain->cred)
      return -1;
  }
  return win_credential_load_cert(domain->cred, certificate_file, private_key_file, password);
}


int pn_ssl_domain_set_trusted_ca_db(pn_ssl_domain_t *domain,
                                    const char *certificate_db)
{
  if (!domain || !certificate_db) return -1;
  csguard g(&domain->cslock);

  int ec = 0;
  HCERTSTORE store = open_cert_db(certificate_db, NULL, &ec);
  if (!store)
    return ec;

  csguard g2(&domain->cred->cslock);
  if (domain->cred->trust_store) {
    win_credential_t *new_cred = win_credential(domain->mode);
    if (!new_cred) {
      CertCloseStore(store, 0);
      return -1;
    }
    new_cred->cert_context = CertDuplicateCertificateContext(domain->cred->cert_context);
    if (win_credential_decref(domain->cred)) {
      g2.release();
      win_credential_delete(domain->cred);
    }
    domain->cred = new_cred;
  }

  domain->cred->trust_store = store;
  domain->cred->trust_store_name = pn_strdup(certificate_db);
  return 0;
}


int pn_ssl_domain_set_peer_authentication(pn_ssl_domain_t *domain,
                                          const pn_ssl_verify_mode_t mode,
                                          const char *trusted_CAs)
{
  if (!domain) return -1;
  csguard g(&domain->cslock);
  csguard g2(&domain->cred->cslock);

  HCERTSTORE store = 0;
  bool changed = domain->verify_mode && mode != domain->verify_mode;

  switch (mode) {
  case PN_SSL_VERIFY_PEER:
  case PN_SSL_VERIFY_PEER_NAME:
    if (domain->mode == PN_SSL_MODE_SERVER) {
      if (!trusted_CAs) {
        ssl_log_error("Error: a list of trusted CAs must be provided.");
        return -1;
      }
      if (!win_credential_has_certificate(domain->cred)) {
        ssl_log_error("Error: Server cannot verify peer without configuring a certificate, use pn_ssl_domain_set_credentials()");
        return -1;
      }
      int ec = 0;
      if (!strcmp(trusted_CAs, domain->cred->trust_store_name)) {
        changed = true;
        store = open_cert_db(trusted_CAs, NULL, &ec);
        if (!store)
          return ec;
      }
    }
    break;

  case PN_SSL_ANONYMOUS_PEER:   // hippie free love mode... :)
    break;

  default:
    ssl_log_error("Invalid peer authentication mode given.");
    return -1;
  }

  if (changed) {
    win_credential_t *new_cred = win_credential(domain->mode);
    if (!new_cred) {
      if (store)
        CertCloseStore(store, 0);
      return -1;
    }
    new_cred->cert_context = CertDuplicateCertificateContext(domain->cred->cert_context);
    new_cred->trust_store = CertDuplicateStore(domain->cred->trust_store);
    new_cred->trust_store_name = pn_strdup(domain->cred->trust_store_name);
    if (win_credential_decref(domain->cred)) {
      g2.release();
      win_credential_delete(domain->cred);
    }
    domain->cred = new_cred;
    domain->cred->server_CA_certs = store;
  }

  domain->verify_mode = mode;

  return 0;
}

int pn_ssl_domain_set_ciphers(pn_ssl_domain_t *domain, const char *ciphers)
{
  return PN_ERR;
}

int pn_ssl_domain_set_protocols(pn_ssl_domain_t *domain, const char *protocols)
{
  return PN_ERR;
}

const pn_io_layer_t ssl_layer = {
    process_input_ssl,
    process_output_ssl,
    NULL,
    NULL,
    buffered_output
};

const pn_io_layer_t ssl_input_closed_layer = {
    process_input_done,
    process_output_ssl,
    NULL,
    NULL,
    buffered_output
};

const pn_io_layer_t ssl_output_closed_layer = {
    process_input_ssl,
    process_output_done,
    NULL,
    NULL,
    buffered_output
};

const pn_io_layer_t ssl_closed_layer = {
    process_input_done,
    process_output_done,
    NULL,
    NULL,
    buffered_output
};

static pn_ssl_domain_t *default_client_domain = 0;
static pn_ssl_domain_t *default_server_domain = 0;

int pn_ssl_init(pn_ssl_t *ssl0, pn_ssl_domain_t *domain, const char *session_id)
{
  pn_transport_t *transport = get_transport_internal(ssl0);
  pni_ssl_t *ssl = transport->ssl;
  if (!ssl) return -1;
  if (ssl->state != CREATED) return -1;

  if (!domain) {
    if (transport->server) {
        if (!default_server_domain) {
            default_server_domain = pn_ssl_domain(PN_SSL_MODE_SERVER);
        }
        domain = default_server_domain;
    }
    else {
      if (!default_client_domain) {
        default_client_domain = pn_ssl_domain(PN_SSL_MODE_CLIENT);
        pn_ssl_domain_set_peer_authentication(default_client_domain, PN_SSL_VERIFY_PEER_NAME, NULL);
      }
      domain = default_client_domain;
    }
  }

  csguard g(&domain->cslock);
  csguard g2(&domain->cred->cslock);
  if (session_id && domain->mode == PN_SSL_MODE_CLIENT)
    ssl->session_id = pn_strdup(session_id);

  // If SSL doesn't specifically allow skipping encryption, require SSL
  // TODO: This is a probably a stop-gap until allow_unsecured is removed
  if (!domain->allow_unsecured) transport->encryption_required = true;

  ssl->cred = domain->cred;
  pn_incref(domain->cred);

  SECURITY_STATUS status = SEC_E_OK;
  ssl->cred_handle = win_credential_cred_handle(ssl->cred, ssl->session_id, &status);
  if (status != SEC_E_OK) {
    ssl_log_error_status(status, "Credentials handle failure");
    return -1;
  }

  ssl->state = (domain->mode == PN_SSL_MODE_CLIENT) ? CLIENT_HELLO : NEGOTIATING;
  ssl->verify_mode = domain->verify_mode;
  ssl->mode = domain->mode;
  return 0;
}


int pn_ssl_domain_allow_unsecured_client(pn_ssl_domain_t *domain)
{
  if (!domain) return -1;
  if (domain->mode != PN_SSL_MODE_SERVER) {
    ssl_log_error("Cannot permit unsecured clients - not a server.");
    return -1;
  }
  domain->allow_unsecured = true;
  return 0;
}


// TODO: This is just an untested guess
int pn_ssl_get_ssf(pn_ssl_t *ssl0)
{
  SecPkgContext_ConnectionInfo info;

  pni_ssl_t *ssl = get_ssl_internal(ssl0);
  if (ssl &&
      ssl->state == RUNNING &&
      SecIsValidHandle(&ssl->ctxt_handle) &&
      QueryContextAttributes(&ssl->ctxt_handle, SECPKG_ATTR_CONNECTION_INFO, &info) == SEC_E_OK) {
    return info.dwCipherStrength;
  }
  return 0;
}

bool pn_ssl_get_cipher_name(pn_ssl_t *ssl0, char *buffer, size_t size )
{
  pni_ssl_t *ssl = get_ssl_internal(ssl0);
  *buffer = '\0';
  if (ssl->state != RUNNING || !SecIsValidHandle(&ssl->ctxt_handle))
    return false;
  SecPkgContext_ConnectionInfo info;
  if (QueryContextAttributes(&ssl->ctxt_handle, SECPKG_ATTR_CONNECTION_INFO, &info) == SEC_E_OK) {
    // TODO: come up with string for all permutations?
    snprintf( buffer, size, "%x_%x:%x_%x:%x_%x",
              info.aiExch, info.dwExchStrength,
              info.aiCipher, info.dwCipherStrength,
              info.aiHash, info.dwHashStrength);
    return true;
  }
  return false;
}

bool pn_ssl_get_protocol_name(pn_ssl_t *ssl0, char *buffer, size_t size )
{
  pni_ssl_t *ssl = get_ssl_internal(ssl0);
  *buffer = '\0';
  if (ssl->state != RUNNING || !SecIsValidHandle(&ssl->ctxt_handle))
    return false;
  SecPkgContext_ConnectionInfo info;
  if (QueryContextAttributes(&ssl->ctxt_handle, SECPKG_ATTR_CONNECTION_INFO, &info) == SEC_E_OK) {
    if (info.dwProtocol & (SP_PROT_TLS1_CLIENT | SP_PROT_TLS1_SERVER))
      snprintf(buffer, size, "%s", "TLSv1");
    // TLSV1.1 and TLSV1.2 are supported as of XP-SP3, but not defined until VS2010
    else if ((info.dwProtocol & 0x300))
      snprintf(buffer, size, "%s", "TLSv1.1");
    else if ((info.dwProtocol & 0xC00))
      snprintf(buffer, size, "%s", "TLSv1.2");
    else {
      ssl_log_error("unexpected protocol %x", info.dwProtocol);
      return false;
    }
    return true;
  }
  return false;
}


void pn_ssl_free( pn_transport_t *transport)
{
  pni_ssl_t *ssl = transport->ssl;
  if (!ssl) return;
  ssl_log( transport, PN_LEVEL_DEBUG, "SSL socket freed." );
  // clean up Windows per TLS session data before releasing the domain count
  csguard g(&ssl->cred->cslock);
  if (SecIsValidHandle(&ssl->ctxt_handle))
    DeleteSecurityContext(&ssl->ctxt_handle);
  if (ssl->cred) {
    if (ssl->mode == PN_SSL_MODE_CLIENT && ssl->session_id == NULL) {
      // Responsible for unshared handle
      if (SecIsValidHandle(&ssl->cred_handle))
        FreeCredentialsHandle(&ssl->cred_handle);
    }
    if (win_credential_decref(ssl->cred)) {
      g.release();
      win_credential_delete(ssl->cred);
    }
  }

  g.release();

  if (ssl->session_id) free((void *)ssl->session_id);
  if (ssl->peer_hostname) free((void *)ssl->peer_hostname);
  if (ssl->sc_inbuf) free((void *)ssl->sc_inbuf);
  if (ssl->sc_outbuf) free((void *)ssl->sc_outbuf);
  if (ssl->inbuf2) pn_buffer_free(ssl->inbuf2);
  if (ssl->subject) free(ssl->subject);

  free(ssl);
}

pn_ssl_t *pn_ssl(pn_transport_t *transport)
{
  if (!transport) return NULL;
  if (transport->ssl) return (pn_ssl_t *)transport;

  pni_ssl_t *ssl = (pni_ssl_t *) calloc(1, sizeof(pni_ssl_t));
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

  transport->ssl = ssl;

  // Set up hostname from any bound connection
  if (transport->connection) {
    if (pn_string_size(transport->connection->hostname)) {
      pn_ssl_set_peer_hostname((pn_ssl_t *) transport, pn_string_get(transport->connection->hostname));
    }
  }

  SecInvalidateHandle(&ssl->cred_handle);
  SecInvalidateHandle(&ssl->ctxt_handle);
  ssl->state = CREATED;
  ssl->decrypting = true;

  return (pn_ssl_t *)transport;
}


pn_ssl_resume_status_t pn_ssl_resume_status( pn_ssl_t *ssl )
{
  // TODO
  return PN_SSL_RESUME_UNKNOWN;
}


int pn_ssl_set_peer_hostname( pn_ssl_t *ssl0, const char *hostname )
{
  pni_ssl_t *ssl = get_ssl_internal(ssl0);
  if (!ssl) return -1;

  if (ssl->peer_hostname) free((void *)ssl->peer_hostname);
  ssl->peer_hostname = NULL;
  if (hostname) {
    ssl->peer_hostname = pn_strdup(hostname);
    if (!ssl->peer_hostname) return -2;
  }
  return 0;
}

int pn_ssl_get_peer_hostname( pn_ssl_t *ssl0, char *hostname, size_t *bufsize )
{
  pni_ssl_t *ssl = get_ssl_internal(ssl0);
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

const char* pn_ssl_get_remote_subject(pn_ssl_t *ssl0)
{
  // RFC 2253 compliant, but differs from openssl's subject string with space separators and
  // ordering of multicomponent RDNs.  Made to work as similarly as possible with choice of flags.
  pni_ssl_t *ssl = get_ssl_internal(ssl0);
  if (!ssl || !SecIsValidHandle(&ssl->ctxt_handle))
    return NULL;
  if (!ssl->subject) {
    SECURITY_STATUS status;
    PCCERT_CONTEXT peer_cc = 0;
    status = QueryContextAttributes(&ssl->ctxt_handle, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &peer_cc);
    if (status != SEC_E_OK) {
      ssl_log_error_status(status, "can't obtain remote certificate subject");
      return NULL;
    }
    DWORD flags = CERT_X500_NAME_STR | CERT_NAME_STR_REVERSE_FLAG;
    DWORD strlen = CertNameToStr(peer_cc->dwCertEncodingType, &peer_cc->pCertInfo->Subject,
                                 flags, NULL, 0);
    if (strlen > 0) {
      ssl->subject = (char*) malloc(strlen);
      if (ssl->subject) {
        DWORD len = CertNameToStr(peer_cc->dwCertEncodingType, &peer_cc->pCertInfo->Subject,
                                  flags, ssl->subject, strlen);
        if (len != strlen) {
          free(ssl->subject);
          ssl->subject = NULL;
          ssl_log_error("pn_ssl_get_remote_subject failure in CertNameToStr");
        }
      }
    }
    CertFreeCertificateContext(peer_cc);
  }
  return ssl->subject;
}


/** SChannel specific: */

const char *tls_version_check(pni_ssl_t *ssl)
{
  SecPkgContext_ConnectionInfo info;
  QueryContextAttributes(&ssl->ctxt_handle, SECPKG_ATTR_CONNECTION_INFO, &info);
  // Ascending bit patterns denote newer SSL/TLS protocol versions.
  // SP_PROT_TLS1_0_SERVER is not defined until VS2010.
  return (info.dwProtocol < SP_PROT_TLS1_SERVER) ?
    "peer does not support TLS 1.0 security" : NULL;
}

static void ssl_encrypt(pn_transport_t *transport, char *app_data, size_t count)
{
  pni_ssl_t *ssl = transport->ssl;

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
  ssl_log(transport, PN_LEVEL_TRACE, "ssl_encrypt %d network bytes", ssl->network_out_pending);
}

// Returns true if decryption succeeded (even for empty content)
static bool ssl_decrypt(pn_transport_t *transport)
{
  pni_ssl_t *ssl = transport->ssl;
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
      ssl_log_error("unexpected TLS renegotiation");
      // TODO.  Fall through for now.
    default:
      ssl_failed(transport, 0);
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

static void client_handshake_init(pn_transport_t *transport)
{
  pni_ssl_t *ssl = transport->ssl;
  // Tell SChannel to create the first handshake token (ClientHello)
  // and place it in sc_outbuf
  SEC_CHAR *host = (SEC_CHAR *)(ssl->peer_hostname);
  ULONG ctxt_requested = ISC_REQ_STREAM | ISC_REQ_USE_SUPPLIED_CREDS | ISC_REQ_EXTENDED_ERROR;
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
  csguard g(&ssl->cred->cslock);
  SECURITY_STATUS status = InitializeSecurityContext(&ssl->cred_handle,
                               NULL, host, ctxt_requested, 0, 0, NULL, 0,
                               &ssl->ctxt_handle, &send_buff_desc,
                               &ctxt_attrs, NULL);

  if (status == SEC_I_CONTINUE_NEEDED) {
    ssl->sc_out_count = send_buffs[0].cbBuffer;
    ssl->network_out_pending = ssl->sc_out_count;
    // the token is the whole quantity to send
    ssl->network_outp = ssl->sc_outbuf;
    ssl_log(transport, PN_LEVEL_TRACE, "Sending client hello %d bytes", ssl->network_out_pending);
  } else {
    ssl_log_error_status(status, "InitializeSecurityContext failed");
    ssl_failed(transport, 0);
  }
}

static void client_handshake( pn_transport_t* transport) {
  pni_ssl_t *ssl = transport->ssl;
  // Feed SChannel ongoing responses from the server until the handshake is complete.
  SEC_CHAR *host = (SEC_CHAR *)(ssl->peer_hostname);
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

  SECURITY_STATUS status;
  {
    csguard g(&ssl->cred->cslock);
    status = InitializeSecurityContext(&ssl->cred_handle,
                               &ssl->ctxt_handle, host, ctxt_requested, 0, 0,
                               &token_buff_desc, 0, NULL, &send_buff_desc,
                               &ctxt_attrs, NULL);
  }

  switch (status) {
  case SEC_E_INCOMPLETE_MESSAGE:
    // Not enough - get more data from the server then try again.
    // Leave input buffers untouched.
    ssl_log(transport, PN_LEVEL_TRACE, "client handshake: incomplete record");
    ssl->sc_in_incomplete = true;
    return;

  case SEC_I_CONTINUE_NEEDED:
    // Successful handshake step, requiring data to be sent to peer.
    ssl->sc_out_count = send_buffs[0].cbBuffer;
    // the token is the whole quantity to send
    ssl->network_out_pending = ssl->sc_out_count;
    ssl->network_outp = ssl->sc_outbuf;
    ssl_log(transport, PN_LEVEL_DEBUG, "client handshake token %d bytes", ssl->network_out_pending);
    break;

  case SEC_E_OK:
    // Handshake complete.
    if (shutdown) {
      if (send_buffs[0].cbBuffer > 0) {
        ssl->sc_out_count = send_buffs[0].cbBuffer;
        // the token is the whole quantity to send
        ssl->network_out_pending = ssl->sc_out_count;
        ssl->network_outp = ssl->sc_outbuf;
        ssl_log(transport, PN_LEVEL_DEBUG, "client shutdown token %d bytes", ssl->network_out_pending);
      } else {
        ssl->state = SSL_CLOSED;
      }
      // we didn't touch sc_inbuf, no need to reset
      return;
    }
    if (send_buffs[0].cbBuffer != 0) {
      ssl_failed(transport, "unexpected final server token");
      break;
    }
    if (const char *err = tls_version_check(ssl)) {
      ssl_failed(transport, err);
      break;
    }
    if (ssl->verify_mode == PN_SSL_VERIFY_PEER || ssl->verify_mode == PN_SSL_VERIFY_PEER_NAME) {
      bool tracing = PN_SHOULD_LOG(&transport->logger, PN_SUBSYSTEM_SSL, PN_LEVEL_TRACE);
      HRESULT ec = verify_peer(ssl, ssl->cred->trust_store, ssl->peer_hostname, tracing);
      if (ec) {
        if (ssl->peer_hostname)
          ssl_log_error_status(ec, "certificate verification failed for host %s", ssl->peer_hostname);
        else
          ssl_log_error_status(ec, "certificate verification failed");
        ssl_failed(transport, "TLS certificate verification error");
        break;
      }
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
      ssl_log_error("Buffer size mismatch have %d, need %d", (int) ssl->sc_out_size, (int) max);
      ssl->state = SHUTTING_DOWN;
      ssl->app_input_closed = ssl->app_output_closed = PN_ERR;
      start_ssl_shutdown(transport);
      pn_do_error(transport, "amqp:connection:framing-error", "SSL Failure: buffer size");
      break;
    }

    ssl->state = RUNNING;
    ssl->max_data_size = max - ssl->sc_sizes.cbHeader - ssl->sc_sizes.cbTrailer;
    ssl_log(transport, PN_LEVEL_DEBUG, "client handshake successful %d max record size", max);
    break;

  case SEC_I_CONTEXT_EXPIRED:
    // ended before we got going
  default:
    ssl_log(transport, PN_LEVEL_ERROR, "client handshake failed %d", (int) status);
    ssl_failed(transport, 0);
    break;
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


static void server_handshake(pn_transport_t* transport)
{
  pni_ssl_t *ssl = transport->ssl;
  if (!ssl->protocol_detected) {
    // SChannel fails less aggressively than openssl on client hello, causing hangs
    // waiting for more bytes.  Help out here.
    pni_protocol_type_t type = pni_sniff_header(ssl->sc_inbuf, ssl->sc_in_count);
    if (type == PNI_PROTOCOL_INSUFFICIENT) {
      ssl_log(transport, PN_LEVEL_DEBUG, "server handshake: incomplete record");
      ssl->sc_in_incomplete = true;
      return;
    } else {
      ssl->protocol_detected = true;
      if (type != PNI_PROTOCOL_SSL) {
        ssl_failed(transport, "bad client hello");
        ssl->decrypting = false;
        rewind_sc_inbuf(ssl);
        return;
      }
    }
  }

  // Feed SChannel ongoing handshake records from the client until the handshake is complete.
  ULONG ctxt_requested = ASC_REQ_STREAM | ASC_REQ_EXTENDED_ERROR;
  if (ssl->verify_mode == PN_SSL_VERIFY_PEER || ssl->verify_mode == PN_SSL_VERIFY_PEER_NAME)
    ctxt_requested |= ASC_REQ_MUTUAL_AUTH;
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

  SECURITY_STATUS status;
  {
    csguard g(&ssl->cred->cslock);
    status = AcceptSecurityContext(&ssl->cred_handle, ctxt_handle_ptr,
                               &token_buff_desc, ctxt_requested, 0, &ssl->ctxt_handle,
                               &send_buff_desc, &ctxt_attrs, NULL);
  }
  bool outbound_token = false;
  switch(status) {
  case SEC_E_INCOMPLETE_MESSAGE:
    // Not enough - get more data from the client then try again.
    // Leave input buffers untouched.
    ssl_log(transport, PN_LEVEL_DEBUG, "server handshake: incomplete record");
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
        ssl_log(transport, PN_LEVEL_DEBUG, "server shutdown token %d bytes", ssl->network_out_pending);
      } else {
        ssl->state = SSL_CLOSED;
      }
      // we didn't touch sc_inbuf, no need to reset
      return;
    }
    if (const char *err = tls_version_check(ssl)) {
      ssl_failed(transport, err);
      break;
    }
    // Handshake complete.

    if (ssl->verify_mode == PN_SSL_VERIFY_PEER || ssl->verify_mode == PN_SSL_VERIFY_PEER_NAME) {
      bool tracing = PN_SHOULD_LOG(&transport->logger, PN_SUBSYSTEM_SSL, PN_LEVEL_TRACE);
      HRESULT ec = verify_peer(ssl, ssl->cred->trust_store, NULL, tracing);
      if (ec) {
        ssl_log_error_status(ec, "certificate verification failed");
        ssl_failed(transport, "certificate verification error");
        break;
      }
    }

    QueryContextAttributes(&ssl->ctxt_handle,
                             SECPKG_ATTR_STREAM_SIZES, &ssl->sc_sizes);
    max = ssl->sc_sizes.cbMaximumMessage + ssl->sc_sizes.cbHeader + ssl->sc_sizes.cbTrailer;
    if (max > ssl->sc_out_size) {
      ssl_log_error("Buffer size mismatch have %d, need %d", (int) ssl->sc_out_size, (int) max);
      ssl->state = SHUTTING_DOWN;
      ssl->app_input_closed = ssl->app_output_closed = PN_ERR;
      start_ssl_shutdown(transport);
      pn_do_error(transport, "amqp:connection:framing-error", "SSL Failure: buffer size");
      break;
    }

    if (send_buffs[0].cbBuffer != 0)
      outbound_token = true;

    ssl->state = RUNNING;
    ssl->max_data_size = max - ssl->sc_sizes.cbHeader - ssl->sc_sizes.cbTrailer;
    ssl_log(transport, PN_LEVEL_DEBUG, "server handshake successful %d max record size", max);
    break;

  case SEC_E_ALGORITHM_MISMATCH:
    ssl_log(transport, PN_LEVEL_WARNING, "server handshake failed: no common algorithm");
    ssl_failed(transport, "server handshake failed: no common algorithm");
    break;

  case SEC_I_CONTEXT_EXPIRED:
    // ended before we got going
  default:
    ssl_log(transport, PN_LEVEL_ERROR, "server handshake failed %d", (int) status);
    ssl_failed(transport, 0);
    break;
  }

  if (outbound_token) {
    // Successful handshake step, requiring data to be sent to peer.
    assert(ssl->network_out_pending == 0);
    ssl->sc_out_count = send_buffs[0].cbBuffer;
    // the token is the whole quantity to send
    ssl->network_out_pending = ssl->sc_out_count;
    ssl->network_outp = ssl->sc_outbuf;
    ssl_log(transport, PN_LEVEL_DEBUG, "server handshake token %d bytes", ssl->network_out_pending);
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

static void ssl_handshake(pn_transport_t* transport) {
  if (transport->ssl->mode == PN_SSL_MODE_CLIENT)
    client_handshake(transport);
  else {
    server_handshake(transport);
  }
}

static bool grow_inbuf2(pn_transport_t *transport, size_t minimum_size) {
  pni_ssl_t *ssl = transport->ssl;
  size_t old_capacity = pn_buffer_capacity(ssl->inbuf2);
  size_t new_capacity = old_capacity ? old_capacity * 2 : 1024;

  while (new_capacity < minimum_size)
    new_capacity *= 2;

  uint32_t max_frame = pn_transport_get_max_frame(transport);
  if (max_frame != 0) {
    if (old_capacity >= max_frame) {
      //  already big enough
      ssl_log(transport, PN_LEVEL_ERROR, "Application expecting %d bytes (> negotiated maximum frame)", new_capacity);
      ssl_failed(transport, "TLS: transport maximum frame size error");
      return false;
    }
  }

  size_t extra_bytes = new_capacity - pn_buffer_size(ssl->inbuf2);
  int err = pn_buffer_ensure(ssl->inbuf2, extra_bytes);
  if (err) {
    ssl_log(transport, PN_LEVEL_ERROR, "TLS memory allocation failed for %d bytes", max_frame);
    ssl_failed(transport, "TLS memory allocation failed");
    return false;
  }
  return true;
}


// Peer initiated a session end by sending us a shutdown alert (and we should politely
// reciprocate), or else we are initiating the session end (and will not bother to wait
// for the peer shutdown alert). Stop processing input immediately, and stop processing
// output once this is sent.

static void start_ssl_shutdown(pn_transport_t *transport)
{
  pni_ssl_t *ssl = transport->ssl;
  assert(ssl->network_out_pending == 0);
  if (ssl->queued_shutdown)
    return;
  ssl->queued_shutdown = true;
  ssl_log(transport, PN_LEVEL_INFO, "Shutting down SSL connection...");

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

  // Next handshake will generate the shutdown alert token
  ssl_handshake(transport);
}

static void rewind_sc_inbuf(pni_ssl_t *ssl)
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

static void app_inbytes_add(pn_transport_t *transport)
{
  pni_ssl_t *ssl = transport->ssl;
  if (!ssl->app_inbytes.start) {
    ssl->app_inbytes.start = ssl->in_data;
    ssl->app_inbytes.size = ssl->in_data_count;
    return;
  }

  if (ssl->double_buffered) {
    if (pn_buffer_available(ssl->inbuf2) == 0) {
      if (!grow_inbuf2(transport, 1024))
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


static void app_inbytes_progress(pn_transport_t *transport, size_t minimum)
{
  pni_ssl_t *ssl = transport->ssl;
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
        if (!grow_inbuf2(transport, minimum))
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
        if (!grow_inbuf2(transport, minimum))
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


static void app_inbytes_advance(pn_transport_t *transport, size_t consumed)
{
  pni_ssl_t *ssl = transport->ssl;
  if (consumed == 0) {
    // more contiguous bytes required
    app_inbytes_progress(transport, ssl->app_inbytes.size + 1);
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
    app_inbytes_progress(transport, 0);
}

static void read_closed(pn_transport_t *transport, unsigned int layer, ssize_t error)
{
  pni_ssl_t *ssl = transport->ssl;
  if (ssl->app_input_closed)
    return;
  if (ssl->state == RUNNING && !error) {
    // Signal end of stream
    ssl->app_input_closed = transport->io_layers[layer+1]->process_input(transport, layer+1, ssl->app_inbytes.start, 0);
  }
  if (!ssl->app_input_closed)
    ssl->app_input_closed = error ? error : PN_ERR;

  if (ssl->app_output_closed) {
    // both sides of app closed, and no more app output pending:
    ssl->state = SHUTTING_DOWN;
    if (ssl->network_out_pending == 0 && !ssl->queued_shutdown) {
      start_ssl_shutdown(transport);
    }
  }
}


// Read up to "available" bytes from the network, decrypt it and pass plaintext to application.

static ssize_t process_input_ssl(pn_transport_t *transport, unsigned int layer, const char *input_data, size_t available)
{
  pni_ssl_t *ssl = transport->ssl;
  ssl_log( transport, PN_LEVEL_TRACE, "process_input_ssl( data size=%d )",available );
  ssize_t consumed = 0;
  ssize_t forwarded = 0;
  bool new_app_input;

  if (available == 0) {
    // No more inbound network data
    read_closed(transport, layer, 0);
    return 0;
  }

  do {
    if (ssl->sc_input_shutdown) {
      // TLS protocol shutdown detected on input, so we are done.
      read_closed(transport, layer, 0);
      return PN_EOS;
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
        ssl_handshake(transport);
      } else {
        if (ssl_decrypt(transport)) {
          // Ignore TLS Record with 0 length data (does not mean EOS)
          if (ssl->in_data_size > 0) {
            new_app_input = true;
            app_inbytes_add(transport);
          } else {
            assert(ssl->decrypting == false);
            rewind_sc_inbuf(ssl);
          }
        }
        ssl_log(transport, PN_LEVEL_TRACE, "Next decryption, %d left over", available);
      }
    }

    if (ssl->state == SHUTTING_DOWN) {
      if (ssl->network_out_pending == 0 && !ssl->queued_shutdown) {
        start_ssl_shutdown(transport);
      }
    } else if (ssl->state == SSL_CLOSED) {
      return PN_EOS;
    }

    // Consume or discard the decrypted bytes
    if (new_app_input && (ssl->state == RUNNING || ssl->state == SHUTTING_DOWN)) {
      // present app_inbytes to io_next only if it has new content
      while (ssl->app_inbytes.size > 0) {
        if (!ssl->app_input_closed) {
          ssize_t count = transport->io_layers[layer+1]->process_input(transport, layer+1, ssl->app_inbytes.start, ssl->app_inbytes.size);
          if (count > 0) {
            forwarded += count;
            // advance() can increase app_inbytes.size if double buffered
            app_inbytes_advance(transport, count);
            ssl_log(transport, PN_LEVEL_TRACE, "Application consumed %d bytes from peer", (int) count);
          } else if (count == 0) {
            size_t old_size = ssl->app_inbytes.size;
            app_inbytes_advance(transport, 0);
            if (ssl->app_inbytes.size == old_size) {
              break;  // no additional contiguous decrypted data available, get more network data
            }
          } else {
            // count < 0
            ssl_log(transport, PN_LEVEL_WARNING, "Application layer closed its input, error=%d (discarding %d bytes)",
                 (int) count, (int)ssl->app_inbytes.size);
            app_inbytes_advance(transport, ssl->app_inbytes.size);    // discard
            read_closed(transport, layer, count);
          }
        } else {
          ssl_log(transport, PN_LEVEL_WARNING, "Input closed discard %d bytes",
               (int)ssl->app_inbytes.size);
          app_inbytes_advance(transport, ssl->app_inbytes.size);      // discard
        }
      }
    }
  } while (available || (ssl->sc_in_count && !ssl->sc_in_incomplete));

  if (ssl->state >= SHUTTING_DOWN) {
    if (ssl->app_input_closed || ssl->sc_input_shutdown) {
      // Next layer doesn't want more bytes, or it can't process without more data than it has seen so far
      // but the ssl stream has ended
      consumed = ssl->app_input_closed ? ssl->app_input_closed : PN_EOS;
      if (transport->io_layers[layer]==&ssl_output_closed_layer) {
        transport->io_layers[layer] = &ssl_closed_layer;
      } else {
        transport->io_layers[layer] = &ssl_input_closed_layer;
      }
    }
  }
  ssl_log(transport, PN_LEVEL_TRACE, "process_input_ssl() returning %d, forwarded %d", (int) consumed, (int) forwarded);
  return consumed;
}

static ssize_t process_output_ssl( pn_transport_t *transport, unsigned int layer, char *buffer, size_t max_len)
{
  pni_ssl_t *ssl = transport->ssl;
  if (!ssl) return PN_EOS;
  ssl_log( transport, PN_LEVEL_TRACE, "process_output_ssl( max_len=%d )",max_len );

  ssize_t written = 0;
  ssize_t total_app_bytes = 0;
  bool work_pending;

  if (ssl->state == CLIENT_HELLO) {
    // output buffers eclusively for internal handshake use until negotiation complete
    client_handshake_init(transport);
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
        app_bytes = transport->io_layers[layer+1]->process_output(transport, layer+1, app_outp, remaining);
        if (app_bytes > 0) {
          app_outp += app_bytes;
          remaining -= app_bytes;
          ssl_log( transport, PN_LEVEL_TRACE, "Gathered %d bytes from app to send to peer", app_bytes );
        } else {
          if (app_bytes < 0) {
            ssl_log(transport, PN_LEVEL_WARNING, "Application layer closed its output, error=%d (%d bytes pending send)",
                 (int) app_bytes, (int) ssl->network_out_pending);
            ssl->app_output_closed = app_bytes;
            if (ssl->app_input_closed)
              ssl->state = SHUTTING_DOWN;
          } else if (total_app_bytes == 0 && ssl->app_input_closed) {
            // We've drained all the App layer can provide
            ssl_log(transport, PN_LEVEL_WARNING, "Application layer blocked on input, closing");
            ssl->state = SHUTTING_DOWN;
            ssl->app_output_closed = PN_ERR;
          }
        }
      } while (app_bytes > 0);
      if (app_outp > app_data) {
        work_pending = (max_len > 0);
        ssl_encrypt(transport, app_data, app_outp - app_data);
      }
    }

    if (ssl->network_out_pending == 0) {
      if (ssl->state == SHUTTING_DOWN) {
        if (!ssl->queued_shutdown) {
          start_ssl_shutdown(transport);
          work_pending = true;
        } else {
          ssl->state = SSL_CLOSED;
        }
      }
      else if (ssl->state == NEGOTIATING && ssl->app_input_closed) {
        ssl->app_output_closed = PN_EOS;
        ssl->state = SSL_CLOSED;
      }
    }
  } while (work_pending);

  if (written == 0 && ssl->state == SSL_CLOSED) {
    written = ssl->app_output_closed ? ssl->app_output_closed : PN_EOS;
    if (transport->io_layers[layer]==&ssl_input_closed_layer) {
      transport->io_layers[layer] = &ssl_closed_layer;
    } else {
      transport->io_layers[layer] = &ssl_output_closed_layer;
    }
  }
  ssl_log(transport, PN_LEVEL_TRACE, "process_output_ssl() returning %d", (int) written);
  return written;
}


static ssize_t process_input_done(pn_transport_t *transport, unsigned int layer, const char *input_data, size_t len)
{
  return PN_EOS;
}

static ssize_t process_output_done(pn_transport_t *transport, unsigned int layer, char *input_data, size_t len)
{
  return PN_EOS;
}

// return # output bytes sitting in this layer
static size_t buffered_output(pn_transport_t *transport)
{
  size_t count = 0;
  pni_ssl_t *ssl = transport->ssl;
  if (ssl) {
    count += ssl->network_out_pending;
    if (count == 0 && ssl->state == SHUTTING_DOWN && ssl->queued_shutdown)
      count++;
  }
  return count;
}

static HCERTSTORE open_cert_db(const char *store_name, const char *passwd, int *error) {
  *error = 0;
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
    if (!pn_strcasecmp(store_name, "personal")) store_name= "my";
    cert_store = CertOpenStore(CERT_STORE_PROV_SYSTEM_A, 0, NULL,
                               CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG |
                               sys_store_type, store_name);
    if (!cert_store) {
      ssl_log_error_status(GetLastError(), "Failed to open system certificate store %s", store_name);
      *error = -3;
      return NULL;
    }
  } else {
    // PKCS#12 file
    HANDLE cert_file = CreateFile(store_name, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, NULL);
    if (INVALID_HANDLE_VALUE == cert_file) {
      HRESULT status = GetLastError();
      ssl_log_error_status(status, "Failed to open the file holding the private key: %s", store_name);
      *error = -4;
      return NULL;
    }
    DWORD nread = 0L;
    const DWORD file_size = GetFileSize(cert_file, NULL);
    char *buf = NULL;
    if (INVALID_FILE_SIZE != file_size)
      buf = (char *) malloc(file_size);
    if (!buf || !ReadFile(cert_file, buf, file_size, &nread, NULL)
        || file_size != nread) {
      HRESULT status = GetLastError();
      CloseHandle(cert_file);
      free(buf);
      ssl_log_error_status(status, "Reading the private key from file failed %s", store_name);
      *error = -5;
      return NULL;
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
        *error = -6;
        return NULL;
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
      *error = -7;
      return NULL;
    }

    free(buf);
  }

  return cert_store;
}

static bool store_contains(HCERTSTORE store, PCCERT_CONTEXT cert)
{
  DWORD find_type = CERT_FIND_EXISTING; // Require exact match
  PCCERT_CONTEXT tcert = CertFindCertificateInStore(store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                                                    0, find_type, cert, 0);
  if (tcert) {
    CertFreeCertificateContext(tcert);
    return true;
  }
  return false;
}

/* Match the DNS name pattern from the peer certificate against our configured peer
   hostname */
static bool match_dns_pattern(const char *hostname, const char *pattern, int plen)
{
  int slen = (int) strlen(hostname);
  if (memchr( pattern, '*', plen ) == NULL)
    return (plen == slen &&
            pn_strncasecmp( pattern, hostname, plen ) == 0);

  /* dns wildcarded pattern - RFC2818 */
  char plabel[64];   /* max label length < 63 - RFC1034 */
  char slabel[64];

  while (plen > 0 && slen > 0) {
    const char *cptr;
    int len;

    cptr = (const char *) memchr( pattern, '.', plen );
    len = (cptr) ? cptr - pattern : plen;
    if (len > (int) sizeof(plabel) - 1) return false;
    memcpy( plabel, pattern, len );
    plabel[len] = 0;
    if (cptr) ++len;    // skip matching '.'
    pattern += len;
    plen -= len;

    cptr = (const char *) memchr( hostname, '.', slen );
    len = (cptr) ? cptr - hostname : slen;
    if (len > (int) sizeof(slabel) - 1) return false;
    memcpy( slabel, hostname, len );
    slabel[len] = 0;
    if (cptr) ++len;    // skip matching '.'
    hostname += len;
    slen -= len;

    char *star = strchr( plabel, '*' );
    if (!star) {
      if (pn_strcasecmp( plabel, slabel )) return false;
    } else {
      *star = '\0';
      char *prefix = plabel;
      int prefix_len = strlen(prefix);
      char *suffix = star + 1;
      int suffix_len = strlen(suffix);
      if (prefix_len && pn_strncasecmp( prefix, slabel, prefix_len )) return false;
      if (suffix_len && pn_strncasecmp( suffix,
                                     slabel + (strlen(slabel) - suffix_len),
                                     suffix_len )) return false;
    }
  }

  return plen == slen;
}

// Caller must free the returned buffer
static char* wide_to_utf8(LPWSTR wstring)
{
  int len = WideCharToMultiByte(CP_UTF8, 0, wstring, -1, 0, 0, 0, 0);
  if (!len) {
    ssl_log_error_status(GetLastError(), "converting UCS2 to UTF8");
    return NULL;
  }
  char *p = (char *) malloc(len);
  if (!p) return NULL;
  if (WideCharToMultiByte(CP_UTF8, 0, wstring, -1, p, len, 0, 0))
    return p;
  ssl_log_error_status(GetLastError(), "converting UCS2 to UTF8");
  free (p);
  return NULL;
}

static bool server_name_matches(const char *server_name, CERT_EXTENSION *alt_name_ext, PCCERT_CONTEXT cert)
{
  // As for openssl.c: alt names first, then CN
  bool matched = false;

  if (alt_name_ext) {
    CERT_ALT_NAME_INFO* alt_name_info = NULL;
    DWORD size = 0;
    if(!CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, szOID_SUBJECT_ALT_NAME2,
                            alt_name_ext->Value.pbData, alt_name_ext->Value.cbData,
                            CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_NOCOPY_FLAG,
                            0, &alt_name_info, &size)) {
      ssl_log_error_status(GetLastError(), "Alternative name match internal error");
      return false;
    }

    int name_ct = alt_name_info->cAltEntry;
    for (int i = 0; !matched && i < name_ct; ++i) {
      if (alt_name_info->rgAltEntry[i].dwAltNameChoice == CERT_ALT_NAME_DNS_NAME) {
        char *alt_name = wide_to_utf8(alt_name_info->rgAltEntry[i].pwszDNSName);
        if (alt_name) {
          matched = match_dns_pattern(server_name, (const char *) alt_name, strlen(alt_name));
          free(alt_name);
        }
      }
    }
    LocalFree(alt_name_info);
  }

  if (!matched) {
    PCERT_INFO info = cert->pCertInfo;
    DWORD len = CertGetNameString(cert, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, 0, 0);
    char *name = (char *) malloc(len);
    if (name) {
      int count = CertGetNameString(cert, CERT_NAME_ATTR_TYPE, 0, szOID_COMMON_NAME, name, len);
      if (count)
        matched = match_dns_pattern(server_name, (const char *) name, strlen(name));
      free(name);
    }
  }
  return matched;
}

const char* pn_ssl_get_remote_subject_subfield(pn_ssl_t *ssl0, pn_ssl_cert_subject_subfield field)
{
    return NULL;
}

int pn_ssl_get_cert_fingerprint(pn_ssl_t *ssl0,
                                          char *fingerprint,
                                          size_t fingerprint_length,
                                          pn_ssl_hash_alg hash_alg)
{
    *fingerprint = '\0';
    return -1;
}

static HRESULT verify_peer(pni_ssl_t *ssl, HCERTSTORE root_store, const char *server_name, bool tracing)
{
  // Free/release the following before return:
  PCCERT_CONTEXT peer_cc = 0;
  PCCERT_CONTEXT trust_anchor = 0;
  PCCERT_CHAIN_CONTEXT chain_context = 0;
  wchar_t *nameUCS2 = 0;

  if (server_name && strlen(server_name) > 255) {
    ssl_log_error("invalid server name: %s", server_name);
    return WSAENAMETOOLONG;
  }

  // Get peer's certificate.
  SECURITY_STATUS status;
  status = QueryContextAttributes(&ssl->ctxt_handle, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &peer_cc);
  if (status != SEC_E_OK) {
    ssl_log_error_status(status, "can't obtain remote peer certificate information");
    return status;
  }

  // Build the peer's certificate chain.  Multiple chains may be built but we
  // care about rgpChain[0], which is the best.  Custom root stores are not
  // allowed until W8/server 2012: see CERT_CHAIN_ENGINE_CONFIG.  For now, we
  // manually override to taste.

  // Chain verification functions give false reports for CRL if the trust anchor
  // is not in the official root store.  We ignore CRL completely if it doesn't
  // apply to any untrusted certs in the chain, and defer to SChannel's veto
  // otherwise.  To rely on CRL, the CA must be in both the official system
  // trusted root store and the Proton cred->trust_store.  To defeat CRL, the
  // most distal cert with CRL must be placed in the Proton cred->trust_store.
  // Similarly, certificate usage checking is overly strict at times.

  CERT_CHAIN_PARA desc;
  memset(&desc, 0, sizeof(desc));
  desc.cbSize = sizeof(desc);

  LPSTR usages[] = { szOID_PKIX_KP_SERVER_AUTH };
  DWORD n_usages = sizeof(usages) / sizeof(LPSTR);
  desc.RequestedUsage.dwType = USAGE_MATCH_TYPE_OR;
  desc.RequestedUsage.Usage.cUsageIdentifier = n_usages;
  desc.RequestedUsage.Usage.rgpszUsageIdentifier = usages;

  if(!CertGetCertificateChain(0, peer_cc, 0, peer_cc->hCertStore, &desc,
                             CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT |
                             CERT_CHAIN_CACHE_END_CERT,
                             0, &chain_context)){
    HRESULT st = GetLastError();
    ssl_log_error_status(st, "Basic certificate chain check failed");
    CertFreeCertificateContext(peer_cc);
    return st;
  }
  if (chain_context->cChain < 1 || chain_context->rgpChain[0]->cElement < 1) {
    ssl_log_error("empty chain with status %x %x", chain_context->TrustStatus.dwErrorStatus,
                 chain_context->TrustStatus.dwInfoStatus);
    return SEC_E_CERT_UNKNOWN;
  }

  int chain_len = chain_context->rgpChain[0]->cElement;
  PCCERT_CONTEXT leaf_cert = chain_context->rgpChain[0]->rgpElement[0]->pCertContext;
  PCCERT_CONTEXT trunk_cert = chain_context->rgpChain[0]->rgpElement[chain_len - 1]->pCertContext;
  if (tracing)
    // See doc for CERT_CHAIN_POLICY_STATUS for bit field error and info status values
    ssl_log_error("status for complete chain: error bits %x info bits %x",
                  chain_context->TrustStatus.dwErrorStatus, chain_context->TrustStatus.dwInfoStatus);

  // Supplement with checks against Proton's trusted_ca_db, custom revocation and usage.
  HRESULT error = 0;
  do {
    // Do not return from this do loop.  Set error = SEC_E_XXX and break.
    bool revocable = false;  // unless we see any untrusted certs that could be
    for (int i = 0; i < chain_len; i++) {
      CERT_CHAIN_ELEMENT *ce = chain_context->rgpChain[0]->rgpElement[i];
      PCCERT_CONTEXT cc = ce->pCertContext;
      if (cc->pCertInfo->dwVersion != CERT_V3) {
        if (tracing)
          ssl_log_error("certificate chain element %d is not version 3", i);
        error = SEC_E_CERT_WRONG_USAGE; // A fossil
        break;
      }

      if (!trust_anchor && store_contains(root_store, cc))
        trust_anchor = CertDuplicateCertificateContext(cc);

      int n_ext = cc->pCertInfo->cExtension;
      for (int ii = 0; ii < n_ext && !revocable && !trust_anchor; ii++) {
        CERT_EXTENSION *p = &cc->pCertInfo->rgExtension[ii];
        // rfc 5280 extensions for revocation
        if (!strcmp(p->pszObjId, szOID_AUTHORITY_INFO_ACCESS) ||
            !strcmp(p->pszObjId, szOID_CRL_DIST_POINTS) ||
            !strcmp(p->pszObjId, szOID_FRESHEST_CRL)) {
          revocable = true;
        }
      }

      if (tracing) {
        char name[512];
        const char *is_anchor = (cc == trust_anchor) ? " trust anchor" : "";
        if (!CertNameToStr(cc->dwCertEncodingType, &cc->pCertInfo->Subject,
                           CERT_X500_NAME_STR | CERT_NAME_STR_NO_PLUS_FLAG, name, sizeof(name)))
          strcpy(name, "[too long]");
        ssl_log_error("element %d (name: %s)%s error bits %x info bits %x", i, name, is_anchor,
                      ce->TrustStatus.dwErrorStatus, ce->TrustStatus.dwInfoStatus);
      }
    }
    if (error)
      break;

    if (!trust_anchor) {
      // We don't trust any of the certs in the chain, see if the last cert
      // is issued by a Proton trusted CA.
      DWORD flags = CERT_STORE_NO_ISSUER_FLAG || CERT_STORE_SIGNATURE_FLAG ||
        CERT_STORE_TIME_VALIDITY_FLAG;
      trust_anchor = CertGetIssuerCertificateFromStore(root_store, trunk_cert, 0, &flags);
      if (trust_anchor) {
        if (tracing) {
          if (flags & CERT_STORE_SIGNATURE_FLAG)
            ssl_log_error("root certificate signature failure");
          if (flags & CERT_STORE_TIME_VALIDITY_FLAG)
            ssl_log_error("root certificate time validity failure");
        }
        if (flags) {
          CertFreeCertificateContext(trust_anchor);
          trust_anchor = 0;
        }
      }
    }
    if (!trust_anchor) {
      error = SEC_E_UNTRUSTED_ROOT;
      break;
    }

    bool strict_usage = false;
    CERT_EXTENSION *leaf_alt_names = 0;
    if (leaf_cert != trust_anchor) {
      int n_ext = leaf_cert->pCertInfo->cExtension;
      for (int ii = 0; ii < n_ext; ii++) {
        CERT_EXTENSION *p = &leaf_cert->pCertInfo->rgExtension[ii];
        if (!strcmp(p->pszObjId, szOID_ENHANCED_KEY_USAGE))
          strict_usage = true;
        if (!strcmp(p->pszObjId, szOID_SUBJECT_ALT_NAME2))
          if (p->Value.pbData)
            leaf_alt_names = p;
      }
    }

    if (server_name) {
      int len = strlen(server_name);
      nameUCS2 = (wchar_t *) calloc(len + 1, sizeof(wchar_t));
      int nwc = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, server_name, len, &nameUCS2[0], len);
      if (!nwc) {
        error = GetLastError();
        ssl_log_error_status(error, "Error converting server name from UTF8");
        break;
      }
    }

    // SSL-specific parameters (ExtraPolicy below)
    SSL_EXTRA_CERT_CHAIN_POLICY_PARA ssl_desc;
    memset(&ssl_desc, 0, sizeof(ssl_desc));
    ssl_desc.cbSize = sizeof(ssl_desc);
    ssl_desc.pwszServerName = nameUCS2;
    ssl_desc.dwAuthType = nameUCS2 ? AUTHTYPE_SERVER : AUTHTYPE_CLIENT;
    ssl_desc.fdwChecks = SECURITY_FLAG_IGNORE_UNKNOWN_CA;
    if (server_name)
      ssl_desc.fdwChecks |= SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
    if (!revocable)
      ssl_desc.fdwChecks |= SECURITY_FLAG_IGNORE_REVOCATION;
    if (!strict_usage)
      ssl_desc.fdwChecks |= SECURITY_FLAG_IGNORE_WRONG_USAGE;

    // General certificate chain parameters
    CERT_CHAIN_POLICY_PARA chain_desc;
    memset(&chain_desc, 0, sizeof(chain_desc));
    chain_desc.cbSize = sizeof(chain_desc);
    chain_desc.dwFlags = CERT_CHAIN_POLICY_ALLOW_UNKNOWN_CA_FLAG;
    if (!revocable)
      chain_desc.dwFlags |= CERT_CHAIN_POLICY_IGNORE_ALL_REV_UNKNOWN_FLAGS;
    if (!strict_usage)
      chain_desc.dwFlags |= CERT_CHAIN_POLICY_IGNORE_WRONG_USAGE_FLAG;
    chain_desc.pvExtraPolicyPara = &ssl_desc;

    CERT_CHAIN_POLICY_STATUS chain_status;
    memset(&chain_status, 0, sizeof(chain_status));
    chain_status.cbSize = sizeof(chain_status);

    if (!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, chain_context,
                                          &chain_desc, &chain_status)) {
      error = GetLastError();
      // Failure to complete the check, does not (in)validate the cert.
      ssl_log_error_status(error, "Supplemental certificate chain check failed");
      break;
    }

    if (chain_status.dwError) {
      error = chain_status.dwError;
      if (tracing) {
        ssl_log_error_status(chain_status.dwError, "Certificate chain verification error");
        if (chain_status.lChainIndex == 0 && chain_status.lElementIndex != -1) {
          int idx = chain_status.lElementIndex;
          CERT_CHAIN_ELEMENT *ce = chain_context->rgpChain[0]->rgpElement[idx];
          ssl_log_error("  chain failure at %d error/info: %x %x", idx,
                        ce->TrustStatus.dwErrorStatus, ce->TrustStatus.dwInfoStatus);
        }
      }
      break;
    }

    if (server_name && ssl->verify_mode == PN_SSL_VERIFY_PEER_NAME &&
        !server_name_matches(server_name, leaf_alt_names, leaf_cert)) {
      error = SEC_E_WRONG_PRINCIPAL;
      break;
    }
    else if (ssl->verify_mode == PN_SSL_VERIFY_PEER_NAME && !server_name) {
      ssl_log_error("Error: configuration error: PN_SSL_VERIFY_PEER_NAME configured, but no peer hostname set!");
      error = SEC_E_WRONG_PRINCIPAL;
      break;
    }
  } while (0);

  if (tracing && !error)
    ssl_log_error("peer certificate authenticated");

  // Lots to clean up.
  if (peer_cc)
    CertFreeCertificateContext(peer_cc);
  if (trust_anchor)
    CertFreeCertificateContext(trust_anchor);
  if (chain_context)
    CertFreeCertificateChain(chain_context);
  free(nameUCS2);
  return error;
}

}
