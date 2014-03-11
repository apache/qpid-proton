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
	}

	@Test
	public void addressTests()
	{
		testParse("host", null, null, null, "host", null, null);
		testParse("host:423", null, null, null, "host", "423", null);
		testParse("user@host", null, "user", null, "host", null, null);
		testParse("user:1243^&^:pw@host:423", null, "user", "1243^&^:pw", "host", "423", null);
		testParse("user:1243^&^:pw@host:423/Foo.bar:90087", null, "user", "1243^&^:pw", "host", "423", "Foo.bar:90087");
		testParse("user:1243^&^:pw@host:423/Foo.bar:90087@somewhere", null, "user", "1243^&^:pw", "host", "423", "Foo.bar:90087@somewhere");
		testParse("amqp://host", "amqp", null, null, "host", null, null);
		testParse("amqp://user@host", "amqp", "user", null, "host", null, null);
		testParse("amqp://user@host/path:%", "amqp", "user", null, "host", null, "path:%");
		testParse("amqp://user@host:5674/path:%", "amqp", "user", null, "host", "5674", "path:%");
		testParse("amqp://user@host/path:%", "amqp", "user", null, "host", null, "path:%");
		testParse("amqp://bigbird@host/queue@host", "amqp", "bigbird", null, "host", null, "queue@host");
		testParse("amqp://host/queue@host", "amqp", null, null, "host", null, "queue@host");
		testParse("amqp://host:9765/queue@host", "amqp", null, null, "host", "9765", "queue@host");
	}

}
