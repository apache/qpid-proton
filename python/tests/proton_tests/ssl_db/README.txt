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

The following bash script can be used to create these certificates (requires keytool from Java 1.7, and openssl):

--8<--
#!/bin/bash
#set -x

rm -f *.pem *.pkcs12

# Create a self-signed certificate for the CA, and a private key to sign certificate requests:
keytool -storetype pkcs12 -keystore ca.pkcs12 -storepass ca-password -alias ca -keypass ca-password -genkey -dname "O=Trust Me Inc.,CN=Trusted.CA.com" -validity 99999 -ext bc:c=ca:true,pathlen:0 -ext ku:c=digitalSignature,keyCertSign -ext ExtendedkeyUsage=serverAuth,clientAuth
openssl pkcs12 -nokeys -passin pass:ca-password -in ca.pkcs12 -passout pass:ca-password -out ca-certificate.pem

# Create a certificate request for the server certificate.  Use the CA's certificate to sign it:
keytool -storetype pkcs12 -keystore server.pkcs12 -storepass server-password -alias server-certificate -keypass server-password -genkey  -dname "O=Server,CN=A1.Good.Server.domain.com" -validity 99999
keytool -storetype pkcs12 -keystore server.pkcs12 -storepass server-password -alias server-certificate -keypass server-password -certreq -file server-request.pem
keytool -storetype pkcs12 -keystore ca.pkcs12 -storepass ca-password -alias ca -keypass ca-password -gencert -rfc -validity 99999 -infile server-request.pem -outfile server-certificate.pem
openssl pkcs12 -nocerts -passin pass:server-password -in server.pkcs12 -passout pass:server-password -out server-private-key.pem

# Create a certificate request for a server certificate using localhost.  Use the CA's certificate to sign it:
keytool -storetype pkcs12 -keystore server-lh.pkcs12 -storepass server-password -alias server-certificate -keypass server-password -genkey  -dname "CN=localhost" -validity 99999
keytool -storetype pkcs12 -keystore server-lh.pkcs12 -storepass server-password -alias server-certificate -keypass server-password -certreq -file server-request-lh.pem
keytool -storetype pkcs12 -keystore ca.pkcs12 -storepass ca-password -alias ca -keypass ca-password -gencert -rfc -validity 99999 -infile server-request-lh.pem -outfile server-certificate-lh.pem
openssl pkcs12 -nocerts -passin pass:server-password -in server-lh.pkcs12 -passout pass:server-password -out server-private-key-lh.pem

# Create a certificate request for the client certificate.  Use the CA's certificate to sign it:
keytool -storetype pkcs12 -keystore client.pkcs12 -storepass client-password -alias client-certificate -keypass client-password -genkey  -dname "O=Client,CN=127.0.0.1" -validity 99999
keytool -storetype pkcs12 -keystore client.pkcs12 -storepass client-password -alias client-certificate -keypass client-password -certreq -file client-request.pem
keytool -storetype pkcs12 -keystore ca.pkcs12 -storepass ca-password -alias ca -keypass ca-password -gencert -rfc -validity 99999 -infile client-request.pem -outfile client-certificate.pem
openssl pkcs12 -nocerts -passin pass:client-password -in client.pkcs12 -passout pass:client-password -out client-private-key.pem
openssl pkcs12 -nocerts -passin pass:client-password -in client.pkcs12 -nodes -out client-private-key-no-password.pem

# Create another client certificate with a different subject line
keytool -storetype pkcs12 -keystore client1.pkcs12 -storepass client-password -alias client-certificate1 -keypass client-password -genkey  -dname "O=Client,CN=127.0.0.1,C=US,ST=ST,L=City,OU=Dev" -validity 99999
keytool -storetype pkcs12 -keystore client1.pkcs12 -storepass client-password -alias client-certificate1 -keypass client-password -certreq -file client-request1.pem
keytool -storetype pkcs12 -keystore ca.pkcs12 -storepass ca-password -alias ca -keypass ca-password -gencert -rfc -validity 99999 -infile client-request1.pem -outfile client-certificate1.pem
openssl pkcs12 -nocerts -passin pass:client-password -in client1.pkcs12 -passout pass:client-password -out client-private-key1.pem

# Create a "bad" certificate - not signed by a trusted authority
keytool -storetype pkcs12 -keystore bad-server.pkcs12 -storepass server-password -alias bad-server -keypass server-password -genkey -dname "O=Not Trusted Inc,CN=127.0.0.1" -validity 99999
openssl pkcs12 -nocerts -passin pass:server-password -in bad-server.pkcs12 -passout pass:server-password -out bad-server-private-key.pem
openssl pkcs12 -nokeys  -passin pass:server-password -in bad-server.pkcs12 -passout pass:server-password -out bad-server-certificate.pem

# Create a server certificate with several alternate names, including a wildcarded common name:
keytool -ext san=dns:alternate.name.one.com,dns:another.name.com -storetype pkcs12 -keystore server-wc.pkcs12 -storepass server-password -alias server-wc-certificate -keypass server-password -genkeypair -dname "O=Server,CN=*.prefix*.domain.com" -validity 99999
keytool -ext san=dns:alternate.name.one.com,dns:another.name.com -storetype pkcs12 -keystore server-wc.pkcs12 -storepass server-password -alias server-wc-certificate -keypass server-password -certreq -file server-wc-request.pem
keytool -ext san=dns:alternate.name.one.com,dns:another.name.com  -storetype pkcs12 -keystore ca.pkcs12 -storepass ca-password -alias ca -keypass ca-password -gencert -rfc -validity 99999 -infile server-wc-request.pem -outfile server-wc-certificate.pem
openssl pkcs12 -nocerts -passin pass:server-password -in server-wc.pkcs12 -passout pass:server-password -out server-wc-private-key.pem

# Create pkcs12 versions of the above certificates (for Windows SChannel)
# The CA certificate store/DB is created without public keys.
# Give the "p12" files the same base name so the tests can just change the extension to switch between platforms.
# These certificates might work for OpenSSL <-> SChannel interop tests, but note that the DH cypher suite
# overlap is poor between platforms especially for older Windows versions.  RSA certificates are better for
# interop (or PFS-friendly certificates on newer platforms).
openssl pkcs12 -export -out ca-certificate.p12 -in ca-certificate.pem -name ca-certificate -nokeys -passout pass:
openssl pkcs12 -export -out server-certificate.p12 -passin pass:server-password -passout pass:server-password -inkey server-private-key.pem -in server-certificate.pem -name server-certificate
openssl pkcs12 -export -out client-certificate.p12 -passin pass:client-password -passout pass:client-password -inkey client-private-key.pem -in client-certificate.pem -name client-certificate
openssl pkcs12 -export -out client-certificate1.p12 -passin pass:client-password -passout pass:client-password -inkey client-private-key1.pem -in client-certificate1.pem -name client-certificate1
openssl pkcs12 -export -out bad-server-certificate.p12 -passin pass:server-password -passout pass:server-password -inkey bad-server-private-key.pem -in bad-server-certificate.pem -name bad-server
openssl pkcs12 -export -out server-wc-certificate.p12 -passin pass:server-password -passout pass:server-password -inkey server-wc-private-key.pem -in server-wc-certificate.pem -name server-wc-certificate
