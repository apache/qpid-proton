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

/*
#include <stdlib.h>
#include <string.h>
#include <proton/url.h>

// Helper function for setting URL fields.
typedef void (*setter_fn)(pn_url_t* url, const char* value);
inline void	set(pn_url_t *url, setter_fn s, const char* value) {
  s(url, value);
}
*/
import "C"

import (
	"fmt"
	"net"
	"net/url"
	"unsafe"
)

const (
	amqp  string = "amqp"
	amqps        = "amqps"
)

// ParseUrl parses an AMQP URL string and returns a net/url.Url.
//
// It is more forgiving than net/url.Parse and allows most of the parts of the
// URL to be missing, assuming AMQP defaults.
//
func ParseURL(s string) (u *url.URL, err error) {
	cstr := C.CString(s)
	defer C.free(unsafe.Pointer(cstr))
	pnUrl := C.pn_url_parse(cstr)
	if pnUrl == nil {
		return nil, fmt.Errorf("bad URL %#v", s)
	}
	defer C.pn_url_free(pnUrl)

	scheme := C.GoString(C.pn_url_get_scheme(pnUrl))
	username := C.GoString(C.pn_url_get_username(pnUrl))
	password := C.GoString(C.pn_url_get_password(pnUrl))
	host := C.GoString(C.pn_url_get_host(pnUrl))
	port := C.GoString(C.pn_url_get_port(pnUrl))
	path := C.GoString(C.pn_url_get_path(pnUrl))

	if err != nil {
		return nil, fmt.Errorf("bad URL %#v: %s", s, err)
	}
	if scheme == "" {
		scheme = amqp
	}
	if port == "" {
		if scheme == amqps {
			port = amqps
		} else {
			port = amqp
		}
	}
	var user *url.Userinfo
	if password != "" {
		user = url.UserPassword(username, password)
	} else if username != "" {
		user = url.User(username)
	}

	u = &url.URL{
		Scheme: scheme,
		User:   user,
		Host:   net.JoinHostPort(host, port),
		Path:   path,
	}

	return u, nil
}
