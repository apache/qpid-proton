package proton

/*
#cgo LDFLAGS: -lqpid-proton
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
	"unsafe"
)

// Url holds the fields of a parsed AMQP URL.
//
// An AMQP URL contains information
// to connect to a server (like any URL) and a path string that is interpreted
// by the remote server to identify an AMQP node such as a queue or topic.
//
// The fields are:
//
//  Scheme: "amqp" or "amqps" for AMQP over SSL
//  Username: user name for authentication
//  Password: password for authentication
//  Host: Host address, can be a DNS name or an IP address literal (v4 or v6)
//  Port: Note this is a string, not a number. It an be a numeric value like "5672" or a service name like "amqp"
//  Path: Interpeted by the remote end as an AMQP node address.
//  PortInt: int value of port, set after calling Validate()
//
// A URL string has the form "scheme://username:password@host:port/path".
// Partial URLs such as "host", "host:port", ":port" or "host/path" are allowed.
//
type Url struct {
	Scheme, Username, Password, Host, Port, Path string
	PortInt                                      int
}

// urlFromC creates a Url and copies the fields from the C pn_url_t pnUrl
func urlFromC(pnUrl *C.pn_url_t) Url {
	return Url{
		C.GoString(C.pn_url_get_scheme(pnUrl)),
		C.GoString(C.pn_url_get_username(pnUrl)),
		C.GoString(C.pn_url_get_password(pnUrl)),
		C.GoString(C.pn_url_get_host(pnUrl)),
		C.GoString(C.pn_url_get_port(pnUrl)),
		C.GoString(C.pn_url_get_path(pnUrl)),
		0,
	}
}

// ParseUrl parses a URL string and returns a Url and an error if it is not valid.
//
// For a partial URL string, missing components will be filled in with default values.
//
func ParseUrl(s string) (u Url, err error) {
	u, err = ParseUrlRaw(s)
	if err == nil {
		err = u.Validate()
	}
	if err != nil {
		return Url{}, err
	}
	return
}

// ParseUrlRaw parses a URL string and returns a Url and an error if it is not valid.
//
// For a partial URL string, missing components will be empty strings "" in the Url struct.
// Error checking is limited, more errors are checked in Validate()
//
func ParseUrlRaw(s string) (u Url, err error) {
	cstr := C.CString(s)
	defer C.free(unsafe.Pointer(cstr))
	pnUrl := C.pn_url_parse(cstr)
	if pnUrl == nil {
		return Url{}, fmt.Errorf("proton: Invalid Url '%v'", s)
	}
	defer C.pn_url_free(pnUrl)
	u = urlFromC(pnUrl)
	return
}

// newPnUrl creates a C pn_url_t and populate it with the fields from url.
func newPnUrl(url Url) *C.pn_url_t {
	pnUrl := C.pn_url()
	set := func(setter unsafe.Pointer, value string) {
		if value != "" {
			cstr := C.CString(value)
			defer C.free(unsafe.Pointer(cstr))
			C.set(pnUrl, C.setter_fn(setter), cstr)
		}
	}
	set(C.pn_url_set_scheme, url.Scheme)
	set(C.pn_url_set_username, url.Username)
	set(C.pn_url_set_password, url.Password)
	set(C.pn_url_set_host, url.Host)
	set(C.pn_url_set_port, url.Port)
	set(C.pn_url_set_path, url.Path)
	return pnUrl
}

// String generates a representation of the Url.
func (u Url) String() string {
	pnUrl := newPnUrl(u)
	defer C.pn_url_free(pnUrl)
	return C.GoString(C.pn_url_str(pnUrl))
}

// Validate supplies defaults for Scheme, Host and Port and does additional error checks.
//
// Fills in the value of PortInt from Port.
//
func (u *Url) Validate() error {
	if u.Scheme == "" {
		u.Scheme = "amqp"
	}
	if u.Host == "" {
		u.Host = "127.0.0.1"
	}
	if u.Port == "" {
		if u.Scheme == "amqps" {
			u.Port = "amqps"
		} else {
			u.Port = "amqp"
		}
	}
	port, err := net.LookupPort("", u.Port)
	u.PortInt = port
	if u.PortInt == 0 || err != nil {
		return fmt.Errorf("proton: Invalid Url '%v' (bad port '%v')", u, u.Port)
	}
	return nil
}

// Equals tests for equality between two Urls.
func (u Url) Equals(x Url) bool {
	return (&u == &x) || (u.Scheme == x.Scheme &&
		u.Username == x.Username && u.Password == x.Password &&
		u.Host == x.Host && u.Port == x.Port && u.Path == x.Path)
}
