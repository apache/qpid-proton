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
	"errors"
	"net"
	"net/url"
	"strings"
)

const (
	amqp  string = "amqp"
	amqps        = "amqps"
	defaulthost  = "localhost"
)

// The way this is used it can only get a hostport already validated by
// the URL parser, so this means we can skip some error checks
func splitHostPort(hostport string) (string, string, error) {
	if hostport == "" {
		return "", "", nil
	}
	if hostport[0] == '[' {
		// There must be a matching ']' as already validated
		if l := strings.LastIndex(hostport, "]"); len(hostport) == l+1 {
			// trim off '[' and ']'
			return hostport[1:l], "", nil
		}
	} else if strings.IndexByte(hostport, ':') < 0 {
		return hostport, "", nil
	}
	return net.SplitHostPort(hostport)
}

func UpdateURL(in *url.URL) (err error) {
	// Detect form without "amqp://" and stick it on front
	// to make it match the usual proton defaults
	u := new (url.URL)
	*u = *in
	if (u.Scheme != "" && u.Opaque != "") ||
	   (u.Scheme == "" && u.Host == "") {
		input := u.String()
		input = "amqp://" + input
		u, err = url.Parse(input)
		if err != nil {
			return
		}
	}
	// If Scheme is still "" then default to amqp
	if u.Scheme == "" {
		u.Scheme = amqp
	}
	// Error if the scheme is not an amqp scheme
	if u.Scheme != amqp && u.Scheme != amqps {
		return errors.New("invalid amqp scheme")
	}
	// Decompose Host into host and port
	host, port, err := splitHostPort(u.Host)
	if err != nil {
		return
	}
	if host == "" {
		host = defaulthost
	}
	if port == "" {
		port = u.Scheme
	}
	u.Host = net.JoinHostPort(host, port)
	*in = *u
	return nil
}

// ParseUrl parses an AMQP URL string and returns a net/url.Url.
//
// It is more forgiving than net/url.Parse and allows most of the parts of the
// URL to be missing, assuming AMQP defaults.
//
func ParseURL(s string) (u *url.URL, err error) {
	if u, err = url.Parse(s); err != nil {
		return
	}
	if err = UpdateURL(u); err != nil {
		u = nil
	}
	return u, err
}
