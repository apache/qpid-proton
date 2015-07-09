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

require "spec_helper"

module Qpid

  module Proton

    describe "A data object" do

      before :each do
        @data = Qpid::Proton::Codec::Data.new
      end

      it "can be initialized" do
        expect(@data).not_to be_nil
      end

      it "can hold a null" do
        @data.null
        expect(@data.null?).to eq(true)
      end

      it "can hold a true boolean" do
        @data.bool = true
        expect(@data.bool).to eq(true)
      end

      it "can hold a false boolean" do
        @data.bool = false
        expect(@data.bool).to eq(false)
      end

      it "raises an error on a negative ubyte" do
        expect {
          @data.ubyte = (0 - (rand(127) + 1))
        }.to raise_error(RangeError)
      end

      it "raises an error on a null ubyte" do
        expect {
          @data.ubyte = nil
        }.to raise_error(TypeError)
      end

      it "can hold an unsigned byte" do
        value = rand(255)
        @data.ubyte = value
        expect(@data.ubyte).to eq(value)
      end

      it "can hold a byte" do
        value = rand(128)
        @data.byte = value
        expect(@data.byte).to eq(value)
      end

      it "can hold a negative byte" do
        value = 0 - (rand(126) + 1)
        @data.byte = value
        expect(@data.byte).to eq(value)
      end

      it "raises an error on a negative ushort" do
        expect {
          @data.ushort = (0 - (rand(65535) + 1))
        }.to raise_error(RangeError)
      end

      it "raises an error on a nil ushort" do
        expect {
          @data.ushort = nil
        }.to raise_error(TypeError)
      end

      it "can hold a zero unsigned short" do
        @data.ushort = 0
        expect(@data.ushort).to eq(0)
      end

      it "can hold an unsigned short" do
        value = rand(2**15) + 1
        @data.ushort = value
        expect(@data.ushort).to eq(value)
      end

      it "raises an error on a nil short" do
        expect {
          @data.short = nil
        }.to raise_error(TypeError)
      end

      it "can hold a short" do
        value = rand(2**15) + 1
        @data.short = value
        expect(@data.short).to eq(value)
      end

      it "can hold a zero short" do
        @data.short = 0
        expect(@data.short).to eq(0)
      end

      it "can hold a negative short" do
        value = (0 - (rand(2**15) + 1))
        @data.short = value
        expect(@data.short).to eq(value)
      end

      it "raises an error on a nil uint" do
        expect {
          @data.uint = nil
        }.to raise_error(TypeError)
      end

      it "raises an error on a negative uint" do
        expect {
          @data.uint = (0 - (rand(2**32) + 1))
        }.to raise_error(RangeError)
      end

      it "can hold an unsigned integer" do
        value = rand(2**32) + 1
        @data.uint = value
        expect(@data.uint).to eq(value)
      end

      it "can hold a zero unsigned integer" do
        @data.uint = 0
        expect(@data.uint).to eq(0)
      end

      it "raise an error on a null integer" do
        expect {
          @data.int = nil
        }.to raise_error(TypeError)
      end

      it "can hold an integer" do
        value = rand(2**31) + 1
        @data.int = value
        expect(@data.int).to eq(value)
      end

      it "can hold zero as an integer" do
        @data.int = 0
        expect(@data.int).to eq(0)
      end

      it "raises an error on a null character" do
        expect {
          @data.char = nil
        }.to raise_error(TypeError)
      end

      it "can hold a character" do
        source = random_string(256)
        index = rand(source.length)
        value = source[index,1].bytes.to_a[0]
        @data.char = value
        expect(@data.char).to eq(value)
      end

      it "raises an error on a null ulong" do
        expect {
          @data.ulong = nil
        }.to raise_error(TypeError)
      end

      it "raises an error on a negative ulong" do
        expect {
          @data.ulong = (0 - (rand(2**63) + 1))
        }.to raise_error(RangeError)
      end

      it "can have a zero unsigned long" do
        @data.ulong = 0
        expect(@data.ulong).to eq(0)
      end

      it "can hold an unsigned long" do
        value = rand(2**63) + 1
        @data.ulong = value
        expect(@data.ulong).to eq(value)
      end

      it "raises an error on a null long" do
        expect {
          @data.long = nil
        }.to raise_error(TypeError)
      end

      it "can have a zero long" do
        @data.long = 0
        expect(@data.long).to eq(0)
      end

      it "can hold a long" do
        value = rand(2**63) + 1
        @data.long = value
        expect(@data.long).to eq(value)
      end

      it "raise an error on a null timestamp" do
        expect {
          @data.timestamp = nil
        }.to raise_error(TypeError)
      end

      it "can handle a negative timestamp" do
        last_year = Time.now - (60*60*24*365)
        @data.timestamp = last_year
        expect(@data.timestamp).to eq(last_year.to_i)
      end

      it "can handle a zero timestamp" do
        @data.timestamp = 0
        expect(@data.timestamp).to eq(0)
      end

      it "can hold a timestamp" do
        next_year = Time.now + (60*60*24*365)
        @data.timestamp = next_year
        expect(@data.timestamp).to eq(next_year.to_i)
      end

      it "raises an error on a null float" do
        expect {
          @data.float = nil
        }.to raise_error(TypeError)
      end

      it "can hold a negative float" do
        value = 0.0 - (1.0 + rand(2.0**15)).to_f
        @data.float = value
        expect(@data.float).to eq(value)
      end

      it "can hold a zero float" do
        @data.float = 0.0
        expect(@data.float).to eq(0.0)
      end

      it "can hold a float" do
        value = (1.0 + rand(2.0**15)).to_f
        @data.float = value
        expect(@data.float).to eq(value)
      end

      it "raise an error on a null double" do
        expect {
          @data.double = nil
        }.to raise_error(TypeError)
      end

      it "can hold a negative double" do
        value = 0.0 - (1.0 + rand(2.0**31)).to_f
        @data.double = value
        expect(@data.double).to eq(value)
      end

      it "can hold a zero double" do
        @data.double = 0.0
        expect(@data.double).to eq(0.0)
      end

      it "can hold a double" do
        value = (1.0 + rand(2.0**31)).to_f
        @data.double = value
        expect(@data.double).to eq(value)
      end

      it "raises an error on a null decimal32" do
        expect {
          @data.decimal32 = nil
        }.to raise_error(TypeError)
      end

      it "can hold a zero decimal32" do
        @data.decimal32 = 0
        expect(@data.decimal32).to eq(0)
      end

      it "can hold a decimal32" do
        value = 1 + rand(2**31)
        @data.decimal32 = value
        expect(@data.decimal32).to eq(value)
      end

      it "raises an error on a null decimal64" do
        expect {
          @data.decimal64 = nil
        }.to raise_error(TypeError)
      end

      it "can hold a zero decimal64" do
        @data.decimal64 = 0
        expect(@data.decimal64).to eq(0)
      end

      it "can hold a decimal64" do
        value = 1 + rand(2**63)
        @data.decimal64 = value
        expect(@data.decimal64).to eq(value)
      end

      it "raises an error on a null decimal128" do
        expect {
          @data.decimal128 = nil
        }.to raise_error(TypeError)
      end

      it "can hold a zero decimal128" do
        @data.decimal128 = 0
        expect(@data.decimal128).to eq(0)
      end

      it "can hold a decimal128" do
        value = rand(2**127)
        @data.decimal128 = value
        expect(@data.decimal128).to eq(value)
      end

      it "raises an error on a null UUID" do
        expect {
          @data.uuid = nil
        }.to raise_error(::ArgumentError)
      end

      it "raises an error on a malformed UUID" do
        expect {
          @data.uuid = random_string(36)
        }.to raise_error(::ArgumentError)
      end

      it "can set a UUID from an integer value" do
        @data.uuid = 336307859334295828133695192821923655679
        expect(@data.uuid).to eq("fd0289a5-8eec-4a08-9283-81d02c9d2fff")
      end

      it "can hold a UUID" do
        value = "fd0289a5-8eec-4a08-9283-81d02c9d2fff"
        @data.uuid = value
        expect(@data.uuid).to eq(value)
      end

      it "can hold a null binary" do
        @data.binary = nil
        expect(@data.binary).to eq("")
      end

      it "can hold a binary" do
        value = random_string(128)
        @data.binary = value
        expect(@data.binary).to eq(value)
      end

      it "can hold a null string" do
        @data.string = nil
        expect(@data.string).to eq("")
      end

      it "can hold a string" do
        value = random_string(128)
        @data.string = value
        expect(@data.string).to eq(value)
      end

      it "can hold a null symbol" do
        @data.symbol = nil
        expect(@data.symbol).to eq("")
      end

      it "can hold a symbol" do
        value = random_string(128)
        @data.symbol = value
        expect(@data.symbol).to eq(value)
      end

      it "can hold a described value" do
        name = random_string(16)
        value = random_string(16)
        @data.put_described
        @data.enter
        @data.symbol = name
        @data.string = value
        @data.exit

        expect(@data.described?).to eq(true)
        @data.enter
        @data.next
        expect(@data.symbol).to eq(name)
        @data.next
        expect(@data.string).to eq(value)
      end

      it "raises an error when setting the wrong type in an array"

      it "can hold an array" do
        values = []
        (1..(rand(100) + 5)).each { values << rand(2**16) }
        @data.put_array false, Qpid::Proton::Codec::INT
        @data.enter
        values.each { |value| @data.int = value }
        @data.exit

        @data.enter
        values.each do |value|
          @data.next
          expect(@data.int).to eq(value)
        end
      end

      it "can hold a described array" do
        values = []
        (1..(rand(100) + 5)).each { values << random_string(64) }
        descriptor = random_string(32)
        @data.put_array true, Qpid::Proton::Codec::STRING
        @data.enter
        @data.symbol = descriptor
        values.each { |value| @data.string = value }
        @data.exit

        expect(@data.array).to match_array([values.size, true, Qpid::Proton::Codec::STRING])
        @data.enter
        @data.next
        expect(@data.symbol).to eq(descriptor)
        values.each do |value|
          @data.next
          expect(@data.string).to eq(value)
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
          expect(@data.string).to eq(value)
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
          expect(@data.string).to eq(key)
          @data.next
          expect(@data.string).to eq(values[key])
        end
      end

    end

  end

end
