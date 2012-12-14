The following certificate files are used by the SSL unit tests (ssl.py):

ca-certificate.pem - contains the public certificate identifying a "trusted" Certificate
Authority.  This certificate is used to sign the certificates that identify the SSL
servers and clients run by the tests.

client-certificate.pem - the public certificate used to identify the client.  Signed by
the CA.

client-private-key.pem - encrypted key used to create client-certificate.pem.  Password is
"client-password"

server-certificate.pem - the public certificate used to identify the server.  Signed by
the CA.

server-private-key.pem - encrypted key used to create server-certificate.pem. Password is
"server-password"

bad-server-certificate.pem, bad-server-private-key.pem - a certificate/key that is not trusted by the client, for negative test.

These certificates have been created using the OpenSSL tool.

The following commands were used to create these certificates (requires keytool from Java 1.7):

# Create a self-signed certificate for the CA, and a private key to sign certificate requests:
 keytool -storetype pkcs12 -keystore ca.pkcs12 -storepass ca-password -alias ca -keypass ca-password -genkey -dname "O=Trust Me Inc.,CN=127.0.0.1"
 openssl pkcs12 -nokeys -passin pass:ca-password -in ca.pkcs12 -passout pass:ca-password -out ca-certificate.pem

# Create a certificate request for the server certificate.  Use the CA's certificate to sign it:
 keytool -storetype pkcs12 -keystore server.pkcs12 -storepass server-password -alias server-certificate -keypass server-password -genkey  -dname "O=Server,CN=127.0.0.1"
 keytool -storetype pkcs12 -keystore server.pkcs12 -storepass server-password -alias server-certificate -keypass server-password -certreq -file server-request.pem
 keytool -storetype pkcs12 -keystore ca.pkcs12 -storepass ca-password -alias ca -keypass ca-password -gencert -rfc -validity 99999 -infile server-request.pem -outfile server-certificate.pem
 openssl pkcs12 -nocerts -passin pass:server-password -in server.pkcs12 -passout pass:server-password -out server-private-key.pem

# Create a certificate request for the client certificate.  Use the CA's certificate to sign it:
 keytool -storetype pkcs12 -keystore client.pkcs12 -storepass client-password -alias client-certificate -keypass client-password -genkey  -dname "O=Client,CN=127.0.0.1"
 keytool -storetype pkcs12 -keystore client.pkcs12 -storepass client-password -alias client-certificate -keypass client-password -certreq -file client-request.pem
 keytool -storetype pkcs12 -keystore ca.pkcs12 -storepass ca-password -alias ca -keypass ca-password -gencert -rfc -validity 99999 -infile client-request.pem -outfile client-certificate.pem
 openssl pkcs12 -nocerts -passin pass:client-password -in client.pkcs12 -passout pass:client-password -out client-private-key.pem

# Create a "bad" certificate - not signed by a trusted authority
 keytool -storetype pkcs12 -keystore bad-server.pkcs12 -storepass server-password -alias bad-server -keypass server-password -genkey -dname "O=Not Trusted Inc,CN=127.0.0.1"
 openssl pkcs12 -nocerts -passin pass:server-password -in bad-server.pkcs12 -passout pass:server-password -out bad-server-private-key.pem
 openssl pkcs12 -nokeys  -passin pass:server-password -in bad-server.pkcs12 -passout pass:server-password -out bad-server-certificate.pem

