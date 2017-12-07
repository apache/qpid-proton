#--
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
#++

module Qpid::Proton
  module Types

    # Represents an AMQP Type
    class Type
      private
      @@builtin = {}
      def initialize(code) @code = code; @@builtin[code] = self; end

      public
      def self.try_convert(code) code.is_a?(Type) ? code : @@builtin[code]; end
      def self.[](code) try_convert(code) or raise IndexError, "unknown type code #{code}"; end

      attr_reader :code
      def name() Cproton.pn_type_name(@code); end
      alias to_s name
      def <=>(x) @code <=> x; end
      def hash() @code.hash; end
    end

    # @!group
    NULL       = Type.new(Cproton::PN_NULL)
    BOOL       = Type.new(Cproton::PN_BOOL)
    UBYTE      = Type.new(Cproton::PN_UBYTE)
    BYTE       = Type.new(Cproton::PN_BYTE)
    USHORT     = Type.new(Cproton::PN_USHORT)
    SHORT      = Type.new(Cproton::PN_SHORT)
    UINT       = Type.new(Cproton::PN_UINT)
    INT        = Type.new(Cproton::PN_INT)
    CHAR       = Type.new(Cproton::PN_CHAR)
    ULONG      = Type.new(Cproton::PN_ULONG)
    LONG       = Type.new(Cproton::PN_LONG)
    TIMESTAMP  = Type.new(Cproton::PN_TIMESTAMP)
    FLOAT      = Type.new(Cproton::PN_FLOAT)
    DOUBLE     = Type.new(Cproton::PN_DOUBLE)
    DECIMAL32  = Type.new(Cproton::PN_DECIMAL32)
    DECIMAL64  = Type.new(Cproton::PN_DECIMAL64)
    DECIMAL128 = Type.new(Cproton::PN_DECIMAL128)
    UUID       = Type.new(Cproton::PN_UUID)
    BINARY     = Type.new(Cproton::PN_BINARY)
    STRING     = Type.new(Cproton::PN_STRING)
    SYMBOL     = Type.new(Cproton::PN_SYMBOL)
    DESCRIBED  = Type.new(Cproton::PN_DESCRIBED)
    ARRAY      = Type.new(Cproton::PN_ARRAY)
    LIST       = Type.new(Cproton::PN_LIST)
    MAP        = Type.new(Cproton::PN_MAP)
    #@!endgroup
  end
end
