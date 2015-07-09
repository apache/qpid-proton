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

    describe "A message" do

      before (:each) do
        @message = Qpid::Proton::Message.new
      end

      it "can be created" do
        expect(@message).not_to be_nil
      end

      it "can be cleared" do
        subject = random_string(16)
        @message.subject = subject
        expect(@message.subject).to eq(subject)
        @message.clear
        expect(@message.subject).not_to eq(subject)
      end

      it "can be durable" do
        @message.durable = true
        expect(@message.durable).to eq(true)
        @message.durable = false
        expect(@message.durable).to eq(false)
      end

      it "raises an error when setting durable to nil" do
        expect {
          @message.durable = nil
        }.to raise_error(TypeError)
      end

      it "raises an error when setting the priority to nil" do
        expect {
          @message.priority = nil
        }.to raise_error(TypeError)
      end

      it "raises an error when setting the priority to a non-number" do
        expect {
          @message.priority = "abck"
        }.to raise_error(TypeError)
      end

      it "sets the priority to the integer portion when a float" do
        priority = rand(100) / 10
        @message.priority = priority
        expect(@message.priority).to eq(priority.floor)
      end

      it "rejects a priority with too large of a value" do
        expect {
          @message.priority = (rand(100) + 256)
        }.to raise_error(RangeError)
      end

      it "rejects a negative priority" do
        expect {
          @message.priority = (0 - (rand(255) + 1))
        }.to raise_error(RangeError)
      end

      it "has a priority" do
        priority = rand(256)
        @message.priority = priority
        expect(@message.priority).to eq(priority)
      end

      it "raises an error when setting the time-to-live to nil" do
        expect {
          @message.ttl = nil
        }.to raise_error(TypeError)
      end

      it "raises an error when setting the time-to-live to a non-number" do
        expect {
          @message.ttl = random_string(5)
        }.to raise_error(TypeError)
      end

      it "sets the time-to-live to the integer portion when a float" do
        ttl = (rand(32767) / 10)
        @message.ttl = ttl
        expect(@message.ttl).to eq(ttl.floor)
      end

      it "raises an error when the time-to-live is negative" do
        expect {
          @message.ttl = (0 - rand(1000))
        }.to raise_error(RangeError)
      end

      it "has a time-to-live" do
        ttl = rand(32767)
        @message.ttl = ttl
        expect(@message.ttl).to eq(ttl)
      end

      it "raises an error when setting first acquirer to nil" do
        expect {
          @message.first_acquirer = nil
        }.to raise_error(TypeError)
      end

      it "raises and error when setting first acquirer to a non-boolean" do
        expect {
          @message.first_acquirer = random_string(16)
        }.to raise_error(TypeError)
      end

      it "has a first acquirer" do
        @message.first_acquirer = true
        expect(@message.first_acquirer?).to eq(true)

        @message.first_acquirer = false
        expect(@message.first_acquirer?).to eq(false)
      end

      it "raises an error on a nil delivery count" do
        expect {
          @message.delivery_count = nil
        }.to raise_error(::ArgumentError)
      end

      it "raises an error on a negative delivery count" do
        expect {
          @message.delivery_count = -1
        }.to raise_error(RangeError)
      end

      it "raises an error on a non-numeric delivery count" do
        expect {
          @message.delivery_count = "farkle"
        }.to raise_error(::ArgumentError)
      end

      it "converts a floating point delivery count to its integer portion" do
          count = rand(255) / 10.0
          @message.delivery_count = count
          expect(@message.delivery_count).to eq(count.floor)
        end

      it "has a delivery count" do
        count = rand(255)
        @message.delivery_count = count
        expect(@message.delivery_count).to eq(count)
      end

      it "allows setting a nil id" do
        @message.id = nil
        expect(@message.id).to be_nil
      end

      it "has an id" do
        id = random_string(16)
        @message.id = id
        expect(@message.id).to eq(id)
      end

      it "allows setting a nil user id" do
        @message.user_id = nil
        expect(@message.user_id).to eq("")
      end

      it "has a user id" do
        id = random_string(16)
        @message.user_id = id
        expect(@message.user_id).to eq(id)
      end

      it "allows setting a nil address" do
        @message.address = nil
        expect(@message.address).to be_nil
      end

      it "has an address" do
        address = "//0.0.0.0/#{random_string(16)}"
        @message.address = address
        expect(@message.address).to eq(address)
      end

      it "allows setting a nil subject" do
        @message.subject = nil
        expect(@message.subject).to be_nil
      end

      it "has a subject" do
        subject = random_string(50)
        @message.subject = subject
        expect(@message.subject).to eq(subject)
      end

      it "will allow a nil reply-to address" do
        @message.reply_to = nil
        expect(@message.reply_to).to be_nil
      end

      it "has a reply-to address" do
        address = "//0.0.0.0/#{random_string(16)}"
        @message.reply_to = address
        expect(@message.reply_to).to eq(address)
      end

      it "will allow a nil correlation id" do
        @message.correlation_id = nil
        expect(@message.correlation_id).to be_nil
      end

      it "has a correlation id" do
        id = random_string(25)
        @message.correlation_id = id
        expect(@message.correlation_id).to eq(id)
      end

      it "will allow a nil content type" do
        @message.content_type = nil
        expect(@message.content_type).to be_nil
      end

      it "will allow an empty content type" do
        @message.content_type = ""
        expect(@message.content_type).to eq("")
      end

      it "has a content type" do
        content_type = random_string(32)
        @message.content_type = content_type
        expect(@message.content_type).to eq(content_type)
      end

      it "can have nil content encoding" do
        @message.content_encoding = nil
        expect(@message.content_encoding).to be_nil
      end

      it "has a content encoding" do
        encoding = "#{random_string(8)}/#{random_string(8)}"
        @message.content_encoding = encoding
        expect(@message.content_encoding).to eq(encoding)
      end

      it "raises an error on a nil expiry time" do
        expect {
          @message.expires = nil
        }.to raise_error(TypeError)
      end

      it "raises an error on a negative expiry time" do
        expect {
          @message.expires = (0-(rand(65535)))
        }.to raise_error(::ArgumentError)
      end

      it "can have a zero expiry time" do
        @message.expires = 0
        expect(@message.expires).to equal(0)
      end

      it "has an expiry time" do
        time = rand(65535)
        @message.expires = time
        expect(@message.expires).to eq(time)
      end

      it "raises an error on a nil creation time" do
        expect {
          @message.creation_time = nil
        }.to raise_error(TypeError)
      end

      it "raises an error on a negative creation time" do
        expect {
          @message.creation_time = (0 - rand(65535))
        }.to raise_error(::ArgumentError)
      end

      it "can have a zero creation time" do
        @message.creation_time = 0
        expect(@message.creation_time).to eq(0)
      end

      it "has a creation time" do
        time = rand(65535)
        @message.creation_time = time
        expect(@message.creation_time).to eq(time)
      end

      it "can have a nil group id" do
        @message.group_id = nil
        expect(@message.group_id).to be_nil
      end

      it "can have an empty group id" do
        @message.group_id = ""
        expect(@message.group_id).to eq("")
      end

      it "has a group id" do
        id = random_string(16)
        @message.group_id = id
        expect(@message.group_id).to eq(id)
      end


      it "raises an error on a nil group sequence" do
        expect {
          @message.group_sequence = nil
        }.to raise_error(TypeError)
      end

      it "can have a negative group sequence" do
        seq = (0 - rand(32767))
        @message.group_sequence = seq
        expect(@message.group_sequence).to eq(seq)
      end

      it "can have a zero group sequence" do
        @message.group_sequence = 0
        expect(@message.group_sequence).to eq(0)
      end

      it "has a group sequence" do
        id = rand(32767)
        @message.group_sequence = id
        expect(@message.group_sequence).to eq(id)
      end

      it "can have a nil reply-to group id" do
        @message.reply_to_group_id = nil
        expect(@message.reply_to_group_id).to be_nil
      end

      it "can have an empty reply-to group id" do
        @message.reply_to_group_id = ""
        expect(@message.reply_to_group_id).to eq("")
      end

      it "has a reply-to group id" do
        id = random_string(16)
        @message.reply_to_group_id = id
        expect(@message.reply_to_group_id).to eq(id)
      end

      it "has properties" do
        expect(@message).to respond_to(:properties)
        expect(@message).to respond_to(:properties=)
        expect(@message).to respond_to(:[])
        expect(@message).to respond_to(:[]=)

        expect(@message.properties).to be_kind_of({}.class)
      end

      it "can replace the set of properties" do
        values = random_hash(128)

        @message.properties = values.clone
        expect(@message.properties).to eq(values)
      end

      it "can set properties" do
        name = random_string(16)
        value = random_string(128)

        @message[name] = value
        expect(@message[name]).to eq(value)
      end

      it "can update properties" do
        name = random_string(16)
        value = random_string(128)

        @message[name] = value
        expect(@message[name]).to eq(value)

        value = random_string(128)
        @message[name] = value
        expect(@message[name]).to eq(value)
      end

      it "can hold a null property" do
        name = random_string(16)
        value = random_string(128)

        @message[name] = value
        expect(@message[name]).to eq(value)

        @message[name] = nil
        expect(@message[name]).to be_nil
      end

      it "can delete a property" do
        name = random_string(16)
        value = random_string(128)

        @message[name] = value
        expect(@message[name]).to eq(value)

        @message.delete_property(name)
        expect(@message.properties.keys).to_not include(name)
      end

      it "has no properties after being cleared" do
        name = random_string(16)
        value = random_string(128)

        @message[name] = value
        expect(@message[name]).to eq(value)

        @message.clear
        expect(@message.properties).to be_empty
      end

      it "has instructions" do
        expect(@message).to respond_to(:instructions)
        expect(@message).to respond_to("instructions=".to_sym)
      end

      it "can set an instruction" do
        name = random_string(16)
        value = random_string(128)

        @message.instructions[name] = value
        expect(@message.instructions[name]).to eq(value)
      end

      it "can update an instruction" do
        name = random_string(16)
        value = random_string(128)

        @message.instructions[name] = value
        expect(@message.instructions[name]).to eq(value)

        value = random_string(128)
        @message.instructions[name] = value
        expect(@message.instructions[name]).to eq(value)
      end

      it "can delete the instructions" do
        name = random_string(16)
        value = random_string(128)

        @message.instructions[name] = value
        expect(@message.instructions).to_not be_empty

        @message.instructions = nil
        expect(@message.instructions).to be_nil
      end

      it "can replace the instructions" do
        values = random_hash(rand(128) + 1)

        @message.instructions = values.clone
        expect(@message.instructions).to eq(values)

        values = random_hash(rand(64) + 1)

        @message.instructions = values.clone
        expect(@message.instructions).to eq(values)
      end

      it "can delete the set of instructions" do
        values = random_hash(rand(128) + 1)

        @message.instructions = values.clone
        expect(@message.instructions).to eq(values)

        @message.instructions = nil
        expect(@message.instructions).to be_nil
      end

      it "has no instructions after being cleared" do
        value = random_hash(128)

        @message.instructions = value.clone
        expect(@message.instructions).to eq(value)

         @message.clear
        expect(@message.instructions).to be_empty
      end

      it "has annotations" do
        expect(@message).to respond_to(:annotations)
        expect(@message).to respond_to(:annotations=)
      end

      it "can set an annotation" do
        name = random_hash(32)
        value = random_hash(256)

        @message.annotations[name] = value.clone
        expect(@message.annotations[name]).to eq(value)
      end

      it "can update an annotation" do
        name = random_hash(32)
        value = random_hash(256)

        @message.annotations[name] = value.clone
        expect(@message.annotations[name]).to eq(value)

        value = random_hash(128)

        @message.annotations[name] = value.clone
        expect(@message.annotations[name]).to eq(value)
      end

      it "can delete an annotation" do
        name = random_hash(32)
        value = random_hash(256)

        @message.annotations[name] = value.clone
        expect(@message.annotations[name]).to eq(value)

        @message.annotations[name] = nil
        expect(@message.annotations[name]).to be_nil
      end

      it "can replace all annotations" do
        values = random_hash(rand(128) + 1)

        @message.annotations = values.clone
        expect(@message.annotations).to eq(values)

        values = random_hash(rand(64) + 1)

        @message.annotations = values.clone
        expect(@message.annotations).to eq(values)
      end

      it "can delete the set of annotations" do
        value = random_hash(rand(128) + 1)

        @message.annotations = value.clone
        expect(@message.annotations).to eq(value)

        @message.annotations = nil
        expect(@message.annotations).to be_nil
      end

      it "has no annotations after being cleared" do
        value = random_hash(16)

        @message.annotations = value
        expect(@message.annotations).to eq(value)

        @message.clear
        expect(@message.annotations).to be_empty
      end

      it "has a body property" do
        expect(@message).to respond_to(:body)
        expect(@message).to respond_to(:body=)
      end

      it "has a default body that is nil" do
        expect(@message.body).to be_nil
      end

      it "has no body after being cleared" do
        name = random_string(16)
        value = random_string(128)

        @message.body = value
        expect(@message.body).to eq(value)

        @message.clear
        expect(@message.body).to be_nil
      end

      it "can set the body property" do
        (1..3).each do |which|
          case which
            when 0
            value = random_string(32)
            when 1
            value = random_array(100)
            when 2
            value = random_hash(100)
            when 3
            value = rand(512)
          end

          @message.body = value
          expect(@message.body).to eq(value)
        end
      end

      it "can update the body property" do
        (1..3).each do |which|
          case which
            when 0
            value = random_string(32)
            when 1
            value = random_array(100)
            when 2
            value = random_hash(100)
            when 3
            value = rand(512)
          end

          @message.body = value
          expect(@message.body).to eq(value)

          @message.body = nil
          expect(@message.body).to be_nil
        end
      end

    end

  end

end

