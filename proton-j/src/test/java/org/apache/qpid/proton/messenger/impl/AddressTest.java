/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.apache.qpid.proton.messenger.impl;

import static org.junit.Assert.*;

import org.junit.Test;

public class AddressTest {

    private void testParse(String url, String scheme, String user, String pass, String host, String port, String name)
    {
        Address address = new Address(url);
        assertEquals(scheme, address.getScheme());
        assertEquals(user, address.getUser());
        assertEquals(pass, address.getPass());
        assertEquals(host, address.getHost());
        assertEquals(port, address.getPort());
        assertEquals(url, address.toString());
    }

    @Test
    public void addressTests()
    {
        testParse("host", null, null, null, "host", null, null);
        testParse("host:423", null, null, null, "host", "423", null);
        testParse("host:423/topic://topicname", null, null, null, "host", "423", "topic://topicname");
        testParse("user@host", null, "user", null, "host", null, null);
        testParse("user:1243^&^:pw@host:423", null, "user", "1243^&^:pw", "host", "423", null);
        testParse("user:1243^&^:pw@host:423/Foo.bar:90087", null, "user", "1243^&^:pw", "host", "423", "Foo.bar:90087");
        testParse("user:1243^&^:pw@host:423/Foo.bar:90087@somewhere", null, "user", "1243^&^:pw", "host", "423", "Foo.bar:90087@somewhere");
        testParse("[::1]", null, null, null, "::1", null, null);
        testParse("[::1]:amqp", null, null, null, "::1", "amqp", null);
        testParse("user@[::1]", null, "user", null, "::1", null, null);
        testParse("user@[::1]:amqp", null, "user", null, "::1", "amqp", null);
        testParse("user:1243^&^:pw@[::1]:amqp", null, "user", "1243^&^:pw", "::1", "amqp", null);
        testParse("user:1243^&^:pw@[::1]:amqp/Foo.bar:90087", null, "user", "1243^&^:pw", "::1", "amqp", "Foo.bar:90087");
        testParse("user:1243^&^:pw@[::1:amqp/Foo.bar:90087", null, "user", "1243^&^:pw", "[", ":1:amqp", "Foo.bar:90087");
        testParse("user:1243^&^:pw@::1]:amqp/Foo.bar:90087", null, "user", "1243^&^:pw", "", ":1]:amqp", "Foo.bar:90087");
        testParse("amqp://user@[::1]", "amqp", "user", null, "::1", null, null);
        testParse("amqp://user@[::1]:amqp", "amqp", "user", null, "::1", "amqp", null);
        testParse("amqp://user@[1234:52:0:1260:f2de:f1ff:fe59:8f87]:amqp", "amqp", "user", null, "1234:52:0:1260:f2de:f1ff:fe59:8f87", "amqp", null);
        testParse("amqp://user:1243^&^:pw@[::1]:amqp", "amqp", "user", "1243^&^:pw", "::1", "amqp", null);
        testParse("amqp://user:1243^&^:pw@[::1]:amqp/Foo.bar:90087", "amqp", "user", "1243^&^:pw", "::1", "amqp", "Foo.bar:90087");
        testParse("amqp://host", "amqp", null, null, "host", null, null);
        testParse("amqp://user@host", "amqp", "user", null, "host", null, null);
        testParse("amqps://user@host", "amqps", "user", null, "host", null, null);
        testParse("amqp://user@host/path:%", "amqp", "user", null, "host", null, "path:%");
        testParse("amqp://user@host:5674/path:%", "amqp", "user", null, "host", "5674", "path:%");
        testParse("amqp://user@host/path:%", "amqp", "user", null, "host", null, "path:%");
        testParse("amqp://bigbird@host/queue@host", "amqp", "bigbird", null, "host", null, "queue@host");
        testParse("amqp://host/queue@host", "amqp", null, null, "host", null, "queue@host");
        testParse("amqp://host:9765/queue@host", "amqp", null, null, "host", "9765", "queue@host");
    }
}
