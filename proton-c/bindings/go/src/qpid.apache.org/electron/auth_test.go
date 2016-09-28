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
)

func testAuthClientServer(t *testing.T, copts []ConnectionOption, sopts []ConnectionOption) (got connectionSettings, err error) {
	client, server := newClientServerOpts(t, copts, sopts)
	defer closeClientServer(client, server)

	go func() {
		for in := range server.Incoming() {
			switch in := in.(type) {
			case *IncomingConnection:
				got = connectionSettings{user: in.User(), virtualHost: in.VirtualHost()}
			}
			in.Accept()
		}
	}()

	err = client.Sync()
	return
}

func TestAuthAnonymous(t *testing.T) {
	fatalIf(t, configureSASL())
	got, err := testAuthClientServer(t,
		[]ConnectionOption{User("fred"), VirtualHost("vhost"), SASLAllowInsecure(true)},
		[]ConnectionOption{SASLAllowedMechs("ANONYMOUS"), SASLAllowInsecure(true)})
	fatalIf(t, err)
	errorIf(t, checkEqual(connectionSettings{user: "anonymous", virtualHost: "vhost"}, got))
}

func TestAuthPlain(t *testing.T) {
	fatalIf(t, configureSASL())
	got, err := testAuthClientServer(t,
		[]ConnectionOption{SASLAllowInsecure(true), SASLAllowedMechs("PLAIN"), User("fred@proton"), Password([]byte("xxx"))},
		[]ConnectionOption{SASLAllowInsecure(true), SASLAllowedMechs("PLAIN")})
	fatalIf(t, err)
	errorIf(t, checkEqual(connectionSettings{user: "fred@proton"}, got))
}

func TestAuthBadPass(t *testing.T) {
	fatalIf(t, configureSASL())
	_, err := testAuthClientServer(t,
		[]ConnectionOption{SASLAllowInsecure(true), SASLAllowedMechs("PLAIN"), User("fred@proton"), Password([]byte("yyy"))},
		[]ConnectionOption{SASLAllowInsecure(true), SASLAllowedMechs("PLAIN")})
	if err == nil {
		t.Error("Expected auth failure for bad pass")
	}
}

func TestAuthBadUser(t *testing.T) {
	fatalIf(t, configureSASL())
	_, err := testAuthClientServer(t,
		[]ConnectionOption{SASLAllowInsecure(true), SASLAllowedMechs("PLAIN"), User("foo@bar"), Password([]byte("yyy"))},
		[]ConnectionOption{SASLAllowInsecure(true), SASLAllowedMechs("PLAIN")})
	if err == nil {
		t.Error("Expected auth failure for bad user")
	}
}

var confDir string
var confErr error

func configureSASL() error {
	if confDir != "" || confErr != nil {
		return confErr
	}
	confDir, confErr = ioutil.TempDir("", "")
	if confErr != nil {
		return confErr
	}

	GlobalSASLConfigDir(confDir)
	GlobalSASLConfigName("test")
	conf := filepath.Join(confDir, "test.conf")

	db := filepath.Join(confDir, "proton.sasldb")
	cmd := exec.Command("saslpasswd2", "-c", "-p", "-f", db, "-u", "proton", "fred")
	cmd.Stdin = strings.NewReader("xxx") // Password
	if out, err := cmd.CombinedOutput(); err != nil {
		confErr = fmt.Errorf("saslpasswd2 failed: %s\n%s", err, out)
		return confErr
	}
	confStr := "sasldb_path: " + db + "\nmech_list: EXTERNAL DIGEST-MD5 SCRAM-SHA-1 CRAM-MD5 PLAIN ANONYMOUS\n"
	if err := ioutil.WriteFile(conf, []byte(confStr), os.ModePerm); err != nil {
		confErr = fmt.Errorf("write conf file %s failed: %s", conf, err)
	}
	return confErr
}

func TestMain(m *testing.M) {
	status := m.Run()
	if confDir != "" {
		_ = os.RemoveAll(confDir)
	}
	os.Exit(status)
}
