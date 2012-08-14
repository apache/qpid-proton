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

#define _POSIX_C_SOURCE 1

#include <proton/driver.h>
#include "../driver_impl.h"
#include "../util.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/socket.h>


/** @file
 * SSL/TLS support API.
 *
 * This file contains stub implementations of the SSL/TLS API.  This implementation is
 * used if there is no SSL/TLS support in the system's environment.
 */

static int ssl_initialized;

struct pn_listener_ssl_t {
    SSL_CTX *ctx;
    bool allow_unsecured;
    bool ca_db;
    char *keyfile_pw;
};
typedef struct pn_listener_ssl_t pn_listener_ssl_t;


struct pn_connector_ssl_t {

    enum { SSL_CLIENT, SSL_SERVER } mode;
    SSL_CTX *ctx;   // NULL if mode=SSL_SERVER - uses listener's ctx
    SSL *ssl;
    BIO *sbio;
    char *keyfile_pw;
    bool read_stalled;  // SSL has data to read, but client buffer is full.
};
typedef struct pn_connector_ssl_t pn_connector_ssl_t;

/* */
static int keyfile_pw_cb(char *buf, int size, int rwflag, void *userdata);
static int configure_ca_database(SSL_CTX *ctx, const char *certificate_db);
static int start_check_for_ssl( pn_connector_t *client );
static int handle_check_for_ssl( pn_connector_t *client );
static int start_ssl_connect(pn_connector_t *client);
static int handle_ssl_connect( pn_connector_t *client );
static int start_ssl_accept(pn_connector_t *client);
static int handle_ssl_accept(pn_connector_t *client);
static int start_ssl_connection_up( pn_connector_t *c );
static int handle_ssl_connection_up( pn_connector_t *c );
static int start_clear_connected( pn_connector_t *c );
static int start_ssl_shutdown( pn_connector_t *c );
static int handle_ssl_shutdown( pn_connector_t *c );


#if 1
static int verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
    fprintf(stderr, "VERIFY_CALLBACK: pre-verify-ok=%d\n", preverify_ok);

           char    buf[256];
           X509   *err_cert;
           int     err, depth;

           err_cert = X509_STORE_CTX_get_current_cert(ctx);
           err = X509_STORE_CTX_get_error(ctx);
           depth = X509_STORE_CTX_get_error_depth(ctx);

           /*
            * Retrieve the pointer to the SSL of the connection currently treated
            * and the application specific data stored into the SSL object.
            */

           X509_NAME_oneline(X509_get_subject_name(err_cert), buf, 256);

           /*
            * Catch a too long certificate chain. The depth limit set using
            * SSL_CTX_set_verify_depth() is by purpose set to "limit+1" so
            * that whenever the "depth>verify_depth" condition is met, we
            * have violated the limit and want to log this error condition.
            * We must do it here, because the CHAIN_TOO_LONG error would not
            * be found explicitly; only errors introduced by cutting off the
            * additional certificates would be logged.
            */
           if (!preverify_ok) {
               printf("verify error:num=%d:%s:depth=%d:%s\n", err,
                        X509_verify_cert_error_string(err), depth, buf);
           }

           /*
            * At this point, err contains the last verification error. We can use
            * it for something special
            */
           if (!preverify_ok && (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT))
           {
             X509_NAME_oneline(X509_get_issuer_name(ctx->current_cert), buf, 256);
             printf("issuer= %s\n", buf);
           }

    return 1;
}
#endif

/** Public API - visible to application code */

int pn_listener_ssl_server_init(pn_listener_t *listener,
                                const char *certificate_file,
                                const char *private_key_file,
                                const char *password,
                                const char *certificate_db)
{
    listener->ssl = calloc(1, sizeof(pn_listener_ssl_t));
    // note: see pn_listener_free_ssl for cleanup/deallocation
    if (!listener->ssl) {
        perror("calloc()");
        return -1;
    }
    pn_listener_ssl_t *impl = listener->ssl;

    if (!ssl_initialized) {
        ssl_initialized = 1;
        SSL_library_init();
        SSL_load_error_strings();
    }

    impl->ctx = SSL_CTX_new(SSLv23_server_method());
    if (!impl->ctx) {
        perror("unable to initialize SSL context");
        return -2;
    }

    if (SSL_CTX_use_certificate_chain_file(impl->ctx, certificate_file) != 1) {
        fprintf(stderr, "SSL_CTX_use_certificate_chain_file( %s ) failed\n", certificate_file);
        return -3;
    }

    if (password) {
        impl->keyfile_pw = pn_strdup(password);  // @todo: obfuscate me!!!
        SSL_CTX_set_default_passwd_cb(impl->ctx, keyfile_pw_cb);
        SSL_CTX_set_default_passwd_cb_userdata(impl->ctx, impl->keyfile_pw);
    }

    if (SSL_CTX_use_PrivateKey_file(impl->ctx, private_key_file, SSL_FILETYPE_PEM) != 1) {
        fprintf(stderr, "SSL_CTX_use_PrivateKey_file( %s ) failed\n", private_key_file);
        return -4;
    }

    if (certificate_db) {
        impl->ca_db = true;
        if (configure_ca_database(impl->ctx, certificate_db)) {
            return -5;
        }
    }
    return 0;
}


int pn_listener_ssl_allow_unsecured_clients(pn_listener_t *listener)
{
    if (!listener->ssl) {
        fprintf(stderr, "SSL not initialized");
        return -1;
    }

    listener->ssl->allow_unsecured = true;
    return 0;
}



int pn_connector_ssl_client_init(pn_connector_t *connector,
                                 const char *certificate_db)
{
    if (connector->listener)
        return -1;   // not for listener-based connectors

    connector->ssl = calloc(1, sizeof(pn_connector_ssl_t));
    if (!connector->ssl) {
        perror("calloc()");
        return -1;
    }
    pn_connector_ssl_t *impl = connector->ssl;

    impl->mode = SSL_CLIENT;

    if (!ssl_initialized) {
        ssl_initialized = 1;
        SSL_library_init();
        SSL_load_error_strings();
    }

    impl->ctx = SSL_CTX_new(SSLv23_client_method());
    if (!impl->ctx) {
        perror("unable to initialize SSL context");
        return -2;
    }

    if (configure_ca_database(impl->ctx, certificate_db)) {
        return -3;
    }

    /* Force servers to authenticate */
    SSL_CTX_set_verify( connector->ssl->ctx,
                        SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                        verify_callback /*?verify callback?*/ );
    //0 /*?verify callback?*/ );

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
    SSL_CTX_set_verify_depth(connector->ssl->ctx, 1);
#endif

    /* make the socket nonblocking*/
    int flags = fcntl(connector->fd, F_GETFL, 0);

    flags |= O_NONBLOCK;
    if (fcntl(connector->fd, F_SETFL, flags)) {
        // @todo: better error handling - fail this connection
        perror("fcntl");
        return -1;
    }

    return start_ssl_connect( connector );    // start connecting!
}


int pn_connector_ssl_set_client_auth(pn_connector_t *connector,
                                     const char *certificate_file,
                                     const char *private_key_file,
                                     const char *password)
{
    // @todo check state to verify not yet connected!

    pn_connector_ssl_t *impl = connector->ssl;

    if (!impl || impl->mode != SSL_CLIENT) {
        fprintf(stderr, "Error: connector not configured as SSL client.\n");
        return -1;
    }

    if (SSL_CTX_use_certificate_chain_file(impl->ctx, certificate_file) != 1) {
        fprintf(stderr, "SSL_CTX_use_certificate_chain_file( %s ) failed\n", certificate_file);
        return -3;
    }

    if (password) {
        impl->keyfile_pw = pn_strdup(password);  // @todo: obfuscate me!!!
        SSL_CTX_set_default_passwd_cb(impl->ctx, keyfile_pw_cb);
        SSL_CTX_set_default_passwd_cb_userdata(impl->ctx, impl->keyfile_pw);
    }

    if (SSL_CTX_use_PrivateKey_file(impl->ctx, private_key_file, SSL_FILETYPE_PEM) != 1) {
        fprintf(stderr, "SSL_CTX_use_PrivateKey_file( %s ) failed\n", private_key_file);
        return -4;
    }

    return 0;
}


int pn_connector_ssl_authenticate_client(pn_connector_t *connector,
                                         const char *trusted_CAs_file)
{
    pn_connector_ssl_t *impl = connector->ssl;

    if (!impl || impl->mode != SSL_SERVER) {
        fprintf(stderr, "Error: connector not configured as SSL server.\n");
    }

    // @todo: make sure certificate_db is set...


    // load the CA's that we trust - this will be sent to the client when client
    // authentication is requested
    STACK_OF(X509_NAME) *cert_names;
    cert_names = SSL_load_client_CA_file( trusted_CAs_file );
    if (cert_names != NULL)
        SSL_set_client_CA_list(impl->ssl, cert_names);
    else {
        fprintf(stderr, "Unable to process file of trusted_CAs: %s\n", trusted_CAs_file);
        return -1;
    }

    SSL_set_verify( impl->ssl,
                    SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                    verify_callback /*?verify callback?*/);
    //0 /*?verify callback?*/ );
    return 0;
}


/** Abstraction API - visible to Driver only - see ssl.h */


int pn_driver_ssl_data_ready( pn_driver_t *d )
{
    int ready = 0;

    // check if any stalled readers exist, and if they have buffer space available.
    pn_connector_t *c = d->connector_head;
    while (c) {
        if (!c->closed && c->ssl) {
            pn_connector_ssl_t *impl = c->ssl;
            if (impl->read_stalled && (c->input_size < PN_CONNECTOR_IO_BUF_SIZE)) {
                impl->read_stalled = 0;
                c->pending_read = true;
                ready += 1;
            }
        }
        c = c->connector_next;
    }
    return ready;
}


int pn_listener_init_ssl_client( pn_listener_t *l, pn_connector_t *c)
{
    if (!l->ssl) return 0;
    assert(!c->ssl);

    c->ssl = calloc(1, sizeof(pn_connector_ssl_t));
    if (!c->ssl) {
        perror("calloc()");
        return -1;
    }
    pn_connector_ssl_t *impl = c->ssl;

    impl->mode = SSL_SERVER;
    impl->ctx = NULL;    // share the acceptor's context

    /* make the socket nonblocking*/
    int flags = fcntl(c->fd, F_GETFL, 0);

    flags |= O_NONBLOCK;
    if (fcntl(c->fd, F_SETFL, flags)) {
        // @todo: better error handling - fail this connection
        perror("fcntl");
        return -1;
    }

    if (l->ssl->allow_unsecured) {
        return start_check_for_ssl(c);
    } else {
        return start_ssl_accept(c);
    }
}

void pn_connector_shutdown_ssl( pn_connector_t *c)
{
    if (c->ssl) {
        if (start_ssl_shutdown(c) == 0)
            return;  // shutting down
    }
    // if no ssl or shutdown fails, just close the underlying socket
    pn_connector_close(c);
}

void pn_listener_free_ssl( pn_listener_t *l )
{
    if (l->ssl) {
        pn_listener_ssl_t *impl = l->ssl;
        // note: ctx is referenced counted - will not actually free until all child SSL
        // connections are freed.
        if (impl->ctx) SSL_CTX_free(impl->ctx);
        if (impl->keyfile_pw) {
            memset( impl->keyfile_pw, 0, strlen( impl->keyfile_pw ) );
            free( impl->keyfile_pw );
        }
        free(l->ssl);
        l->ssl = NULL;
    }
}


void pn_connector_free_ssl( pn_connector_t *c )
{
    if (c->ssl) {
        pn_connector_ssl_t *impl = c->ssl;
        if (impl->ssl) SSL_free(impl->ssl);
        if (impl->sbio) BIO_free(impl->sbio);
        if (impl->ctx) SSL_CTX_free(impl->ctx);
        if (impl->keyfile_pw) {
            memset( impl->keyfile_pw, 0, strlen( impl->keyfile_pw ) );
            free( impl->keyfile_pw );
        }
        free(c->ssl);
        c->ssl = NULL;
    }
}


/** Private: */

static int keyfile_pw_cb(char *buf, int size, int rwflag, void *userdata)
{
    strncpy(buf, (char *)userdata, size);   // @todo: un-obfuscate me!!!
    buf[size - 1] = '\0';
    return(strlen(buf));
}


// Configure the database containing trusted CA certificates
static int configure_ca_database(SSL_CTX *ctx, const char *certificate_db)
{
    // certificates can be either a file or a directory, which determines how it is passed
    // to SSL_CTX_load_verify_locations()
    struct stat sbuf;
    if (stat( certificate_db, &sbuf ) != 0) {
        fprintf(stderr, "stat(%s) failed: %s\n", certificate_db, strerror(errno));
        return -1;
    }
    const char *file;
    const char *dir;
    if (S_ISDIR(sbuf.st_mode)) {
        dir = certificate_db;
        file = NULL;
    } else {
        dir = NULL;
        file = certificate_db;
    }

    fprintf(stderr, "load verify locations: file=%s dir=%s\n", file, dir);
    if (SSL_CTX_load_verify_locations( ctx, file, dir ) != 1) {
        fprintf(stderr, "SSL_CTX_load_verify_locations( %s ) failed\n", certificate_db);
        return -1;
    }

    return 0;
}





/* STATE: check_for_ssl
 *
 * The server will allow both encrypted and non-encrypted clients.  Determine if
 * encryption is being used by peeking at the first few bytes of incoming data.
 */

static int start_check_for_ssl( pn_connector_t *client )
{
    printf("start_check_for_ssl()\n");
    client->status |= PN_SEL_RD;
    client->io_handler = handle_check_for_ssl;
    return 0;
}


static int handle_check_for_ssl( pn_connector_t *client )
{
    unsigned char buf[5];
    int rc;
    int retries = 3;

    printf("handle_check_for_ssl()\n");

    do {
        rc = recv(client->fd, buf, sizeof(buf), MSG_PEEK);
        if (rc == sizeof(buf))
            break;
        if (rc < 0) {
            if (errno == EWOULDBLOCK) {
                client->status |= PN_SEL_RD;
                return 0;
            } else if (errno != EINTR) {
                perror("handle_check_for_ssl() recv failed:");
                break;
            }
        }
    } while (retries-- > 0);

    if (rc == sizeof(buf)) {
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
            if (client->driver->trace & PN_TRACE_DRV) fprintf(stderr, "Connector %s using encryption.\n", client->name);
            return start_ssl_accept( client );
        }
    }
    if (client->driver->trace & PN_TRACE_DRV) fprintf(stderr, "Connector %s not using encryption.\n", client->name);
    return start_clear_connected( client );
}


/* STATE: ssl_connect
 *
 * Start the SSL connection to the server.  This will require I/O to complete the
 * handshake.
 */
static int start_ssl_connect(pn_connector_t *client)
{
    fprintf(stderr, "start_ssl_connect()\n");
    pn_connector_ssl_t *impl = client->ssl;
    if (!impl) return -1;

    impl->ssl = SSL_new(impl->ctx);
    impl->sbio = BIO_new_socket( client->fd, BIO_NOCLOSE );
    SSL_set_bio(impl->ssl, impl->sbio, impl->sbio);
#if 0
    // @todo reconnect support
    if (conn->reconnecting && conn->session) {
        printf("Resuming old session...\n");
        SSL_set_session(conn->ssl, conn->session);
        SSL_SESSION_free(conn->session);
        conn->session = NULL;
    }
#endif
    return handle_ssl_connect(client);
}


int handle_ssl_connect( pn_connector_t *client )
{
    fprintf(stderr, "handle_ssl_connect()\n");
    pn_connector_ssl_t *impl = client->ssl;
    if (!impl) return -1;

    int rc = SSL_connect( impl->ssl );
    switch (SSL_get_error( impl->ssl, rc )) {
    case SSL_ERROR_NONE:
        // connection completed
        return start_ssl_connection_up( client );

    case SSL_ERROR_WANT_READ:
        //printf("  need read...\n");
        client->status |= PN_SEL_RD;
        break;
    case SSL_ERROR_WANT_WRITE:
        printf("  need write...\n");
        client->status |= PN_SEL_WR;
        break;
    default:
        fprintf(stderr, "SSL_connect() failure: %d\n", SSL_get_error(impl->ssl, rc));
        ERR_print_errors_fp(stderr);
        return -1;
    }

    client->io_handler = handle_ssl_connect;
    return 0;
}



/* STATE: ssl_accept
 *
 * Accept the client connection.  This may require I/O to complete the handshake.
 */

static int start_ssl_accept(pn_connector_t *client)
{
    printf("start_ssl_accept()\n");
    pn_connector_ssl_t *impl = client->ssl;
    if (!impl) return -1;
    pn_listener_ssl_t *parent = client->listener->ssl;
    if (!parent) return -1;

    impl->sbio = BIO_new_socket(client->fd, BIO_NOCLOSE);
    impl->ssl = SSL_new(parent->ctx);
    SSL_set_bio(impl->ssl, impl->sbio, impl->sbio);

    return handle_ssl_accept(client);
}

static int handle_ssl_accept(pn_connector_t *client)
{
    printf("handle_ssl_accept()\n");
    pn_connector_ssl_t *impl = client->ssl;
    if (!impl) return -1;

    int rc = SSL_accept(impl->ssl);
    switch (SSL_get_error(impl->ssl, rc)) {
    case SSL_ERROR_NONE:
        // connection completed
        return start_ssl_connection_up( client );

    case SSL_ERROR_WANT_READ:
        //printf("  need read...\n");
        client->status |= PN_SEL_RD;
        break;
    case SSL_ERROR_WANT_WRITE:
        printf("  need write...\n");
        client->status |= PN_SEL_WR;
        break;
    default:
        fprintf(stderr, "SSL_accept() failure: %d\n", SSL_get_error(impl->ssl, rc));
        ERR_print_errors_fp(stderr);
        return -1;
    }
    client->io_handler = handle_ssl_accept;
    return 0;
}


/* STATE: ssl_connection_up
 *
 * The SSL connection is up and data is passing!
 */
int start_ssl_connection_up( pn_connector_t *c )
{
    printf("ssl_connection_up\n");
    c->io_handler = handle_ssl_connection_up;
    c->status |= PN_SEL_RD;
    return 0;
}


int handle_ssl_connection_up( pn_connector_t *c )
{
    int rc;
    int ssl_err;
    int read_blocked = 0;
    int write_blocked = 0;
    int need_read = 0;
    int need_write = 0;
    int input_space;
    pn_connector_ssl_t *impl = c->ssl;
    assert(impl);

    printf("handle_ssl_connection_up  OUT=%d\n", (int)c->output_size);

    c->status &= ~(PN_SEL_RD | PN_SEL_WR);

    do {
        input_space = PN_CONNECTOR_IO_BUF_SIZE - c->input_size;
        if (!read_blocked && input_space > 0) {
            rc = SSL_read(impl->ssl, &c->input[c->input_size], input_space);
            printf("read %d possible bytes: actual = %d\n", input_space, rc);
            ssl_err = SSL_get_error( impl->ssl, rc );
            if (ssl_err == SSL_ERROR_NONE) {
                c->input_size += rc;
                printf("... read bytes now =%d\n", (int)c->input_size);
            } else if (ssl_err == SSL_ERROR_WANT_READ) {
                // socket read blocked
                printf("need read\n");
                read_blocked = 1;
                need_read = 1;
            } else if (ssl_err == SSL_ERROR_WANT_WRITE) {
                printf("need write\n");
                read_blocked = 1;
                write_blocked = 1;  // don't bother writing
                need_write = 1;
            } else {
                if (ssl_err == SSL_ERROR_ZERO_RETURN) {
                    /* remote shutdown */
                    printf("zero return (read)\n");
                } else {
                    perror("Unexpected SSL read failure, errno:");
                    printf("- SSL read status: %d\n", ssl_err);
                }
                return start_ssl_shutdown(c);
            }
        }

        pn_connector_process_input(c);
        pn_connector_process_output(c);

        if (!write_blocked && c->output_size > 0) {

            rc = SSL_write( impl->ssl, c->output, c->output_size );
            printf("write %d possible bytes: actual = %d\n", (int)c->output_size, rc);
            ssl_err = SSL_get_error( impl->ssl, rc );
            if (ssl_err == SSL_ERROR_NONE) {
                c->output_size -= rc;
                if (c->output_size > 0)
                    memmove(c->output, c->output + rc, c->output_size);
            } else if (ssl_err == SSL_ERROR_WANT_WRITE) {
                /* We would have blocked */
                printf("need write\n");
                write_blocked = 1;
                need_write = 1;
            } else if (ssl_err == SSL_ERROR_WANT_READ) {
                printf("need read\n");
                read_blocked = 1;
                write_blocked = 1;  // don't bother reading
                need_read = 1;
            } else {
                if (ssl_err == SSL_ERROR_ZERO_RETURN)
                    printf("zero return (write)\n");
                else {
                    perror("Unexpected SSL_write failure, errno:");
                    printf("- SSL write status: %d\n", ssl_err);
                }
                return start_ssl_shutdown(c);
            }
        }

    } while ((!read_blocked && c->input_size < PN_CONNECTOR_IO_BUF_SIZE) ||
             (!write_blocked && c->output_size));

    if (need_read) c->status |= PN_SEL_RD;
    if (need_write) c->status |= PN_SEL_WR;

    // we need to re-call SSL_read when the receive buffer is drained (do not need to wait for I/O!)
    if (!read_blocked && SSL_pending(impl->ssl) && PN_CONNECTOR_IO_BUF_SIZE == c->input_size) {
        printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!  >>>>> READ STALLED <<<<<< !!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        impl->read_stalled = true;
    }

    return 0;
}


/* STATE: clear_connected
 *
 * The peer has connected without SSL (in the clear).  Use the default I/O handler
 */
static int start_clear_connected( pn_connector_t *c )
{
    printf("start_clear_connected()\n");
    pn_connector_free_ssl( c );
    c->status |= (PN_SEL_RD | PN_SEL_WR);
    c->io_handler = pn_io_handler;
    return 0;
}


/* STATE: ssl_shutdown
 *
 * Shutting down the SSL connection.
 */
static int start_ssl_shutdown( pn_connector_t *c )
{
    printf("start_ssl_shutdown()\n");
    if (c->closed) return 0;
    return handle_ssl_shutdown( c );
}

static int handle_ssl_shutdown( pn_connector_t *c )
{
    int rc;
    pn_connector_ssl_t *impl = c->ssl;
    if (!impl) return -1;

    printf("handle_ssl_shutdown()\n");

    do {
        rc = SSL_shutdown( impl->ssl );
    } while (rc == 0);

    switch (SSL_get_error( impl->ssl, rc )) {
    case SSL_ERROR_WANT_READ:
        //printf("  need read...\n");
        c->status |= PN_SEL_RD;
        break;
    case SSL_ERROR_WANT_WRITE:
        printf("  need write...\n");
        c->status |= PN_SEL_WR;
        break;
    default: // whatever- consider us closed
    case SSL_ERROR_NONE:
        printf("  shutdown code=%d\n", SSL_get_error(impl->ssl,rc));
        // shutdown completed
        c->io_handler = pn_null_io_handler;
        pn_connector_close( c );
        return 0;
    }
    c->io_handler = handle_ssl_shutdown;
    return 0;
}




