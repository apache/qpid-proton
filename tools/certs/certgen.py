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

"""Certificate generation helpers built on trustme.

Provides a simplified interface for generating test certificates using the
trustme library (MIT/Apache 2.0 licensed). Adds PKCS12 export functionality
that trustme does not provide directly.
"""
from __future__ import annotations

from pathlib import Path
from typing import List, Optional

import trustme
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.serialization import pkcs12
from cryptography.x509 import load_pem_x509_certificate


class CertificateAuthority:
    """A certificate authority that can issue certificates."""

    def __init__(self, organization_name: str = "Trust Me Inc."):
        self._ca = trustme.CA(
            organization_name=organization_name,
            key_type=trustme.KeyType.RSA,
        )

    @property
    def cert_pem(self) -> bytes:
        """The CA certificate in PEM format."""
        return self._ca.cert_pem.bytes()

    @property
    def private_key_pem(self) -> bytes:
        """The CA private key in PEM format (unencrypted)."""
        return self._ca.private_key_pem.bytes()

    def write_cert_pem(self, path: Path) -> None:
        """Write the CA certificate to a PEM file."""
        self._ca.cert_pem.write_to_path(path)

    def issue_cert(
        self,
        common_name: Optional[str] = None,
        san_dns: Optional[List[str]] = None,
    ) -> Certificate:
        """Issue a certificate signed by this CA.

        Args:
            common_name: The common name for the certificate.
            san_dns: Optional list of DNS names for Subject Alternative Name.
                     Note: DNS names must be valid (no underscores).

        Returns:
            A Certificate object.
        """
        identities = san_dns or []
        leaf = self._ca.issue_cert(
            *identities,
            common_name=common_name,
            key_type=trustme.KeyType.RSA,
        )
        return Certificate(leaf, self._ca.cert_pem.bytes())


class Certificate:
    """An issued certificate with its private key."""

    def __init__(self, leaf: trustme.LeafCert, ca_cert_pem: bytes):
        self._leaf = leaf
        self._ca_cert_pem = ca_cert_pem

    @property
    def cert_pem(self) -> bytes:
        """The certificate in PEM format."""
        return self._leaf.cert_chain_pems[0].bytes()

    @property
    def private_key_pem(self) -> bytes:
        """The private key in PEM format (unencrypted)."""
        return self._leaf.private_key_pem.bytes()

    def write_cert_pem(self, path: Path) -> None:
        """Write the certificate to a PEM file."""
        self._leaf.cert_chain_pems[0].write_to_path(path)

    def write_private_key_pem(
        self,
        path: Path,
        password: Optional[bytes] = None,
    ) -> None:
        """Write the private key to a PEM file.

        Args:
            path: Destination file path.
            password: Optional password to encrypt the key.
        """
        if password:
            # Load the unencrypted key and re-serialize with encryption
            key = serialization.load_pem_private_key(
                self._leaf.private_key_pem.bytes(),
                password=None,
            )
            enc = serialization.BestAvailableEncryption(password)
            data = key.private_bytes(
                encoding=serialization.Encoding.PEM,
                format=serialization.PrivateFormat.TraditionalOpenSSL,
                encryption_algorithm=enc,
            )
            path.write_bytes(data)
        else:
            self._leaf.private_key_pem.write_to_path(path)

    def write_pkcs12(
        self,
        path: Path,
        name: str,
        password: Optional[bytes] = None,
        include_key: bool = True,
        include_ca: bool = False,
    ) -> None:
        """Write the certificate (and optionally key/CA) to a PKCS12 file.

        Args:
            path: Destination file path (.p12).
            name: Friendly name for the certificate.
            password: Optional password to encrypt the bundle.
            include_key: If True, include the private key.
            include_ca: If True, include the CA certificate.
        """
        cert = load_pem_x509_certificate(self.cert_pem)

        key = None
        if include_key:
            key = serialization.load_pem_private_key(
                self._leaf.private_key_pem.bytes(),
                password=None,
            )

        cas = None
        if include_ca:
            cas = [load_pem_x509_certificate(self._ca_cert_pem)]

        enc = (
            serialization.NoEncryption()
            if password is None
            else serialization.BestAvailableEncryption(password)
        )

        p12_data = pkcs12.serialize_key_and_certificates(
            name.encode() if name else None,
            key,
            cert,
            cas,
            enc,
        )
        path.write_bytes(p12_data)
