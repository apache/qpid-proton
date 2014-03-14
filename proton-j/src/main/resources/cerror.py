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

# from proton/error.h
PN_EOS = -1
PN_ERR = -2
PN_OVERFLOW = -3
PN_UNDERFLOW = -4
PN_STATE_ERR = -5
PN_ARG_ERR = -6
PN_TIMEOUT =-7
PN_INTR = -8
PN_INPROGRESS =-9

class pn_error:

  def __init__(self, code, text):
    self.code = code
    self.text = text

  def set(self, code, text):
    self.code = code
    self.text = text
    return self.code

def pn_error_code(err):
  return err.code

def pn_error_text(err):
  return err.text

class Skipped(Exception):
  skipped = True
