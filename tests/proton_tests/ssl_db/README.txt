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


These certificates have been created using the OpenSSL tool.

The following commands were used to create these certificates:

# Create a self-signed certificate for the CA, and a private key to sign certificate requests:
 openssl req -x509 -newkey rsa:2048 -keyout ca-private-key.pem -passout pass:ca-password -out ca-certificate.pem  -days 99999 -subj "/O=Trust Me, Inc/CN=127.0.0.1"

# Create a certificate request for the server certificate.  Use the CA's certificate to sign it:
 openssl req -newkey rsa:2048 -keyout server-private-key.pem -passout pass:server-password -out server-request.pem -subj "/O=Server/CN=127.0.0.1"
 openssl x509 -req -in server-request.pem -CA ca-certificate.pem -CAkey ca-private-key.pem -CAcreateserial -passin pass:ca-password -days 99999 -out server-certificate.pem

# Create a certificate request for the client certificate.  Use the CA's certificate to sign it:
 openssl req -newkey rsa:2048 -keyout client-private-key.pem -passout pass:client-password -out client-request.pem -subj "/O=Client/CN=127.0.0.1"
 openssl x509 -req -in client-request.pem -CA ca-certificate.pem -CAkey ca-private-key.pem -CAcreateserial -passin pass:ca-password -days 99999 -out client-certificate.pem


