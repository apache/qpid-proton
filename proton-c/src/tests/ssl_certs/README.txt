This directory contains basic self signed test certificates for use by
proton examples.

The ".pem" files are in the format expected by proton implementations
using OpenSSL.  The ".p12" file are for Windows implementations using
SChannel.

The commands used to generate the certificates follow.


make_pn_cert()
{
  name=$1
  subject=$2
  passwd=$3
  # create the pem files
  openssl req -newkey rsa:2048 -keyout $name-private-key.pem -out $name-certificate.pem -subj $subject -passout pass:$passwd -x509 -days 3650
  # create the p12 files
  openssl pkcs12 -export -out $name-full.p12 -passin pass:$passwd -passout pass:$passwd -inkey $name-private-key.pem -in $name-certificate.pem -name $name
  openssl pkcs12 -export -out $name-certificate.p12 -in $name-certificate.pem -name $name -nokeys -passout pass:
}

make_pn_cert tserver /CN=test_server/OU=proton_test tserverpw
make_pn_cert tclient /CN=test_client/OU=proton_test tclientpw
