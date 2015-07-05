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

def pn_class_name(cls):
  return cls

def pn_void2py(obj):
  return obj

def pn_py2void(obj):
    return obj

def pn_cast_pn_connection(obj):
    return obj

def pn_cast_pn_session(obj):
    return obj

def pn_cast_pn_link(obj):
    return obj

def pn_cast_pn_delivery(obj):
    return obj

def pn_cast_pn_transport(obj):
    return obj

def pn_cast_pn_reactor(obj):
    return obj

def pn_cast_pn_task(obj):
    return obj

def pn_cast_pn_selectable(obj):
    return obj

PN_PYREF = None

def pn_record_def(record, key, clazz):
  pass

from java.lang import Object

def pn_record_get(record, key):
  return record.get(key, Object)

def pn_record_set(record, key, value):
  record.set(key, Object, value)

def pn_incref(obj):
  pass

def pn_decref(obj):
  pass

def pn_free(obj):
  pass

from java.lang import StringBuilder

def pn_string(st):
  sb = StringBuilder()
  if st:
    sb.append(st)
  return sb

def pn_string_get(sb):
  return sb.toString()

def pn_inspect(obj, st):
  if obj is None:
    st.append("null")
  else:
    st.append(repr(obj))
  return 0
