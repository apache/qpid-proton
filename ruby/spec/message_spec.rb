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

    describe "A message" do

      before (:each) do
        @message = Qpid::Proton::Message.new
      end

      it "can be created" do
        @message.wont_be_nil
      end

      it "can be cleared" do
        subject = random_string(16)
        @message.subject = subject
        @message.subject.must_equal(subject)
        @message.clear
        @message.subject.wont_equal(subject)
      end

      it "can be durable" do
        @message.durable = true
        @message.durable.must_equal(true)
        @message.durable = false
        @message.durable.must_equal(false)
      end

      it "raises an error when setting durable to nil" do
        proc {
          @message.durable = nil
        }.must_raise(TypeError)
      end

      it "raises an error when setting the priority to nil" do
        proc {
          @message.priority = nil
        }.must_raise(TypeError)
      end

      it "raises an error when setting the priority to a non-number" do
        proc {
          @message.priority = "abck"
        }.must_raise(TypeError)
      end

      it "sets the priority to the integer portion when a float" do
        priority = rand(100) / 10
        @message.priority = priority
        @message.priority.must_equal(priority.floor)
      end

      it "rejects a priority with too large of a value" do
        proc {
          @message.priority = (rand(100) + 256)
        }.must_raise(RangeError)
      end

      it "rejects a negative priority" do
        proc {
          @message.priority = (0 - (rand(255) + 1))
        }.must_raise(RangeError)
      end

      it "has a priority" do
        priority = rand(256)
        @message.priority = priority
        @message.priority.must_equal(priority)
      end

      it "raises an error when setting the time-to-live to nil" do
        proc {
          @message.ttl = nil
        }.must_raise(TypeError)
      end

      it "raises an error when setting the time-to-live to a non-number" do
        proc {
          @message.ttl = random_string(5)
        }.must_raise(TypeError)
      end

      it "sets the time-to-live to the integer portion when a float" do
        ttl = (rand(32767) / 10)
        @message.ttl = ttl
        @message.ttl.must_equal(ttl.floor)
      end

      it "raises an error when the time-to-live is negative" do
        proc {
          @message.ttl = (0 - (rand(1000) + 1))
        }.must_raise(RangeError)
      end

      it "has a time-to-live" do
        ttl = rand(32767)
        @message.ttl = ttl
        @message.ttl.must_equal(ttl)
      end

      it "raises an error when setting first acquirer to nil" do
        proc {
          @message.first_acquirer = nil
        }.must_raise(TypeError)
      end

      it "raises and error when setting first acquirer to a non-boolean" do
        proc {
          @message.first_acquirer = random_string(16)
        }.must_raise(TypeError)
      end

      it "has a first acquirer" do
        @message.first_acquirer = true
        @message.first_acquirer?.must_equal(true)

        @message.first_acquirer = false
        @message.first_acquirer?.must_equal(false)
      end

      it "raises an error on a nil delivery count" do
        proc {
          @message.delivery_count = nil
        }.must_raise(::ArgumentError)
      end

      it "raises an error on a negative delivery count" do
        proc {
          @message.delivery_count = -1
        }.must_raise(RangeError)
      end

      it "raises an error on a non-numeric delivery count" do
        proc {
          @message.delivery_count = "farkle"
        }.must_raise(::ArgumentError)
      end

      it "converts a floating point delivery count to its integer portion" do
          count = rand(255) / 10.0
          @message.delivery_count = count
          @message.delivery_count.must_equal(count.floor)
        end

      it "has a delivery count" do
        count = rand(255)
        @message.delivery_count = count
        @message.delivery_count.must_equal(count)
      end

      it "allows setting a nil id" do
        @message.id = nil
        @message.id.must_be_nil
      end

      it "has an id" do
        id = random_string(16)
        @message.id = id
        @message.id.must_equal(id)
      end

      it "allows setting a nil user id" do
        @message.user_id = nil
        @message.user_id.must_equal("")
      end

      it "has a user id" do
        id = random_string(16)
        @message.user_id = id
        @message.user_id.must_equal(id)
      end

      it "allows setting a nil address" do
        @message.address = nil
        @message.address.must_be_nil
      end

      it "has an address" do
        address = "//0.0.0.0/#{random_string(16)}"
        @message.address = address
        @message.address.must_equal(address)
      end

      it "allows setting a nil subject" do
        @message.subject = nil
        @message.subject.must_be_nil
      end

      it "has a subject" do
        subject = random_string(50)
        @message.subject = subject
        @message.subject.must_equal(subject)
      end

      it "will allow a nil reply-to address" do
        @message.reply_to = nil
        @message.reply_to.must_be_nil
      end

      it "has a reply-to address" do
        address = "//0.0.0.0/#{random_string(16)}"
        @message.reply_to = address
        @message.reply_to.must_equal(address)
      end

      it "will allow a nil correlation id" do
        @message.correlation_id = nil
        @message.correlation_id.must_be_nil
      end

      it "has a correlation id" do
        id = random_string(25)
        @message.correlation_id = id
        @message.correlation_id.must_equal(id)
      end

      it "will allow a nil content type" do
        @message.content_type = nil
        @message.content_type.must_be_nil
      end

      it "will allow an empty content type" do
        @message.content_type = ""
        @message.content_type.must_equal("")
      end

      it "has a content type" do
        content_type = random_string(32)
        @message.content_type = content_type
        @message.content_type.must_equal(content_type)
      end

      it "can have nil content encoding" do
        @message.content_encoding = nil
        @message.content_encoding.must_be_nil
      end

      it "has a content encoding" do
        encoding = "#{random_string(8)}/#{random_string(8)}"
        @message.content_encoding = encoding
        @message.content_encoding.must_equal(encoding)
      end

      it "raises an error on a nil expiry time" do
        proc {
          @message.expires = nil
        }.must_raise(TypeError)
      end

      it "raises an error on a negative expiry time" do
        proc {
          @message.expires = (0-(rand(65535)))
        }.must_raise(::ArgumentError)
      end

      it "can have a zero expiry time" do
        @message.expires = 0
        @message.expires.must_equal(0)
      end

      it "has an expiry time" do
        time = rand(65535)
        @message.expires = time
        @message.expires.must_equal(time)
      end

      it "raises an error on a nil creation time" do
        proc {
          @message.creation_time = nil
        }.must_raise(TypeError)
      end

      it "raises an error on a negative creation time" do
        proc {
          @message.creation_time = (0 - rand(65535))
        }.must_raise(::ArgumentError)
      end

      it "can have a zero creation time" do
        @message.creation_time = 0
        @message.creation_time.must_equal(0)
      end

      it "has a creation time" do
        time = rand(65535)
        @message.creation_time = time
        @message.creation_time.must_equal(time)
      end

      it "can have a nil group id" do
        @message.group_id = nil
        @message.group_id.must_be_nil
      end

      it "can have an empty group id" do
        @message.group_id = ""
        @message.group_id.must_equal("")
      end

      it "has a group id" do
        id = random_string(16)
        @message.group_id = id
        @message.group_id.must_equal(id)
      end


      it "raises an error on a nil group sequence" do
        proc {
          @message.group_sequence = nil
        }.must_raise(TypeError)
      end

      it "can have a zero group sequence" do
        @message.group_sequence = 0
        @message.group_sequence.must_equal(0)
      end

      it "has a group sequence" do
        id = rand(4294967295)
        @message.group_sequence = id
        @message.group_sequence.must_equal(id)
      end

      it "can have a nil reply-to group id" do
        @message.reply_to_group_id = nil
        @message.reply_to_group_id.must_be_nil
      end

      it "can have an empty reply-to group id" do
        @message.reply_to_group_id = ""
        @message.reply_to_group_id.must_equal("")
      end

      it "has a reply-to group id" do
        id = random_string(16)
        @message.reply_to_group_id = id
        @message.reply_to_group_id.must_equal(id)
      end

      it "has properties" do
        @message.must_respond_to(:properties)
        @message.must_respond_to(:properties=)
        @message.must_respond_to(:[])
        @message.must_respond_to(:[]=)

        @message.properties.must_be_kind_of({}.class)
      end

      it "can replace the set of properties" do
        values = random_hash(128)

        @message.properties = values.clone
        @message.properties.must_equal(values)
      end

      it "can set properties" do
        name = random_string(16)
        value = random_string(128)

        @message[name] = value
        @message[name].must_equal(value)
      end

      it "can update properties" do
        name = random_string(16)
        value = random_string(128)

        @message[name] = value
        @message[name].must_equal(value)

        value = random_string(128)
        @message[name] = value
        @message[name].must_equal(value)
      end

      it "can hold a null property" do
        name = random_string(16)
        value = random_string(128)

        @message[name] = value
        @message[name].must_equal(value)

        @message[name] = nil
        @message[name].must_be_nil
      end

      it "can delete a property" do
        name = random_string(16)
        value = random_string(128)

        @message[name] = value
        @message[name].must_equal(value)

        @message.delete_property(name)
        @message.properties.keys.wont_include(name)
      end

      it "has no properties after being cleared" do
        name = random_string(16)
        value = random_string(128)

        @message[name] = value
        @message[name].must_equal(value)

        @message.clear
        @message.properties.must_be_empty
      end

      it "has instructions" do
        @message.must_respond_to(:instructions)
        @message.must_respond_to("instructions=".to_sym)
      end

      it "can set an instruction" do
        name = random_string(16)
        value = random_string(128)

        @message.instructions[name] = value
        @message.instructions[name].must_equal(value)
      end

      it "can update an instruction" do
        name = random_string(16)
        value = random_string(128)

        @message.instructions[name] = value
        @message.instructions[name].must_equal(value)

        value = random_string(128)
        @message.instructions[name] = value
        @message.instructions[name].must_equal(value)
      end

      it "can delete the instructions" do
        name = random_string(16)
        value = random_string(128)

        @message.instructions[name] = value
        @message.instructions.wont_be_empty

        @message.instructions = nil
        @message.instructions.must_be_nil
      end

      it "can replace the instructions" do
        values = random_hash(rand(128) + 1)

        @message.instructions = values.clone
        @message.instructions.must_equal(values)

        values = random_hash(rand(64) + 1)

        @message.instructions = values.clone
        @message.instructions.must_equal(values)
      end

      it "can delete the set of instructions" do
        values = random_hash(rand(128) + 1)

        @message.instructions = values.clone
        @message.instructions.must_equal(values)

        @message.instructions = nil
        @message.instructions.must_be_nil
      end

      it "has no instructions after being cleared" do
        value = random_hash(128)

        @message.instructions = value.clone
        @message.instructions.must_equal(value)

         @message.clear
        @message.instructions.must_be_empty
      end

      it "has annotations" do
        @message.must_respond_to(:annotations)
        @message.must_respond_to(:annotations=)
      end

      it "can set an annotation" do
        name = random_hash(32)
        value = random_hash(256)

        @message.annotations[name] = value.clone
        @message.annotations[name].must_equal(value)
      end

      it "can update an annotation" do
        name = random_hash(32)
        value = random_hash(256)

        @message.annotations[name] = value.clone
        @message.annotations[name].must_equal(value)

        value = random_hash(128)

        @message.annotations[name] = value.clone
        @message.annotations[name].must_equal(value)
      end

      it "can delete an annotation" do
        name = random_hash(32)
        value = random_hash(256)

        @message.annotations[name] = value.clone
        @message.annotations[name].must_equal(value)

        @message.annotations[name] = nil
        @message.annotations[name].must_be_nil
      end

      it "can replace all annotations" do
        values = random_hash(rand(128) + 1)

        @message.annotations = values.clone
        @message.annotations.must_equal(values)

        values = random_hash(rand(64) + 1)

        @message.annotations = values.clone
        @message.annotations.must_equal(values)
      end

      it "can delete the set of annotations" do
        value = random_hash(rand(128) + 1)

        @message.annotations = value.clone
        @message.annotations.must_equal(value)

        @message.annotations = nil
        @message.annotations.must_be_nil
      end

      it "has no annotations after being cleared" do
        value = random_hash(16)

        @message.annotations = value
        @message.annotations.must_equal(value)

        @message.clear
        @message.annotations.must_be_empty
      end

      it "has a body property" do
        @message.must_respond_to(:body)
        @message.must_respond_to(:body=)
      end

      it "has a default body that is nil" do
        @message.body.must_be_nil
      end

      it "has no body after being cleared" do
        value = random_string(128)

        @message.body = value
        @message.body.must_equal(value)

        @message.clear
        @message.body.must_be_nil
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
          @message.body.must_equal(value)
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
          @message.body.must_equal(value)

          @message.body = nil
          @message.body.must_be_nil
        end
      end

    end

  end

end
