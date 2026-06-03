#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

"""Run tx_tester -f scripts against python/examples/broker.py."""

import glob
import os
import re
import socket
import subprocess
import time
import unittest


def _env(name):
    value = os.environ.get(name)
    if not value:
        raise unittest.SkipTest(f"{name} is not set")
    return value


def _port_open(host, port, timeout=1.0):
    try:
        with socket.create_connection((host, port), timeout):
            return True
    except OSError:
        return False


def _wait_for_port(host, port, attempts=100, interval=0.1):
    for _ in range(attempts):
        if _port_open(host, port):
            return
        time.sleep(interval)
    raise RuntimeError(f"timed out waiting for {host}:{port}")


def _discover_tx_test_scripts():
    script_dir = os.environ.get("TX_TESTER_SCRIPTS")
    if not script_dir:
        return []
    pattern = os.path.join(script_dir, "*.tx_test")
    return sorted(os.path.basename(path) for path in glob.glob(pattern))


def _test_method_name(script_name):
    stem = script_name.removesuffix(".tx_test")
    safe = re.sub(r"[^0-9A-Za-z_]", "_", stem)
    return f"test_{safe}"


def _make_tx_test_method(script_name):
    def test_method(self):
        self.run_tx_test(script_name)

    test_method.__name__ = _test_method_name(script_name)
    test_method.__doc__ = f"Run tx_tester script {script_name}."
    return test_method


class TxTesterBrokerTest(unittest.TestCase):
    """Shared broker fixture for tx_tester script files."""

    host = "127.0.0.1"
    port = 5672
    txn_timeout = "2"

    @classmethod
    def setUpClass(cls):
        cls.tx_tester = _env("TX_TESTER")
        cls.python = _env("TX_TESTER_PYTHON")
        cls.broker_script = _env("TX_TESTER_BROKER")
        cls.script_dir = _env("TX_TESTER_SCRIPTS")

        subprocess.run(
            [cls.python, "-c", "import proton"],
            check=True,
            capture_output=True,
            text=True,
        )

        if _port_open(cls.host, cls.port):
            raise unittest.SkipTest(
                f"port {cls.port} already in use; stop other brokers before running"
            )

        # Future: TX_TESTER_BROKER_CMD / TX_TESTER_BROKER_URL for other brokers.
        cls.broker = subprocess.Popen(
            [cls.python, cls.broker_script, "-t", cls.txn_timeout],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            _wait_for_port(cls.host, cls.port)
        except RuntimeError:
            cls.broker.kill()
            cls.broker.wait()
            out, err = cls.broker.communicate(timeout=1)
            raise RuntimeError(
                f"broker failed to listen on {cls.host}:{cls.port}\n"
                f"stdout: {out}\nstderr: {err}"
            )

    @classmethod
    def tearDownClass(cls):
        if getattr(cls, "broker", None) is not None:
            cls.broker.terminate()
            try:
                cls.broker.wait(timeout=5)
            except subprocess.TimeoutExpired:
                cls.broker.kill()
                cls.broker.wait()

    def run_tx_test(self, script_name):
        script_path = os.path.join(self.script_dir, script_name)
        self.assertTrue(os.path.isfile(script_path), script_path)
        result = subprocess.run(
            [self.tx_tester, "-f", script_path],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            self.fail(
                f"{script_name} failed (exit {result.returncode})\n"
                f"stdout:\n{result.stdout}\n"
                f"stderr:\n{result.stderr}"
            )


class TestTxScriptSuite(TxTesterBrokerTest):
    """One test method per *.tx_test script (methods added at import time)."""

    pass


for _script_name in _discover_tx_test_scripts():
    _method = _make_tx_test_method(_script_name)
    setattr(TestTxScriptSuite, _method.__name__, _method)


if __name__ == "__main__":
    unittest.main()
