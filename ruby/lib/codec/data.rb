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

module Qpid::Proton
  # @private
  module Codec

    DataError = ::TypeError

    # @private wrapper for pn_data_t*
    # Raises TypeError for invalid conversions
    class Data

      # @private
      PROTON_METHOD_PREFIX = "pn_data"
      # @private
      include Util::Wrapper

      # @private
      # Convert a pn_data_t* containing a single value to a ruby object.
      # @return [Object, nil] The ruby value extracted from +impl+ or nil if impl is empty
      def self.to_object(impl)
        if (Cproton.pn_data_size(impl) > 0)
          d = Data.new(impl)
          d.rewind
          d.next_object
        end
      end

      # @private
      # Convert a pn_data_t* containing an AMQP "multiple" field to an Array or nil.
      # A "multiple" field can be encoded as an array or a single value - always return Array.
      # @return [Array, nil] The ruby Array extracted from +impl+ or nil if impl is empty
      def self.to_multiple(impl)
        o = self.to_object(impl)
        Array(o) if o
      end

      # @private
      # Clear a pn_data_t* and convert a ruby object into it. If x==nil leave it empty.
      def self.from_object(impl, x)
        d = Data.new(impl)
        d.clear
        d.object = x if x
        nil
      end

      # @overload initialize(capacity)
      #   @param capacity [Integer] capacity for the new data instance.
      # @overload instance(impl)
      #    @param impl [SWIG::pn_data_t*] wrap the C impl pointer.
      def initialize(capacity = 16)
        if capacity.is_a?(Integer)
          @impl = Cproton.pn_data(capacity.to_i)
          @free = true
        else
          # Assume non-integer capacity is a SWIG::pn_data_t*
          @impl = capacity
          @free = false
        end

        # destructor
        ObjectSpace.define_finalizer(self, self.class.finalize!(@impl, @free))
      end

      # @private
      def self.finalize!(impl, free)
        proc {
          Cproton.pn_data_free(impl) if free
        }
      end

      proton_caller :clear, :rewind, :next, :prev, :enter, :exit

      def enter_exit()
        enter
        yield self
      ensure
        exit
      end

      def code() Cproton.pn_data_type(@impl); end

      def type() Mapping.for_code(Cproton.pn_data_type(@impl)); end

      # Returns a representation of the data encoded in AMQP format.
      def encode
        buffer = "\0"*1024
        loop do
          cd = Cproton.pn_data_encode(@impl, buffer, buffer.length)
          if cd == Cproton::PN_OVERFLOW
            buffer *= 2
          elsif cd >= 0
            return buffer[0...cd]
          else
            check(cd)
          end
        end
      end

      # Decodes the first value from supplied AMQP data and returns the number
      # of bytes consumed.
      #
      # @param encoded [String] The encoded data.
      def decode(encoded)
        check(Cproton.pn_data_decode(@impl, encoded, encoded.length))
      end

      proton_is :described, :array_described
      proton_caller :put_described
      proton_caller :put_list, :get_list, :put_map, :get_map
      proton_get :array_type
      proton_caller :put_array
      def get_array() [Cproton.pn_data_get_array(@impl), array_described?, array_type]; end

      def expect(code)
        unless code == self.code
          raise TypeError, "expected #{Cproton.pn_type_name(code)}, got #{Cproton.pn_type_name(self.code)}"
        end
      end

      def described
        expect Cproton::PN_DESCRIBED
        enter_exit { Types::Described.new(self.next_object, self.next_object) }
      end

      def described= d
        put_described
        enter_exit { self << d.descriptor << d.value }
      end

      def fill(a, count, what)
        a << self.object while self.next
        raise TypeError, "#{what} expected #{count} elements, got #{a.size}" unless a.size == count
        a
      end

      def list
        return array if code == Cproton::PN_ARRAY
        expect Cproton::PN_LIST
        count = get_list
        a = []
        enter_exit { fill(a, count, __method__) }
      end

      def list=(a)
        put_list
        enter_exit { a.each { |x| self << x } }
      end

      def array
        return list if code == Cproton::PN_LIST
        expect Cproton::PN_ARRAY
        count, d, t = get_array
        enter_exit do
          desc = next_object if d
          a = Types::UniformArray.new(t, nil, desc)
          fill(a, count, "array")
        end
      end

      def array=(a)
        t = a.type if a.respond_to? :type
        d = a.descriptor if a.respond_to? :descriptor
        if (h = a.instance_variable_get(:@proton_array_header))
          t ||= h.type
          d ||= h.descriptor
        end
        raise TypeError, "no type when converting #{a.class} to an array" unless t
        put_array(!d.nil?, t.code)
        m = Mapping[t]
        enter_exit do
          self << d unless d.nil?
          a.each { |e| m.put(self, e); }
        end
      end

      def map
        expect Cproton::PN_MAP
        count = self.get_map
        raise TypeError, "invalid map, total of keys and values is odd" if count.odd?
        enter_exit do
          m = {}
          m[object] = next_object while self.next
          raise TypeError, "map expected #{count/2} entries, got #{m.size}" unless m.size == count/2
          m
        end
      end

      def map= m
        put_map
        enter_exit { m.each_pair { |k,v| self << k << v } }
      end

      # Return nil if vallue is null, raise exception otherwise.
      def null() raise TypeError, "expected null, got #{type || 'empty'}" unless null?; end

      # Set the current value to null
      def null=(dummy=nil) check(Cproton.pn_data_put_null(@impl)); end

      # Puts an arbitrary object type.
      #
      # The Data instance will determine which AMQP type is appropriate and will
      # use that to encode the object.
      #
      # @param object [Object] The value.
      #
      def object=(object)
        Mapping.for_class(object.class).put(self, object)
        object
      end

      # Add an arbitrary data value using object=, return self
      def <<(x) self.object=x; self; end

      # Move forward to the next value and return it
      def next_object
        self.next or raise TypeError, "not enough data"
        self.object
      end

      # Gets the current node, based on how it was encoded.
      #
      # @return [Object] The current node.
      #
      def object
        self.type.get(self) if self.type
      end

      # Checks if the current node is null.
      #
      # @return [Boolean] True if the node is null.
      #
      def null?
        Cproton.pn_data_is_null(@impl)
      end

      # Puts a boolean value.
      #
      # @param value [Boolean] The boolean value.
      #
      def bool=(value)
        check(Cproton.pn_data_put_bool(@impl, value))
      end

      # If the current node is a boolean, then it returns the value. Otherwise,
      # it returns false.
      #
      # @return [Boolean] The boolean value.
      #
      def bool
        Cproton.pn_data_get_bool(@impl)
      end

      # Puts an unsigned byte value.
      #
      # @param value [Integer] The unsigned byte value.
      #
      def ubyte=(value)
        check(Cproton.pn_data_put_ubyte(@impl, value))
      end

      # If the current node is an unsigned byte, returns its value. Otherwise,
      # it returns 0.
      #
      # @return [Integer] The unsigned byte value.
      #
      def ubyte
        Cproton.pn_data_get_ubyte(@impl)
      end

      # Puts a byte value.
      #
      # @param value [Integer] The byte value.
      #
      def byte=(value)
        check(Cproton.pn_data_put_byte(@impl, value))
      end

      # If the current node is an byte, returns its value. Otherwise,
      # it returns 0.
      #
      # @return [Integer] The byte value.
      #
      def byte
        Cproton.pn_data_get_byte(@impl)
      end

      # Puts an unsigned short value.
      #
      # @param value [Integer] The unsigned short value
      #
      def ushort=(value)
        check(Cproton.pn_data_put_ushort(@impl, value))
      end

      # If the current node is an unsigned short, returns its value. Otherwise,
      # it returns 0.
      #
      # @return [Integer] The unsigned short value.
      #
      def ushort
        Cproton.pn_data_get_ushort(@impl)
      end

      # Puts a short value.
      #
      # @param value [Integer] The short value.
      #
      def short=(value)
        check(Cproton.pn_data_put_short(@impl, value))
      end

      # If the current node is a short, returns its value. Otherwise,
      # returns a 0.
      #
      # @return [Integer] The short value.
      #
      def short
        Cproton.pn_data_get_short(@impl)
      end

      # Puts an unsigned integer value.
      #
      # @param value [Integer] the unsigned integer value
      #
      def uint=(value)
        raise TypeError if value.nil?
        raise RangeError, "invalid uint: #{value}" if value < 0
        check(Cproton.pn_data_put_uint(@impl, value))
      end

      # If the current node is an unsigned int, returns its value. Otherwise,
      # returns 0.
      #
      # @return [Integer] The unsigned integer value.
      #
      def uint
        Cproton.pn_data_get_uint(@impl)
      end

      # Puts an integer value.
      #
      # ==== Options
      #
      # * value - the integer value
      def int=(value)
        check(Cproton.pn_data_put_int(@impl, value))
      end

      # If the current node is an integer, returns its value. Otherwise,
      # returns 0.
      #
      # @return [Integer] The integer value.
      #
      def int
        Cproton.pn_data_get_int(@impl)
      end

      # Puts a character value.
      #
      # @param value [Integer] The character value.
      #
      def char=(value)
        check(Cproton.pn_data_put_char(@impl, value))
      end

      # If the current node is a character, returns its value. Otherwise,
      # returns 0.
      #
      # @return [Integer] The character value.
      #
      def char
        Cproton.pn_data_get_char(@impl)
      end

      # Puts an unsigned long value.
      #
      # @param value [Integer] The unsigned long value.
      #
      def ulong=(value)
        raise TypeError if value.nil?
        raise RangeError, "invalid ulong: #{value}" if value < 0
        check(Cproton.pn_data_put_ulong(@impl, value))
      end

      # If the current node is an unsigned long, returns its value. Otherwise,
      # returns 0.
      #
      # @return [Integer] The unsigned long value.
      #
      def ulong
        Cproton.pn_data_get_ulong(@impl)
      end

      # Puts a long value.
      #
      # @param value [Integer] The long value.
      #
      def long=(value)
        check(Cproton.pn_data_put_long(@impl, value))
      end

      # If the current node is a long, returns its value. Otherwise, returns 0.
      #
      # @return [Integer] The long value.
      def long
        Cproton.pn_data_get_long(@impl)
      end

      # Puts a timestamp value.
      #
      # @param value [Integer] The timestamp value.
      #
      def timestamp=(value)
        value = value.to_i if (!value.nil? && value.is_a?(Time))
        check(Cproton.pn_data_put_timestamp(@impl, value))
      end

      # If the current node is a timestamp, returns its value. Otherwise,
      # returns 0.
      #
      # @return [Integer] The timestamp value.
      #
      def timestamp
        Cproton.pn_data_get_timestamp(@impl)
      end

      # Puts a float value.
      #
      # @param value [Float] The floating point value.
      #
      def float=(value)
        check(Cproton.pn_data_put_float(@impl, value))
      end

      # If the current node is a float, returns its value. Otherwise,
      # returns 0.
      #
      # @return [Float] The floating point value.
      #
      def float
        Cproton.pn_data_get_float(@impl)
      end

      # Puts a double value.
      #
      # @param value [Float] The double precision floating point value.
      #
      def double=(value)
        check(Cproton.pn_data_put_double(@impl, value))
      end

      # If the current node is a double, returns its value. Otherwise,
      # returns 0.
      #
      # @return [Float] The double precision floating point value.
      #
      def double
        Cproton.pn_data_get_double(@impl)
      end

      # Puts a decimal32 value.
      #
      # @param value [Integer] The decimal32 value.
      #
      def decimal32=(value)
        check(Cproton.pn_data_put_decimal32(@impl, value))
      end

      # If the current node is a decimal32, returns its value. Otherwise,
      # returns 0.
      #
      # @return [Integer] The decimal32 value.
      #
      def decimal32
        Cproton.pn_data_get_decimal32(@impl)
      end

      # Puts a decimal64 value.
      #
      # @param value [Integer] The decimal64 value.
      #
      def decimal64=(value)
        check(Cproton.pn_data_put_decimal64(@impl, value))
      end

      # If the current node is a decimal64, returns its value. Otherwise,
      # it returns 0.
      #
      # @return [Integer] The decimal64 value.
      #
      def decimal64
        Cproton.pn_data_get_decimal64(@impl)
      end

      # Puts a decimal128 value.
      #
      # @param value [Integer] The decimal128 value.
      #
      def decimal128=(value)
        raise TypeError, "invalid decimal128 value: #{value}" if value.nil?
        value = value.to_s(16).rjust(32, "0")
        bytes = []
        value.scan(/(..)/) {|v| bytes << v[0].to_i(16)}
        check(Cproton.pn_data_put_decimal128(@impl, bytes))
      end

      # If the current node is a decimal128, returns its value. Otherwise,
      # returns 0.
      #
      # @return [Integer] The decimal128 value.
      #
      def decimal128
        value = ""
        Cproton.pn_data_get_decimal128(@impl).each{|val| value += ("%02x" % val)}
        value.to_i(16)
      end

      # Puts a +UUID+ value.
      #
      # The UUID is expected to be in the format of a string or else a 128-bit
      # integer value.
      #
      # @param value [String, Numeric] A string or numeric representation of the UUID.
      #
      # @example
      #
      #   # set a uuid value from a string value
      #   require 'securerandom'
      #   @impl.uuid = SecureRandom.uuid
      #
      #   # or
      #   @impl.uuid = "fd0289a5-8eec-4a08-9283-81d02c9d2fff"
      #
      #   # set a uuid value from a 128-bit value
      #   @impl.uuid = 0 # sets to 00000000-0000-0000-0000-000000000000
      #
      def uuid=(value)
        raise ::ArgumentError, "invalid uuid: #{value}" if value.nil?

        # if the uuid that was submitted was numeric value, then translated
        # it into a hex string, otherwise assume it was a string represtation
        # and attempt to decode it
        if value.is_a? Numeric
          value = "%032x" % value
        else
          raise ::ArgumentError, "invalid uuid: #{value}" if !valid_uuid?(value)

          value = (value[0, 8]  +
                   value[9, 4]  +
                   value[14, 4] +
                   value[19, 4] +
                   value[24, 12])
        end
        bytes = []
        value.scan(/(..)/) {|v| bytes << v[0].to_i(16)}
        check(Cproton.pn_data_put_uuid(@impl, bytes))
      end

      # If the current value is a +UUID+, returns its value. Otherwise,
      # it returns nil.
      #
      # @return [String] The string representation of the UUID.
      #
      def uuid
        value = ""
        Cproton.pn_data_get_uuid(@impl).each{|val| value += ("%02x" % val)}
        value.insert(8, "-").insert(13, "-").insert(18, "-").insert(23, "-")
      end

      # Puts a binary value.
      #
      # A binary string is encoded as an ASCII 8-bit string value. This is in
      # contranst to other strings, which are treated as UTF-8 encoded.
      #
      # @param value [String] An arbitrary string value.
      #
      # @see #string=
      #
      def binary=(value)
        check(Cproton.pn_data_put_binary(@impl, value))
      end

      # If the current node is binary, returns its value. Otherwise, it returns
      # an empty string ("").
      #
      # @return [String] The binary string.
      #
      # @see #string
      #
      def binary
        Qpid::Proton::Types::BinaryString.new(Cproton.pn_data_get_binary(@impl))
      end

      # Puts a UTF-8 encoded string value.
      #
      # *NOTE:* A nil value is stored as an empty string rather than as a nil.
      #
      # @param value [String] The UTF-8 encoded string value.
      #
      # @see #binary=
      #
      def string=(value)
        check(Cproton.pn_data_put_string(@impl, value))
      end

      # If the current node is a string, returns its value. Otherwise, it
      # returns an empty string ("").
      #
      # @return [String] The UTF-8 encoded string.
      #
      # @see #binary
      #
      def string
        Qpid::Proton::Types::UTFString.new(Cproton.pn_data_get_string(@impl))
      end

      # Puts a symbolic value.
      #
      # @param value [String|Symbol] The symbolic string value.
      #
      def symbol=(value)
        check(Cproton.pn_data_put_symbol(@impl, value.to_s))
      end

      # If the current node is a symbol, returns its value. Otherwise, it
      # returns an empty string ("").
      #
      # @return [Symbol] The symbol value.
      #
      def symbol
        Cproton.pn_data_get_symbol(@impl).to_sym
      end

      # Get the current value as a single object.
      #
      # @return [Object] The current node's object.
      #
      # @see #type_code
      # @see #type
      #
      def get
        type.get(self);
      end

      # Puts a new value with the given type into the current node.
      #
      # @param value [Object] The value.
      # @param type_code [Mapping] The value's type.
      #
      # @private
      #
      def put(value, type_code);
        type_code.put(self, value);
      end

      private

      def valid_uuid?(value)
        # ensure that the UUID is in the right format
        # xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx
        value =~ /[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}/
      end

      # @private
      def check(err)
        if err < 0
          raise TypeError, "[#{err}]: #{Cproton.pn_data_error(@impl)}"
        else
          return err
        end
      end
    end
  end
end
