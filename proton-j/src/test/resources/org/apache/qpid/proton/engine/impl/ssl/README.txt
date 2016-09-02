# These resources are used during the unittesting of the SSL capabilities
#
#  cert.pem.txt              - A public certificate
#  key.pem.txt               - A passphrase protected private key
#  private-key-clear.pem.txt - An unprotected private key

# Files have a .txt suffix to prevent undesired handling by other tooling (IDEs etc)
# and can easilly be re-created using the following openssl commands or you can
# execute this file directly with 
#             sh README.txt

## Clean Up

echo
echo Clean Up
echo

rm *.pem.txt

# 1. Generate a certificate and protected private key

echo
echo when prompted use 'unittest' as the passphase, all other fields can be random values
echo

openssl req -x509 -newkey rsa:2048 -keyout key.pem.txt -out cert.pem.txt -days 10000
 
# 2. The following command produces an unprotected private key

echo
echo when prompted, use 'unittest' as the passphrase
echo

openssl rsa -in key.pem.txt -out private-key-clear.pem.txt -outform PEM

echo

# 3. The following command produces an unprotected PKCS#8 private key

echo
echo when prompted, use 'unittest' as the passphrase
echo

openssl pkcs8 -topk8 -nocrypt -in key.pem.txt -out private-key-clear-pkcs8.pem.txt

echo
