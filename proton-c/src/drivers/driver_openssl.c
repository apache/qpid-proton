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

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/types.h>
#include <sys/stat.h>


/** @file
 * SSL/TLS support API.
 *
 * This file contains stub implementations of the SSL/TLS API.  This implementation is
 * used if there is no SSL/TLS support in the system's environment.
 */


struct pn_listener_ssl_impl_t {
    SSL_CTX *ctx;
    bool allow_unsecured;
    bool ca_db;
    char *keyfile_pw;
};
typedef struct pn_listener_ssl_impl_t pn_listener_ssl_impl_t;


struct pn_connector_ssl_impl_t {

    enum { SSL_CLIENT, SSL_SERVER } mode;
    SSL_CTX *ctx;
    SSL *ssl;
    BIO *sbio;
    char *keyfile_pw;

    int state;  /// ????
    int need_read;
    int need_write;
    int read_blocked_on_write;
    int write_blocked_on_read;
    int read_stalled;

};
typedef struct pn_connector_ssl_impl_t pn_connector_ssl_impl_t;


static char *copy_str(const char *str)
{
    int len = strlen(str);
    char *s = (char *)malloc(len + 1);
    if (s) strcpy(s, str);
    return s;
}

static int keyfile_pw_cb(char *buf, int size, int rwflag, void *userdata)
{
    strncpy(buf, (char *)userdata, size);   // @todo: un-obfuscate me!!!
    buf[size - 1] = '\0';
    return(strlen(buf));
}


// Configure the database containing trusted CA certificates
static int configure_database(SSL_CTX *ctx, const char *certificate_db)
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

    if (SSL_CTX_load_verify_locations( ctx, file, dir ) != 1) {
        fprintf(stderr, "SSL_CTX_load_verify_locations( %s ) failed\n", certificate_db);
        return -1;
    }

    return 0;
}


int pn_listener_ssl_server_init(pn_listener_t *listener,
                                const char *certificate_file,
                                const char *private_key_file,
                                const char *password,
                                const char *certificate_db)
{
    listener->ssl = calloc(1, sizeof(pn_listener_ssl_impl_t));
    if (!listener->ssl) {
        perror("calloc()");
        return -1;
    }
    pn_listener_ssl_impl_t *impl = listener->ssl;

    SSL_library_init();
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
        impl->keyfile_pw = copy_str(password);  // @todo: obfuscate me!!!
        SSL_CTX_set_default_passwd_cb(impl->ctx, keyfile_pw_cb);
        SSL_CTX_set_default_passwd_cb_userdata(impl->ctx, impl->keyfile_pw);
    }

    if (SSL_CTX_use_PrivateKey_file(impl->ctx, private_key_file, SSL_FILETYPE_PEM) != 1) {
        fprintf(stderr, "SSL_CTX_use_PrivateKey_file( %s ) failed\n", private_key_file);
        return -4;
    }

    int rc = 0;
    if (certificate_db) {
        impl->ca_db = true;
        rc = configure_database(impl->ctx, certificate_db);
    }
    return rc;
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
    connector->ssl = calloc(1, sizeof(pn_connector_ssl_impl_t));
    if (!connector->ssl) {
        perror("calloc()");
        return -1;
    }
    pn_connector_ssl_impl_t *impl = connector->ssl;

    impl->mode = SSL_CLIENT;
    SSL_library_init();
    impl->ctx = SSL_CTX_new(SSLv23_client_method());
    if (!impl->ctx) {
        perror("unable to initialize SSL context");
        return -2;
    }

    if (configure_database(impl->ctx, certificate_db)) {
        return -3;
    }

    /* Force servers to authenticate */
    SSL_CTX_set_verify( connector->ssl->ctx,
                        SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                        0 /*?verify callback?*/ );

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
    SSL_CTX_set_verify_depth(connector->ssl->ctx, 1);
#endif

    return 0;
}


int pn_connector_ssl_set_client_auth(pn_connector_t *connector,
                                     const char *certificate_file,
                                     const char *private_key_file,
                                     const char *password)
{
    // @todo check state to verify not yet connected!

    pn_connector_ssl_impl_t *impl = connector->ssl;

    if (!impl || impl->mode != SSL_CLIENT) {
        fprintf(stderr, "Error: connector not configured as SSL client.\n");
        return -1;
    }

    if (SSL_CTX_use_certificate_chain_file(impl->ctx, certificate_file) != 1) {
        fprintf(stderr, "SSL_CTX_use_certificate_chain_file( %s ) failed\n", certificate_file);
        return -3;
    }

    if (password) {
        impl->keyfile_pw = copy_str(password);  // @todo: obfuscate me!!!
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
    pn_connector_ssl_impl_t *impl = connector->ssl;

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
                    0 /*?verify callback?*/);
    return 0;
}
