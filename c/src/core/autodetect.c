/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#include "autodetect.h"

#define SASL_HEADER ("AMQP\x03\x01\x00\x00")
#define SSL_HEADER  ("AMQP\x02\x01\x00\x00")
#define AMQP_HEADER ("AMQP\x00\x01\x00\x00")

#define SASL_HEADER_LEN 8

/*
 * SSLv2 Client Hello format
 * http://www.mozilla.org/projects/security/pki/nss/ssl/draft02.html
 *
 * Bytes 0-1: RECORD-LENGTH
 * Byte    2: MSG-CLIENT-HELLO (1)
 * Byte    3: CLIENT-VERSION-MSB
 * Byte    4: CLIENT-VERSION-LSB
 *
 * Allowed versions:
 * 2.0 - SSLv2
 * 3.0 - SSLv3
 * 3.1 - TLS 1.0
 * 3.2 - TLS 1.1
 * 3.3 - TLS 1.2
 *
 * The version sent in the Client-Hello is the latest version supported by
 * the client. NSS may send version 3.x in an SSLv2 header for
 * maximum compatibility.
 */
/*
 * SSLv3/TLS Client Hello format
 * RFC 2246
 *
 * Byte    0: ContentType (handshake - 22)
 * Bytes 1-2: ProtocolVersion {major, minor}
 *
 * Allowed versions:
 * 3.0 - SSLv3
 * 3.1 - TLS 1.0
 * 3.2 - TLS 1.1
 * 3.3 - TLS 1.2
 */
/*
 * AMQP 1.0 Header
 *
 * Bytes 0-3: "AMQP"
 * Byte    4: 0==AMQP, 2==SSL, 3==SASL
 * Byte    5: 1
 * Bytes 6-7: 0
 */
/*
 * AMQP Pre 1.0 Header
 *
 * Bytes 0-3: 'AMQP'
 * Byte    4: 1
 * Byte    5: 1
 * Byte    6: 0 (major version)
 * Byte    7: Minor version
 */
pni_protocol_type_t pni_sniff_header(const char *buf, size_t len)
{
  if (len < 3) return PNI_PROTOCOL_INSUFFICIENT;
  bool isSSL3Handshake = buf[0]==22 &&            // handshake
                         buf[1]==3  && buf[2]<=3; // SSL 3.0 & TLS 1.0-1.2 (v3.1-3.3)
  if (isSSL3Handshake) return PNI_PROTOCOL_SSL;

  bool isFirst3AMQP = buf[0]=='A' && buf[1]=='M' && buf[2]=='Q';
  bool isFirst3SSL2CLientHello = buf[2]==1;       // Client Hello
  if (!isFirst3AMQP && !isFirst3SSL2CLientHello) return PNI_PROTOCOL_UNKNOWN;


  if (len < 4) return PNI_PROTOCOL_INSUFFICIENT;
  bool isAMQP = isFirst3AMQP && buf[3]=='P';
  bool isFirst4SSL2ClientHello = isFirst3SSL2CLientHello && (buf[3]==2 || buf[3]==3);
  if (!isAMQP && !isFirst4SSL2ClientHello) return PNI_PROTOCOL_UNKNOWN;

  if (len < 5) return PNI_PROTOCOL_INSUFFICIENT;
  bool isSSL2Handshake = buf[2] == 1 &&   // MSG-CLIENT-HELLO
      ((buf[3] == 3 && buf[4] <= 3) ||    // SSL 3.0 & TLS 1.0-1.2 (v3.1-3.3)
       (buf[3] == 2 && buf[4] == 0));     // SSL 2
  if (isSSL2Handshake) return PNI_PROTOCOL_SSL;

  bool isFirst5OldAMQP = isAMQP && buf[4]==1;
  bool isFirst5AMQP = isAMQP && (buf[4]==0 || buf[4]==2 || buf[4]==3);
  if (!isFirst5AMQP && !isFirst5OldAMQP) return PNI_PROTOCOL_UNKNOWN;

  if (len < 6) return PNI_PROTOCOL_INSUFFICIENT;

  // Both old and new versions of AMQP have 1 in byte 5
  if (buf[5]!=1) return PNI_PROTOCOL_UNKNOWN;

  // From here on it must be some sort of AMQP
  if (len < 8) return PNI_PROTOCOL_INSUFFICIENT;
  if (buf[6]==0 && buf[7]==0) {
    // AM<QP 1.0
      if (buf[4]==0) return PNI_PROTOCOL_AMQP1;
      if (buf[4]==2) return PNI_PROTOCOL_AMQP_SSL;
      if (buf[4]==3) return PNI_PROTOCOL_AMQP_SASL;
  }
  return PNI_PROTOCOL_AMQP_OTHER;
}

const char* pni_protocol_name(pni_protocol_type_t p)
{
  static const char * const names[] = {
  "Insufficient data to determine protocol",
  "Unknown protocol",
  "SSL/TLS connection",
  "AMQP TLS layer",
  "AMQP SASL layer",
  "AMQP 1.0 layer",
  "Pre standard AMQP connection"
  };
  return names[p];
}
