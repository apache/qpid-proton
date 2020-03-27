The following certificate files are used by the SSL unit tests (ssl.py):

ca-certificate.pem - contains the public certificate identifying a "trusted" Certificate
Authority.  This certificate is used to sign the certificates that identify the SSL
servers and clients run by the tests.

client-certificate.pem - the public certificate used to identify the client.  Signed by
the CA.

client-private-key.pem - encrypted key used to create client-certificate.pem.  Password is
"client-password"

server-certificate.pem - the public certificate used to identify the server.  Signed by
the CA.  The CommonName is "A1.Good.Server.domain.com", and is checked by some unit tests.

server-private-key.pem - encrypted key used to create server-certificate.pem. Password is
"server-password"

bad-server-certificate.pem, bad-server-private-key.pem - a certificate/key that is not trusted by the client, for negative test.

server-wc-certificate.pem and server-wc-private-key.pem - similar to
server-certificate.pem and server-private-key.pem, but contains Subject Alternate Name
entries, and a wildcard CommonName.  Used for certificate name checking tests.

These certificates have been created using the OpenSSL tool.

The mkcerts.sh script in this directory can be used to create these certificates (requires keytool from Java 1.7, and openssl):
