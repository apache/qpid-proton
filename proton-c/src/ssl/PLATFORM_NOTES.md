Proton SSL/TLS implementations have platform dependent formats for specifying
private and public key information.

OpenSSL
=======

On OpenSSL (POSIX) based systems, certificates and their private keys are
specified separately in two files: the public X509 certificate in PEM format
and the password protected PKCS#8 encoded private key.

  `pn_ssl_domain_set_credentials(path_to_public_x509.pem,  
                path_to_private_pkcs8.pem, password_for_pkcs8)`


A database of trusted Certificate Authority certificates may be specified as a
path to a file or a directory.  In the former case, the file consists of one
or more X509 certificates in PEM format concatenated together.  In the latter
case, the directory contains a file for each X509 certificate in PEM format
and indexed by (i.e. the file name is derived from) the X509 `-subject_hash`
of the certificate's name.  See
[here](https://www.openssl.org/docs/ssl/SSL_CTX_load_verify_locations.htm)
for more details.


SChannel
========

On SChannel (Windows) based systems, trust and identity certificates are
stored in certificate stores, which may be file based or system/registry
based.  The former are in PKCS#12 format and the latter are typically managed
by the Microsoft graphical management console.  The public and private keys
are stored together, except in the case of trusted authority certificates
which only contain the public key information.

To specify a certificate:

  `pn_ssl_domain_set_credentials(store, certificate_friendly_name,  
                 password_for_store)`

File based stores are specified by their relative or absolute path names.
Registry stores are specified by their names (which are case insensitive)
preceded by "ss:" for "Current User" system stores or "lmss:" for "Local
Machine" system stores.  Examples:

  "ss:Personal" specifies the Personal store for the Current User.

  "lmss:AMQP" specifies a registry store called "AMQP" for the Local Machine
  context.

  "ss:Root" specifies the Trusted Root Certificate Authorities store for the
  Current User.

If a store contains a single certificate, the friendly name is optional.  The
password may be null in the case of a registry store that is not password
protected.

Trusted root certificates must be placed in a store that is not password
protected.

In the special case that the peer certificate chain being verified requires
revocation checking, the trusted root certificate must be present in both the
trust store specified to Proton and also in the Windows "Trusted Root
Certificate Authorities" system store.  Such certificate chains are usually
managed by a central corporate network administrator or by a recognized
certificate authority in which case the trusted root is often already present
in the system store.  This requirement can be worked around by creating a
special purpose CA database for Proton that includes the target peer's
certificate (making it trusted, with the caution that you must consider the
security implications of bypassing the revocation check).

Existing OpenSSL keys (say `xx_x509.pem` and `xx_private_key.pem`) can be
converted to PKCS#12 by the command:

  `openssl pkcs12 -export -out xx_windows.p12 -passin pass:password \  
          -passout pass:password -inkey xx_private_key.pem -in xx_x509.pem \  
          -name xx_friendlyname`

To create a PKCS#12 trust store from a Certificate Authority's public X509
certificate with an empty password:

  `openssl pkcs12 -export -out trust_store.p12 -in ca-certificate.pem \  
          -name ca-certificate -nokeys -passout pass:`
