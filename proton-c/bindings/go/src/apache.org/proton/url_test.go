package proton

import (
	"fmt"
	"reflect"
	"testing"
)

func checkEqual(t *testing.T, a, b interface{}) string {
	if !reflect.DeepEqual(a, b) {
		return fmt.Sprintf("%#v != %#v", a, b)
	}
	return ""
}

// Verify URL matches fields, check round trip compose/parse
func checkUrl(t *testing.T, u Url, scheme, username, password, host, port, path string) string {
	if msg := checkEqual(t, u.Scheme, scheme); msg != "" {
		return msg
	}
	if msg := checkEqual(t, u.Username, username); msg != "" {
		return msg
	}
	if msg := checkEqual(t, u.Password, password); msg != "" {
		return msg
	}
	if msg := checkEqual(t, u.Host, host); msg != "" {
		return msg
	}
	if msg := checkEqual(t, u.Port, port); msg != "" {
		return msg
	}
	if msg := checkEqual(t, u.Path, path); msg != "" {
		return msg
	}
	u2, _ := ParseUrlRaw(u.String()) // Round trip compose/parse
	if msg := checkEqual(t, u.String(), u2.String()); msg != "" {
		return msg
	}
	return ""
}

func TestParse(t *testing.T) {
	s := "amqp://me:secret@myhost:1234/foobar"
	u, _ := ParseUrl(s)
	if msg := checkEqual(t, u.String(), s); msg != "" {
		t.Error(msg)
	}
	if msg := checkUrl(t, u, "amqp", "me", "secret", "myhost", "1234", "foobar"); msg != "" {
		t.Error(msg)
	}
}

// Test URL's with missing elements.
func TestMissing(t *testing.T) {
	u := Url{Username: "me", Password: "secret", Host: "myhost", Path: "foobar"}
	if msg := checkUrl(t, u, "", "me", "secret", "myhost", "", "foobar"); msg != "" {
		t.Error(msg)
	}
	if msg := checkEqual(t, "me:secret@myhost/foobar", u.String()); msg != "" {
		t.Error(msg)
	}
	u, _ = ParseUrlRaw("myhost/")
	if msg := checkUrl(t, u, "", "", "", "myhost", "", ""); msg != "" {
		t.Error(msg)
	}
}

func TestDefaults(t *testing.T) {
	for pre, post := range map[string]string{
		"foo":      "amqp://foo:amqp",
		":1234":    "amqp://127.0.0.1:1234",
		"/path":    "amqp://127.0.0.1:amqp/path",
		"amqp://":  "amqp://127.0.0.1:amqp",
		"amqps://": "amqps://127.0.0.1:amqps",
	} {
		url, _ := ParseUrl(pre)
		if msg := checkEqual(t, url.String(), post); msg != "" {
			t.Error(msg)
		}
	}
}

func ExampleParseUrl_1parse() {
	url, _ := ParseUrl("amqp://username:password@host:1234/path")
	fmt.Printf("%#v\n", url) // Show the struct fields.
	fmt.Println(url)         // url.String() shows the string form
	// Output:
	// proton.Url{Scheme:"amqp", Username:"username", Password:"password", Host:"host", Port:"1234", Path:"path", PortInt:1234}
	// amqp://username:password@host:1234/path
}

func ExampleParseUrl_2defaults() {
	// Default settings for partial URLs
	url, _ := ParseUrl(":5672")
	fmt.Println(url)
	url, _ = ParseUrl("host/path")
	fmt.Println(url)
	url, _ = ParseUrl("amqp://")
	fmt.Printf("%v port=%v\n", url, url.PortInt)
	url, _ = ParseUrl("amqps://")
	fmt.Printf("%v port=%v\n", url, url.PortInt)
	// Output:
	// amqp://127.0.0.1:5672
	// amqp://host:amqp/path
	// amqp://127.0.0.1:amqp port=5672
	// amqps://127.0.0.1:amqps port=5671
}

func ExampleParseUrl_3invalid() {
	// Invalid URLs
	_, err := ParseUrl("")
	fmt.Println(err)
	_, err = ParseUrl(":foobar")
	fmt.Println(err)
	// Output:
	// proton: Invalid Url ''
	// proton: Invalid Url 'amqp://127.0.0.1:foobar' (bad port 'foobar')
}
