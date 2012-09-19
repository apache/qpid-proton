#/bin/bash
#
#
set -x

# Step 0: create a password file

echo "trustno1" > ./password.txt

# Step 1: Create a self-signed certificate that identifies the CA

openssl req -x509 -newkey rsa:2048 -keyout ca-private-key.pem -passout file:./password.txt -out ca-certificate.pem  -days 999 -subj "/O=Trust Me, Inc/CN=127.0.0.1"

# Step 2: Create a certificate signing request for the server

openssl req -newkey rsa:2048 -keyout server-private-key.pem -passout file:./password.txt -out server-request.pem -subj "/O=Soft Serve/CN=127.0.0.1"

# Step 3: Use the "CA" to create a certificate for the server from the request:

openssl x509 -req -in server-request.pem -CA ca-certificate.pem -CAkey ca-private-key.pem -CAcreateserial -passin file:./password.txt -days 999 -out server-certificate.pem

# Step 4: create a certificate database to hold the "trusted" CA certificate

mkdir ./trusted_db
rm -f ./trusted_db/*
mv ca-certificate.pem ./trusted_db
c_rehash ./trusted_db



