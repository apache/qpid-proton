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
from org.apache.qpid.proton import Proton
from org.apache.qpid.proton.amqp import Symbol, UnsignedByte, UnsignedInteger, \
  UnsignedLong, Decimal32, Decimal64, Decimal128
from org.apache.qpid.proton.codec.Data import DataType

from java.util import UUID as JUUID, Date as JDate
from java.nio import ByteBuffer
from jarray import array, zeros

# from proton/codec.h
PN_NULL = 1
PN_BOOL = 2
PN_UBYTE = 3
PN_BYTE = 4
PN_USHORT = 5
PN_SHORT = 6
PN_UINT = 7
PN_INT = 8
PN_CHAR = 9
PN_ULONG = 10
PN_LONG = 11
PN_TIMESTAMP = 12
PN_FLOAT = 13
PN_DOUBLE = 14
PN_DECIMAL32 = 15
PN_DECIMAL64 = 16
PN_DECIMAL128 = 17
PN_UUID = 18
PN_BINARY = 19
PN_STRING = 20
PN_SYMBOL = 21
PN_DESCRIBED = 22
PN_ARRAY = 23
PN_LIST = 24
PN_MAP = 25

DATA_TYPES_J2P = {}
DATA_TYPES_P2J = {}

def DATA_TYPES(jtype, ptype):
  DATA_TYPES_J2P[jtype] = ptype
  DATA_TYPES_P2J[ptype] = jtype

DATA_TYPES(DataType.NULL, PN_NULL)
DATA_TYPES(DataType.BOOL, PN_BOOL)
DATA_TYPES(DataType.UBYTE, PN_UBYTE)
DATA_TYPES(DataType.USHORT, PN_USHORT)
DATA_TYPES(DataType.UINT, PN_UINT)
DATA_TYPES(DataType.ULONG, PN_ULONG)
DATA_TYPES(DataType.SHORT, PN_SHORT)
DATA_TYPES(DataType.INT, PN_INT)
DATA_TYPES(DataType.LONG, PN_LONG)
DATA_TYPES(DataType.CHAR, PN_CHAR)
DATA_TYPES(DataType.TIMESTAMP, PN_TIMESTAMP)
DATA_TYPES(DataType.FLOAT, PN_FLOAT)
DATA_TYPES(DataType.DOUBLE, PN_DOUBLE)
DATA_TYPES(DataType.DECIMAL32, PN_DECIMAL32)
DATA_TYPES(DataType.DECIMAL64, PN_DECIMAL64)
DATA_TYPES(DataType.DECIMAL128, PN_DECIMAL128)
DATA_TYPES(DataType.BINARY, PN_BINARY)
DATA_TYPES(DataType.STRING, PN_STRING)
DATA_TYPES(DataType.SYMBOL, PN_SYMBOL)
DATA_TYPES(DataType.UUID, PN_UUID)
DATA_TYPES(DataType.LIST, PN_LIST)
DATA_TYPES(DataType.MAP, PN_MAP)
DATA_TYPES(DataType.ARRAY, PN_ARRAY)
DATA_TYPES(DataType.DESCRIBED, PN_DESCRIBED)

def pn_data(capacity):
  return Proton.data(capacity)

def pn_data_put_null(data):
  data.putNull()
  return 0

def pn_data_put_bool(data, b):
  data.putBoolean(b)
  return 0

def pn_data_get_bool(data):
  return data.getBoolean()

def pn_data_get_ubyte(data):
  return data.getUnsignedByte().longValue()

def pn_data_put_ubyte(data, u):
  data.putUnsignedByte(UnsignedByte.valueOf(u))
  return 0

def pn_data_get_ushort(data):
  return data.getUnsignedShort().longValue()

def pn_data_put_ushort(data, u):
  data.putUnsignedShort(UnsignedShort.valueOf(u))
  return 0

def pn_data_get_uint(data):
  return data.getUnsignedInteger().longValue()

def pn_data_put_uint(data, u):
  data.putUnsignedInteger(UnsignedInteger.valueOf(u))
  return 0

def pn_data_put_ulong(data, u):
  data.putUnsignedLong(UnsignedLong.valueOf(u))
  return 0

def pn_data_get_ulong(data):
  return data.getUnsignedLong().longValue()

def pn_data_get_short(data):
  return data.getShort()

def pn_data_put_short(data, s):
  data.putShort(s)
  return 0

def pn_data_put_int(data, i):
  data.putInt(i)
  return 0

def pn_data_get_int(data):
  return data.getInt()

def pn_data_put_long(data, l):
  data.putLong(l)
  return 0

def pn_data_get_long(data):
  return data.getLong()

def pn_data_put_char(data, c):
  data.putChar(c)
  return 0

def pn_data_get_char(data):
  return data.getChar()

def pn_data_put_timestamp(data, t):
  data.putTimestamp(JDate(t))
  return 0

def pn_data_get_timestamp(data):
  return data.getTimestamp().getTime()

def pn_data_put_float(data, f):
  data.putFloat(f)
  return 0

def pn_data_get_float(data):
  return data.getFloat()

def pn_data_put_double(data, d):
  data.putDouble(d)
  return 0

def pn_data_get_double(data):
  return data.getDouble()

def pn_data_put_decimal32(data, d):
  data.putDecimal32(Decimal32(d))
  return 0

def pn_data_get_decimal32(data):
  return data.getDecimal32().getBits()

def pn_data_put_decimal64(data, d):
  data.putDecimal64(Decimal64(d))
  return 0

def pn_data_get_decimal64(data):
  return data.getDecimal64().getBits()

def pn_data_put_decimal128(data, d):
  data.putDecimal128(Decimal128(array(d, 'b')))
  return 0

def pn_data_get_decimal128(data):
  return data.getDecimal128().asBytes().tostring()

def pn_data_put_binary(data, b):
  data.putBinary(array(b, 'b'))
  return 0

def pn_data_get_binary(data):
  return data.getBinary().getArray().tostring()

def pn_data_put_string(data, s):
  data.putString(s)
  return 0

def pn_data_get_string(data):
  return data.getString()

def pn_data_put_symbol(data, s):
  data.putSymbol(Symbol.valueOf(s))
  return 0

def pn_data_get_symbol(data):
  return data.getSymbol().toString()

def pn_data_put_uuid(data, u):
  bb = ByteBuffer.wrap(array(u, 'b'))
  first = bb.getLong()
  second = bb.getLong()
  data.putUUID(JUUID(first, second))
  return 0

def pn_data_get_uuid(data):
  u = data.getUUID()
  ba = zeros(16, 'b')
  bb = ByteBuffer.wrap(ba)
  bb.putLong(u.getMostSignificantBits())
  bb.putLong(u.getLeastSignificantBits())
  return ba.tostring()

def pn_data_put_list(data):
  data.putList()
  return 0

def pn_data_get_list(data):
  return data.getList()

def pn_data_put_map(data):
  data.putMap()
  return 0

def pn_data_put_array(data, described, type):
  data.putArray(described, DATA_TYPES_P2J[type])
  return 0

def pn_data_get_array(data):
  return data.getArray()

def pn_data_is_array_described(data):
  return data.isArrayDescribed()

def pn_data_get_array_type(data):
  return DATA_TYPES_J2P[data.getArrayType()]

def pn_data_put_described(data):
  data.putDescribed()
  return 0

def pn_data_rewind(data):
  data.rewind()

def pn_data_next(data):
  t = data.next()
  return t != None

def pn_data_enter(data):
  return data.enter()

def pn_data_exit(data):
  return data.exit()

def pn_data_type(data):
  t = data.type()
  if t is None:
    return -1
  else:
    return DATA_TYPES_J2P[t]

def pn_data_encode(data, size):
  enc = data.encode().getArray().tostring()
  if len(enc) > size:
    return PN_OVERFLOW, None
  else:
    return len(enc), enc

def pn_data_decode(data, encoded):
  return data.decode(ByteBuffer.wrap(array(encoded, 'b')))

def pn_data_narrow(data):
  data.narrow()

def pn_data_widen(data):
  data.widen()

def pn_data_copy(data, src):
  data.copy(src)

def pn_data_format(data, n):
  return 0, data.format()

def pn_data_clear(data):
  data.clear()

def pn_data_free(data):
  pass

def dat2obj(dat):
  dat.rewind()
  if dat.next():
    return dat.getObject()
  else:
    return None

def obj2dat(obj, dat=None):
  if dat is None:
    dat = pn_data(0)
  else:
    dat.clear()
  if obj:
    dat.putObject(obj)
  dat.rewind()
  return dat

def array2dat(ary, atype, dat=None):
  if dat is None:
    dat = pn_data(0)
  else:
    dat.clear()
  if ary:
    pn_data_put_array(dat, False, atype)
    pn_data_enter(dat)
    for o in ary:
      dat.putObject(o)
  dat.rewind()
  return dat
