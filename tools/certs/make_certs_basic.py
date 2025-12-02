#!/usr/bin/env python3
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

"""Generate small 'tserver/tclient' cert bundle for testing.

Usage: make_certs_basic.py [OUT_DIR]

Generates certificates using the trustme library (MIT/Apache 2.0 licensed).
"""
from __future__ import annotations

import sys
from pathlib import Path

from certgen import CertificateAuthority


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else Path(".")
    out.mkdir(parents=True, exist_ok=True)

    # Create CA
    ca = CertificateAuthority(organization_name="Small.CA.com")
    ca.write_cert_pem(out / "ca-certificate.pem")

    # Server certificate - use common_name only (SANs require valid DNS names)
    # The C tests verify against "test_server" which has underscore (invalid DNS char)
    server = ca.issue_cert(common_name="test_server")
    server.write_cert_pem(out / "tserver-certificate.pem")
    server.write_private_key_pem(out / "tserver-private-key.pem", password=b"tserverpw")
    server.write_pkcs12(out / "tserver-certificate.p12", "tserver", include_key=False, include_ca=True)
    server.write_pkcs12(out / "tserver-full.p12", "tserver", password=b"tserverpw", include_key=True)

    # Client certificate
    client = ca.issue_cert(common_name="test_client")
    client.write_cert_pem(out / "tclient-certificate.pem")
    client.write_private_key_pem(out / "tclient-private-key.pem", password=b"tclientpw")
    client.write_pkcs12(out / "tclient-certificate.p12", "tclient", include_key=False, include_ca=True)
    client.write_pkcs12(out / "tclient-full.p12", "tclient", password=b"tclientpw", include_key=True)

    print("Basic certificate bundle generated in:", str(out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
