/*
Licensed to the Apache Software Foundation (ASF) under one
or more contributor license agreements.  See the NOTICE file
distributed with this work for additional information
regarding copyright ownership.  The ASF licenses this file
to you under the Apache License, Version 2.0 (the
"License"); you may not use this file except in compliance
with the License.  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied.  See the License for the
specific language governing permissions and limitations
under the License.
*/

package amqp

import (
	"fmt"
	"net/url"
	"strings"
	"testing"
)

func ExampleParseURL() {
	for _, s := range []string{
		"amqp://username:password@host:1234/path",
		"host:1234",
		"host",
		"host/path",
		"amqps://host",
		"/path",
		"",
	} {
		u, err := ParseURL(s)
		if err != nil {
			fmt.Println(err)
		} else {
			fmt.Println(u)
		}
	}
	// Output:
	// amqp://username:password@host:1234/path
	// amqp://host:1234
	// amqp://host:amqp
	// amqp://host:amqp/path
	// amqps://host:amqps
	// amqp://localhost:amqp/path
	// amqp://localhost:amqp
}

func TestParseValidURL(t *testing.T) {
	for _, s := range []struct {
		// input
		url string
		// expected
		scheme string
		user   *url.Userinfo
		host   string
		port   string
		path   string
	}{
		{"amqp://username:password@host:1234/path",
			"amqp", url.UserPassword("username", "password"), "host:1234", "1234", "/path"},
		//{"[::1]",
		//	"amqp", &url2.Userinfo{}, "[::]:amqp", "amqp", ""}, // this used to be valid example; does not parse in go 1.13
		//	                                                    //  parse [::1]: first path segment in URL cannot contain colon
	} {
		u, err := ParseURL(s.url)
		if err != nil {
			t.Errorf("Unexpected parsing error on '%s' got error '%s'", s.url, err)
			continue
		}
		if u.Scheme != s.scheme {
			t.Errorf("Unexpected scheme parsing '%s', expected '%s', got '%s'", s.url, s.scheme, u.Scheme)
		}
		if *u.User != *s.user {
			t.Errorf("Unexpected user parsing '%s', expected '%s', got '%s'", s.url, s.user, u.User)
		}
		if u.Host != s.host {
			t.Errorf("Unexpected host parsing '%s', expected '%s', got '%s'", s.url, s.host, u.Host)
		}
		if u.Port() != s.port {
			t.Errorf("Unexpected port parsing '%s', expected '%s', got '%s'", s.url, s.port, u.Port())
		}
		if u.Path != s.path {
			t.Errorf("Unexpected path parsing '%s', expected '%s', got '%s'", s.url, s.path, u.Path)
		}
	}
}

func TestParseInvalidURL(t *testing.T) {
	for _, s := range []struct {
		// input
		url string
		// expected
		inError string
	}{
		// proton errors
		{"http://seznam.cz", "invalid amqp scheme"},
		// go errors
		{":1234", "missing protocol scheme"},
		{"[::1", ""}, // error message is completely different depending on Go toolchain version
	} {
		u, err := ParseURL(s.url)
		if err == nil {
			t.Errorf("Expected error on '%s', parsed out '%s'", s.url, u)
			continue
		}
		if !strings.Contains(err.Error(), s.inError) {
			t.Errorf("Expected error '%s' to contain '%s'", err, s.inError)
		}
	}
}
