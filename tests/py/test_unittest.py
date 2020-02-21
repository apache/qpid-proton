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
# under the License
#

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import sys

__all__ = ['unittest']


def _monkey_patch():
    """Monkey-patch a few unittest 2.7 features for Python 2.6.

    These are not the pretty versions provided by 2.7, but they do the
    same job as far as correctness is concerned.

    Only used as a measure of last resort if unittest2 is not available.
    """
    if not hasattr(unittest.TestCase, "assertMultiLineEqual"):
        def assertMultiLineEqual(self, a, b, msg=None): self.assertEqual(a, b, msg)

        unittest.TestCase.assertMultiLineEqual = assertMultiLineEqual

    if not hasattr(unittest.TestCase, "assertIn"):
        def assertIn(self, a, b, msg=None): self.assertTrue(a in b, msg)

        unittest.TestCase.assertIn = assertIn

    if not hasattr(unittest.TestCase, "assertIsNone"):
        def assertIsNone(self, obj, msg=None): self.assertEqual(obj, None, msg)

        unittest.TestCase.assertIsNone = assertIsNone

    if not hasattr(unittest, "skip"):
        def skip(reason="Test skipped"):
            return lambda f: print(reason)

        unittest.skip = skip

    if not hasattr(unittest, "skipIf"):
        def skipIf(condition, reason):
            if condition:
                return skip(reason)
            return lambda f: f

        unittest.skipIf = skipIf

    if not hasattr(unittest, "skipUnless"):
        def skipUnless(condition, reason):
            if not condition:
                return skip(reason)
            return lambda f: f

        unittest.skipUnless = skipUnless


if sys.version_info >= (2, 7):
    import unittest
else:
    try:
        import unittest2 as unittest
    except ImportError:
        import unittest

        _monkey_patch()
