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



require "spec_helper"

module Qpid

  module Proton

    describe "A data object" do

      before :each do
        @data = Qpid::Proton::Codec::Data.new
      end

      it "can be initialized" do
        @data.wont_be_nil
      end

      it "can hold a null" do
        @data.null = nil
        @data.null?.must_equal(true)
      end

      it "can hold a true boolean" do
        @data.bool = true
        @data.bool.must_equal(true)
      end

      it "can hold a false boolean" do
        @data.bool = false
        @data.bool.must_equal(false)
      end

      it "raises an error on a negative ubyte" do
        proc {
          @data.ubyte = (0 - (rand(127) + 1))
        }.must_raise(RangeError)
      end

      it "raises an error on a null ubyte" do
        proc {
          @data.ubyte = nil
        }.must_raise(TypeError)
      end

      it "can hold an unsigned byte" do
        value = rand(255)
        @data.ubyte = value
        @data.ubyte.must_equal(value)
      end

      it "can hold a byte" do
        value = rand(128)
        @data.byte = value
        @data.byte.must_equal(value)
      end

      it "can hold a negative byte" do
        value = 0 - (rand(126) + 1)
        @data.byte = value
        @data.byte.must_equal(value)
      end

      it "raises an error on a negative ushort" do
        proc {
          @data.ushort = (0 - (rand(65535) + 1))
        }.must_raise(RangeError)
      end

      it "raises an error on a nil ushort" do
        proc {
          @data.ushort = nil
        }.must_raise(TypeError)
      end

      it "can hold a zero unsigned short" do
        @data.ushort = 0
        @data.ushort.must_equal(0)
      end

      it "can hold an unsigned short" do
        value = rand(2**15) + 1
        @data.ushort = value
        @data.ushort.must_equal(value)
      end

      it "raises an error on a nil short" do
        proc {
          @data.short = nil
        }.must_raise(TypeError)
      end

      it "can hold a short" do
        value = rand(2**15) + 1
        @data.short = value
        @data.short.must_equal(value)
      end

      it "can hold a zero short" do
        @data.short = 0
        @data.short.must_equal(0)
      end

      it "can hold a negative short" do
        value = (0 - (rand(2**15) + 1))
        @data.short = value
        @data.short.must_equal(value)
      end

      it "raises an error on a nil uint" do
        proc {
          @data.uint = nil
        }.must_raise(TypeError)
      end

      it "raises an error on a negative uint" do
        proc {
          @data.uint = (0 - (rand(2**32) + 1))
        }.must_raise(RangeError)
      end

      it "can hold an unsigned integer" do
        value = rand(2**32) + 1
        @data.uint = value
        @data.uint.must_equal(value)
      end

      it "can hold a zero unsigned integer" do
        @data.uint = 0
        @data.uint.must_equal(0)
      end

      it "raise an error on a null integer" do
        proc {
          @data.int = nil
        }.must_raise(TypeError)
      end

      it "can hold an integer" do
        value = rand(2**31) + 1
        @data.int = value
        @data.int.must_equal(value)
      end

      it "can hold zero as an integer" do
        @data.int = 0
        @data.int.must_equal(0)
      end

      it "raises an error on a null character" do
        proc {
          @data.char = nil
        }.must_raise(TypeError)
      end

      it "can hold a character" do
        source = random_string(256)
        index = rand(source.length)
        value = source[index,1].bytes.to_a[0]
        @data.char = value
        @data.char.must_equal(value)
      end

      it "raises an error on a null ulong" do
        proc {
          @data.ulong = nil
        }.must_raise(TypeError)
      end

      it "raises an error on a negative ulong" do
        proc {
          @data.ulong = (0 - (rand(2**63) + 1))
        }.must_raise(RangeError)
      end

      it "can have a zero unsigned long" do
        @data.ulong = 0
        @data.ulong.must_equal(0)
      end

      it "can hold an unsigned long" do
        value = rand(2**63) + 1
        @data.ulong = value
        @data.ulong.must_equal(value)
      end

      it "raises an error on a null long" do
        proc {
          @data.long = nil
        }.must_raise(TypeError)
      end

      it "can have a zero long" do
        @data.long = 0
        @data.long.must_equal(0)
      end

      it "can hold a long" do
        value = rand(2**63) + 1
        @data.long = value
        @data.long.must_equal(value)
      end

      it "raise an error on a null timestamp" do
        proc {
          @data.timestamp = nil
        }.must_raise(TypeError)
      end

      it "can handle a negative timestamp" do
        last_year = Time.now - (60*60*24*365)
        @data.timestamp = last_year
        @data.timestamp.must_equal(last_year.to_i)
      end

      it "can handle a zero timestamp" do
        @data.timestamp = 0
        @data.timestamp.must_equal(0)
      end

      it "can hold a timestamp" do
        next_year = Time.now + (60*60*24*365)
        @data.timestamp = next_year
        @data.timestamp.must_equal(next_year.to_i)
      end

      it "raises an error on a null float" do
        proc {
          @data.float = nil
        }.must_raise(TypeError)
      end

      it "can hold a negative float" do
        value = 0.0 - (1.0 + rand(2.0**15)).to_f
        @data.float = value
        @data.float.must_equal(value)
      end

      it "can hold a zero float" do
        @data.float = 0.0
        @data.float.must_equal(0.0)
      end

      it "can hold a float" do
        value = (1.0 + rand(2.0**15)).to_f
        @data.float = value
        @data.float.must_equal(value)
      end

      it "raise an error on a null double" do
        proc {
          @data.double = nil
        }.must_raise(TypeError)
      end

      it "can hold a negative double" do
        value = 0.0 - (1.0 + rand(2.0**31)).to_f
        @data.double = value
        @data.double.must_equal(value)
      end

      it "can hold a zero double" do
        @data.double = 0.0
        @data.double.must_equal(0.0)
      end

      it "can hold a double" do
        value = (1.0 + rand(2.0**31)).to_f
        @data.double = value
        @data.double.must_equal(value)
      end

      it "raises an error on a null decimal32" do
        proc {
          @data.decimal32 = nil
        }.must_raise(TypeError)
      end

      it "can hold a zero decimal32" do
        @data.decimal32 = 0
        @data.decimal32.must_equal(0)
      end

      it "can hold a decimal32" do
        value = 1 + rand(2**31)
        @data.decimal32 = value
        @data.decimal32.must_equal(value)
      end

      it "raises an error on a null decimal64" do
        proc {
          @data.decimal64 = nil
        }.must_raise(TypeError)
      end

      it "can hold a zero decimal64" do
        @data.decimal64 = 0
        @data.decimal64.must_equal(0)
      end

      it "can hold a decimal64" do
        value = 1 + rand(2**63)
        @data.decimal64 = value
        @data.decimal64.must_equal(value)
      end

      it "raises an error on a null decimal128" do
        proc {
          @data.decimal128 = nil
        }.must_raise(TypeError)
      end

      it "can hold a zero decimal128" do
        @data.decimal128 = 0
        @data.decimal128.must_equal(0)
      end

      it "can hold a decimal128" do
        value = rand(2**127)
        @data.decimal128 = value
        @data.decimal128.must_equal(value)
      end

      it "raises an error on a null UUID" do
        proc {
          @data.uuid = nil
        }.must_raise(::ArgumentError)
      end

      it "raises an error on a malformed UUID" do
        proc {
          @data.uuid = random_string(36)
        }.must_raise(::ArgumentError)
      end

      it "can set a UUID from an integer value" do
        @data.uuid = 336307859334295828133695192821923655679
        @data.uuid.must_equal("fd0289a5-8eec-4a08-9283-81d02c9d2fff")
      end

      it "can hold a UUID" do
        value = "fd0289a5-8eec-4a08-9283-81d02c9d2fff"
        @data.uuid = value
        @data.uuid.must_equal(value)
      end

      it "can hold a null binary" do
        @data.binary = nil
        @data.binary.must_equal("")
      end

      it "can hold a binary" do
        value = random_string(128)
        @data.binary = value
        @data.binary.must_equal(value)
      end

      it "can hold a null string" do
        @data.string = nil
        @data.string.must_equal("")
      end

      it "can hold a string" do
        value = random_string(128)
        @data.string = value
        @data.string.must_equal(value)
      end

      it "can hold a null symbol" do
        @data.symbol = nil
        @data.symbol.must_equal(:"")
      end

      it "can hold a symbol" do
        value = random_string(128).to_sym
        @data.symbol = value
        @data.symbol.must_equal(value)
      end

      it "can hold a described value" do
        name = random_string(16).to_sym
        value = random_string(16)
        @data.put_described
        @data.enter
        @data.symbol = name
        @data.string = value
        @data.exit

        @data.described?.must_equal(true)
        @data.enter
        @data.next
        @data.symbol.must_equal(name)
        @data.next
        @data.string.must_equal(value)
      end

      it "raises an error when setting the wrong type in an array" do
        proc {
          @data << Qpid::Proton::Types::UniformArray.new(Qpid::Proton::Types::INT, [1, 2.0, :a, "b"])
        }.must_raise(TypeError)
      end

      it "can hold an array" do
        values = []
        (1..(rand(100) + 5)).each { values << rand(2**16) }
        @data.put_array false, Qpid::Proton::Codec::INT.code
        @data.enter
        values.each { |value| @data.int = value }
        @data.exit

        @data.enter
        values.each do |value|
          @data.next
          @data.int.must_equal(value)
        end
      end

      it "can hold a described array" do
        values = []
        (1..(rand(100) + 5)).each { values << random_string(64) }
        descriptor = random_string(32).to_sym
        @data.put_array true, Qpid::Proton::Codec::STRING.code
        @data.enter
        @data.symbol = descriptor
        values.each { |value| @data.string = value }
        @data.exit

        @data.get_array.must_equal([values.size, true, Qpid::Proton::Codec::STRING.code])
        @data.enter
        @data.next
        @data.symbol.must_equal(descriptor)
        values.each do |value|
          @data.next
          @data.string.must_equal(value)
        end
      end

      it "can hold a list" do
        values = []
        (1..(rand(100) + 5)).each { values << random_string(128) }
        @data.put_list
        @data.enter
        values.each {|value| @data.string = value}
        @data.exit

        @data.enter
        values.each do |value|
          @data.next
          @data.string.must_equal(value)
        end
      end

      it "can hold a map" do
        keys = []
        (1..(rand(100) + 5)).each {keys << random_string(128)}
        values = {}
        keys.each {|key| values[key] = random_string(128)}

        @data.put_map
        @data.enter
        keys.each do |key|
          @data.string = key
          @data.string = values[key]
        end
        @data.exit

        @data.enter
        keys.each do |key|
          @data.next
          @data.string.must_equal(key)
          @data.next
          @data.string.must_equal(values[key])
        end
      end

    end

  end

end
