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

// Code generator to generate a thin Go wrapper API around the C proton API.
//
// Not run automatically, generated sources are checked in. To update the
// generated sources run `go run genwrap.go` in this directory.
//
// WARNING: generating code from the wrong proton header file versions
// will break compatibility guarantees. This program will attempt to detect
// such errors. If you are deliberately changing the compatibility requirements
// update the variable minVersion below.
//

package main

import (
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"os/exec"
	"path"
	"regexp"
	"strings"
	"text/template"
)

// Note last code generation was from 0.27 source, but we should still
// be binary compatible back to 0.10. Only change was the type name
// for pn_proton_tick() from pn_timestamp_t to int64_t, identical type.

var minVersion = "0.27" // The proton-c header version last used to generate code

var include = flag.String("include", "../c/include", "Directory containing proton/*.h include files")

var versionH = regexp.MustCompile("(?s:PN_VERSION_MAJOR ([0-9]+).*PN_VERSION_MINOR ([0-9]+))")
var versionTxt = regexp.MustCompile("^[0-9]+\\.[0-9]+")

func main() {
	flag.Parse()
	genVersion()
	genWrappers()
}

func getVersion() string {
	_, err := ioutil.ReadFile(path.Join(*include, "proton/version.h.in"))
	if err == nil {
		// We are using the headers in git sources, get the VERSION.txt
		vt, err := ioutil.ReadFile(path.Join(*include, "../../VERSION.txt"))
		panicIf(err)
		return versionTxt.FindString(string(vt))
	}
	vh, err := ioutil.ReadFile(path.Join(*include, "proton/version.h"))
	if err == nil {
		// We are using installed headers
		return strings.Join(versionH.FindStringSubmatch(string(vh))[1:], ".")
	}
	panic(err)
}

func genVersion() {
	version := getVersion()
	if minVersion != version {
		panic(fmt.Errorf("Found proton-c version %v, expected %v. Update minVersion in genwrap.go if you want to increase the minimum required proton-c version.", version, minVersion))
	}
	out, err := os.Create("pkg/amqp/version.go")
	panicIf(err)
	defer out.Close()
	splitVersion := strings.Split(minVersion, ".")
	fmt.Fprintf(out, copyright+`

package amqp

// Version check for proton library.
// Done here because this is the lowest-level dependency for all the proton Go packages.

// #include <proton/version.h>
// #if PN_VERSION_MAJOR == %s && PN_VERSION_MINOR < %s
// #error module github.com/apache/qpid-proton requires Proton-C library version 0.10 or greater
// #endif
import "C"
`, splitVersion[0], splitVersion[1])
}

func genWrappers() {
	outPath := "pkg/proton/wrappers_gen.go"
	out, err := os.Create(outPath)
	panicIf(err)
	defer out.Close()

	apis := []string{"session", "link", "delivery", "disposition", "condition", "terminus", "connection", "transport", "sasl"}
	fmt.Fprintln(out, copyright)
	fmt.Fprint(out, `
package proton

import (
	"time"
  "unsafe"
)

// #include <proton/condition.h>
// #include <proton/error.h>
// #include <proton/event.h>
// #include <proton/types.h>
// #include <stdlib.h>
import "C"

`)
	for _, api := range apis {
		fmt.Fprintf(out, "// #include <proton/%s.h>\n", api)
	}
	fmt.Fprintln(out, `import "C"`)

	event(out)

	for _, api := range apis {
		fmt.Fprintf(out, "// Wrappers for declarations in %s.h\n\n", api)
		header := readHeader(api)
		enums := findEnums(header)
		for _, e := range enums {
			genEnum(out, e.Name, e.Values)
		}
		apiWrapFns(api, header, out)
	}
	out.Close()
	if err := exec.Command("gofmt", "-w", outPath).Run(); err != nil {
		fmt.Fprintf(os.Stderr, "gofmt: %s", err)
		os.Exit(1)
	}
}

// Identify acronyms that should be uppercase not Mixedcase
var acronym = regexp.MustCompile("(?i)SASL|AMQP")

func mixedCase(s string) string {
	result := ""
	for _, w := range strings.Split(s, "_") {
		if acronym.MatchString(w) {
			w = strings.ToUpper(w)
		} else {
			w = strings.ToUpper(w[0:1]) + strings.ToLower(w[1:])
		}
		result = result + w
	}
	return result
}

func mixedCaseTrim(s, prefix string) string {
	return mixedCase(strings.TrimPrefix(s, prefix))
}

var templateFuncs = template.FuncMap{"mixedCase": mixedCase, "mixedCaseTrim": mixedCaseTrim}

func doTemplate(out io.Writer, data interface{}, tmpl string) {
	panicIf(template.Must(template.New("").Funcs(templateFuncs).Parse(tmpl)).Execute(out, data))
}

type enumType struct {
	Name   string
	Values []string
}

// Find enums in a header file return map of enum name to values.
func findEnums(header string) (enums []enumType) {
	for _, enum := range enumDefRe.FindAllStringSubmatch(header, -1) {
		enums = append(enums, enumType{enum[2], enumValRe.FindAllString(enum[1], -1)})
	}
	return enums
}

// Types that are integral, not wrappers. Enums are added automatically.
var simpleType = map[string]bool{
	"State": true, // integral typedef
}

func genEnum(out io.Writer, name string, values []string) {
	simpleType[mixedCase(name)] = true
	doTemplate(out, []interface{}{name, values}, `
{{$enumName := index . 0}}{{$values := index . 1}}
type {{mixedCase $enumName}} C.pn_{{$enumName}}_t
const ({{range $values}}
	{{mixedCaseTrim . "PN_"}} {{mixedCase $enumName}} = C.{{.}} {{end}}
)

func (e {{mixedCase $enumName}}) String() string {
	switch e {
{{range $values}}
	case C.{{.}}: return "{{mixedCaseTrim . "PN_"}}" {{end}}
	}
	return "unknown"
}
`)
}

func panicIf(err error) {
	if err != nil {
		panic(err)
	}
}

func readHeader(name string) string {
	s, err := ioutil.ReadFile(path.Join(*include, "proton", name+".h"))
	panicIf(err)
	return string(s)
}

var copyright string = `/*
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

//
// NOTE: DO NOT EDIT. This file was generated by genwrap.go from the proton header files.
// Update the generator and re-run if you need to modify this code.
//
`

type eventType struct {
	// C, function and interface names for the event
	Name, Cname, Fname, Iname string
}

func newEventType(cName string) eventType {
	var etype eventType
	etype.Cname = cName
	etype.Name = mixedCaseTrim(cName, "PN_")
	etype.Fname = "On" + etype.Name
	etype.Iname = etype.Fname + "Interface"
	return etype
}

var (
	enumDefRe   = regexp.MustCompile("typedef enum {([^}]*)} pn_([a-z_]+)_t;")
	enumValRe   = regexp.MustCompile("PN_[A-Z_]+")
	skipEventRe = regexp.MustCompile("EVENT_NONE|REACTOR|SELECTABLE|TIMER")
	skipFnRe    = regexp.MustCompile("attach|context|class|collect|link_recv|link_send|transport_.*logf$|transport_.*trace|transport_head|transport_tail|transport_push|connection_set_password|link_get_drain")
)

// Generate event wrappers.
func event(out io.Writer) {
	event_h := readHeader("event")

	// Event is implemented by hand in wrappers.go

	// Get all the pn_event_type_t enum values
	var etypes []eventType
	enums := findEnums(event_h)
	for _, e := range enums[0].Values {
		if skipEventRe.FindStringSubmatch(e) == nil {
			etypes = append(etypes, newEventType(e))
		}
	}

	doTemplate(out, etypes, `
type EventType int
const ({{range .}}
	 E{{.Name}} EventType = C.{{.Cname}}{{end}}
)
`)

	doTemplate(out, etypes, `
func (e EventType) String() string {
	switch e {
{{range .}}
	case C.{{.Cname}}: return "{{.Name}}"{{end}}
	}
	return "Unknown"
}
`)
}

type genType struct {
	Ctype, Gotype string
	ToGo          func(value string) string
	ToC           func(value string) string
	Assign        func(value string) string
}

func (g genType) printBody(out io.Writer, value string) {
	if g.Gotype != "" {
		fmt.Fprintf(out, "return %s", g.ToGo(value))
	} else {
		fmt.Fprintf(out, "%s", value)
	}
}

func (g genType) goLiteral(value string) string {
	return fmt.Sprintf("%s{%s}", g.Gotype, value)
}

func (g genType) goConvert(value string) string {
	switch g.Gotype {
	case "string":
		return fmt.Sprintf("C.GoString(%s)", value)
	case "Event":
		return fmt.Sprintf("makeEvent(%s)", value)
	default:
		return fmt.Sprintf("%s(%s)", g.Gotype, value)
	}
}

func mapType(ctype string) (g genType) {
	g.Ctype = "C." + strings.Trim(ctype, " \n")

	// Special-case mappings for C types, default: is the general wrapper type case.
	switch g.Ctype {
	case "C.void":
		g.Gotype = ""
	case "C.size_t":
		g.Gotype = "uint"
	case "C.int":
		g.Gotype = "int"
	case "C.void *":
		g.Gotype = "unsafe.Pointer"
		g.Ctype = "unsafe.Pointer"
	case "C.bool":
		g.Gotype = "bool"
	case "C.ssize_t":
		g.Gotype = "int"
	case "C.int64_t":
		g.Gotype = "int64"
	case "C.int32_t":
		g.Gotype = "int16"
	case "C.int16_t":
		g.Gotype = "int32"
	case "C.uint64_t":
		g.Gotype = "uint64"
	case "C.uint32_t":
		g.Gotype = "uint16"
	case "C.uint16_t":
		g.Gotype = "uint32"
	case "C.const char *":
		fallthrough
	case "C.char *":
		g.Gotype = "string"
		g.Ctype = "C.CString"
		g.ToC = func(v string) string { return fmt.Sprintf("%sC", v) }
		g.Assign = func(v string) string {
			return fmt.Sprintf("%sC := C.CString(%s)\n defer C.free(unsafe.Pointer(%sC))\n", v, v, v)
		}
	case "C.pn_seconds_t":
		g.Gotype = "time.Duration"
		g.ToGo = func(v string) string { return fmt.Sprintf("(time.Duration(%s) * time.Second)", v) }
		g.ToC = func(v string) string { return fmt.Sprintf("C.pn_seconds_t(%s/time.Second)", v) }
	case "C.pn_millis_t":
		g.Gotype = "time.Duration"
		g.ToGo = func(v string) string { return fmt.Sprintf("(time.Duration(%s) * time.Millisecond)", v) }
		g.ToC = func(v string) string { return fmt.Sprintf("C.pn_millis_t(%s/time.Millisecond)", v) }
	case "C.pn_timestamp_t":
		g.Gotype = "time.Time"
		g.ToC = func(v string) string { return fmt.Sprintf("pnTime(%s)", v) }
		g.ToGo = func(v string) string { return fmt.Sprintf("goTime(%s)", v) }
	case "C.pn_error_t *":
		g.Gotype = "error"
		g.ToGo = func(v string) string { return fmt.Sprintf("PnError(%s)", v) }
	default:
		pnId := regexp.MustCompile(" *pn_([a-z_]+)_t *\\*? *")
		match := pnId.FindStringSubmatch(g.Ctype)
		if match == nil {
			panic(fmt.Errorf("unknown C type %#v", g.Ctype))
		}
		g.Gotype = mixedCase(match[1])
		if !simpleType[g.Gotype] {
			g.ToGo = g.goLiteral
			g.ToC = func(v string) string { return v + ".pn" }
		}
	}
	if g.ToGo == nil {
		g.ToGo = g.goConvert // Use conversion by default.
	}
	if g.ToC == nil {
		g.ToC = func(v string) string { return fmt.Sprintf("%s(%s)", g.Ctype, v) }
	}
	return
}

type genArg struct {
	Name string
	genType
}

var typeNameRe = regexp.MustCompile("^(.*( |\\*))([^ *]+)$")

func splitArgs(argstr string) []genArg {
	argstr = strings.Trim(argstr, " \n")
	if argstr == "" {
		return []genArg{}
	}
	args := make([]genArg, 0)
	for _, item := range strings.Split(argstr, ",") {
		item = strings.Trim(item, " \n")
		typeName := typeNameRe.FindStringSubmatch(item)
		if typeName == nil {
			panic(fmt.Errorf("Can't split argument type/name %#v", item))
		}
		cType := strings.Trim(typeName[1], " \n")
		name := strings.Trim(typeName[3], " \n")
		if name == "type" {
			name = "type_"
		}
		args = append(args, genArg{name, mapType(cType)})
	}
	return args
}

func goArgs(args []genArg) string {
	l := ""
	for i, arg := range args {
		if i != 0 {
			l += ", "
		}
		l += arg.Name + " " + arg.Gotype
	}
	return l
}

func cArgs(args []genArg) string {
	l := ""
	for _, arg := range args {
		l += fmt.Sprintf(", %s", arg.ToC(arg.Name))
	}
	return l
}

func cAssigns(args []genArg) string {
	l := "\n"
	for _, arg := range args {
		if arg.Assign != nil {
			l += fmt.Sprintf("%s\n", arg.Assign(arg.Name))
		}
	}
	return l
}

// Return the go name of the function or "" to skip the function.
func goFnName(api, fname string) string {
	// Skip class, context and attachment functions.
	if skipFnRe.FindStringSubmatch(api+"_"+fname) != nil {
		return ""
	}
	return mixedCaseTrim(fname, "get_")
}

func apiWrapFns(api, header string, out io.Writer) {
	defer func() {
		if err := recover(); err != nil {
			panic(fmt.Sprintf("in %s.h: %s", api, err))
		}
	}()
	fmt.Fprintf(out, "type %s struct{pn *C.pn_%s_t}\n", mixedCase(api), api)
	fmt.Fprintf(out, "func (%c %s) IsNil() bool { return %c.pn == nil }\n", api[0], mixedCase(api), api[0])
	fmt.Fprintf(out, "func (%c %s) CPtr() unsafe.Pointer { return unsafe.Pointer(%c.pn) }\n", api[0], mixedCase(api), api[0])
	fn := regexp.MustCompile(fmt.Sprintf(`PN_EXTERN ([a-z0-9_ ]+ *\*?) *pn_%s_([a-z_]+)\(pn_%s_t *\*[a-z_]+ *,? *([^)]*)\)`, api, api))
	for _, m := range fn.FindAllStringSubmatch(header, -1) {
		rtype, fname, argstr := mapType(m[1]), m[2], m[3]
		gname := goFnName(api, fname)
		if gname == "" { // Skip
			continue
		}
		args := splitArgs(argstr)
		fmt.Fprintf(out, "func (%c %s) %s", api[0], mixedCase(api), gname)
		fmt.Fprintf(out, "(%s) %s { ", goArgs(args), rtype.Gotype)
		fmt.Fprint(out, cAssigns(args))
		rtype.printBody(out, fmt.Sprintf("C.pn_%s_%s(%c.pn%s)", api, fname, api[0], cArgs(args)))
		fmt.Fprintf(out, "}\n")
	}
}
