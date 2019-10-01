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

package electron

import (
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"

	"github.com/apache/qpid-proton/go/pkg/internal/test"
)

func TestAuthAnonymous(t *testing.T) {
	p := newPipe(t,
		[]ConnectionOption{User("fred"), VirtualHost("vhost"), SASLAllowInsecure(true)},
		[]ConnectionOption{SASLAllowedMechs("ANONYMOUS"), SASLAllowInsecure(true)})
	test.FatalIf(t, p.server.Sync())
	test.ErrorIf(t, test.Differ("anonymous", p.server.User()))
	test.ErrorIf(t, test.Differ("vhost", p.server.VirtualHost()))
}

func TestAuthPlain(t *testing.T) {
	extendedSASL.startTest(t)
	p := newPipe(t,
		[]ConnectionOption{SASLAllowInsecure(true), SASLAllowedMechs("PLAIN"), User("fred@proton"), Password([]byte("xxx"))},
		[]ConnectionOption{SASLAllowInsecure(true), SASLAllowedMechs("PLAIN")})
	test.FatalIf(t, p.server.Sync())
	test.ErrorIf(t, test.Differ("fred@proton", p.server.User()))
}

func TestAuthBadPass(t *testing.T) {
	extendedSASL.startTest(t)
	p := newPipe(t,
		[]ConnectionOption{SASLAllowInsecure(true), SASLAllowedMechs("PLAIN"), User("fred@proton"), Password([]byte("yyy"))},
		[]ConnectionOption{SASLAllowInsecure(true), SASLAllowedMechs("PLAIN")})
	if p.server.Sync() == nil {
		t.Error("Expected auth failure for bad pass")
	}
}

func TestAuthBadUser(t *testing.T) {
	extendedSASL.startTest(t)
	p := newPipe(t,
		[]ConnectionOption{SASLAllowInsecure(true), SASLAllowedMechs("PLAIN"), User("foo@bar"), Password([]byte("yyy"))},
		[]ConnectionOption{SASLAllowInsecure(true), SASLAllowedMechs("PLAIN")})
	if p.server.Sync() == nil {
		t.Error("Expected auth failure for bad user")
	}
}

type extendedSASLState struct {
	err error
	dir string
}

func (s *extendedSASLState) setup() {
	if SASLExtended() {
		if s.dir, s.err = ioutil.TempDir("", ""); s.err == nil {
			GlobalSASLConfigDir(s.dir)
			GlobalSASLConfigName("test")
			conf := filepath.Join(s.dir, "test.conf")
			db := filepath.Join(s.dir, "proton.sasldb")
			saslpasswd := os.Getenv("SASLPASSWD")
			if saslpasswd == "" {
				saslpasswd = "saslpasswd2"
			}
			cmd := exec.Command(saslpasswd, "-c", "-p", "-f", db, "-u", "proton", "fred")
			cmd.Stdin = strings.NewReader("xxx") // Password
			if _, s.err = cmd.CombinedOutput(); s.err == nil {
				confStr := fmt.Sprintf(`
sasldb_path: %s
mech_list: EXTERNAL DIGEST-MD5 SCRAM-SHA-1 CRAM-MD5 PLAIN ANONYMOUS
`, db)
				s.err = ioutil.WriteFile(conf, []byte(confStr), os.ModePerm)
			}
		}
	}
	// Note we don't do anything with s.err now, tests that need the
	// extended SASL config will fail if s.err != nil. If no such tests
	// are run then it is not an error that we couldn't set it up.
}

func (s extendedSASLState) teardown() {
	if s.dir != "" {
		_ = os.RemoveAll(s.dir)
	}
}

func (s extendedSASLState) startTest(t *testing.T) {
	if !SASLExtended() {
		t.Skipf("Extended SASL not enabled")
	} else if extendedSASL.err != nil {
		t.Skipf("Extended SASL setup error: %v", extendedSASL.err)
	}
}

var extendedSASL extendedSASLState

func TestMain(m *testing.M) {
	// Do global SASL setup/teardown in main.
	// Doing it on-demand makes the tests fragile to parallel test runs and
	// changes in test ordering.
	extendedSASL.setup()
	status := m.Run()
	extendedSASL.teardown()
	os.Exit(status)
}
