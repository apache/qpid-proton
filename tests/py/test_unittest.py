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

import unittest

# Monkey-patch a few unittest 2.7 features for Python 2.6.
#
# These are note the pretty versions provided by 2.7 but they do the
# same job as far as correctness is concerned.

if not hasattr(unittest.TestCase, "assertMultiLineEqual"):
    def assertMultiLineEqual(self, a, b, msg=None): self.assertEqual(a,b,msg)
    unittest.TestCase.assertMultiLineEqual = assertMultiLineEqual

if not hasattr(unittest.TestCase, "assertIn"):
    def assertIn(self, a, b, msg=None): self.assertTrue(a in b,msg)
    unittest.TestCase.assertIn = assertIn


