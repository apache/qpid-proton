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
        _(@message).wont_be_nil
      end

      it "can be cleared" do
        subject = random_string(16)
        @message.subject = subject
        _(@message.subject).must_equal(subject)
        @message.clear
        _(@message.subject).wont_equal(subject)
      end

      it "can be durable" do
        @message.durable = true
        _(@message.durable).must_equal(true)
        @message.durable = false
        _(@message.durable).must_equal(false)
      end

      it "raises an error when setting durable to nil" do
        _{
          @message.durable = nil
        }.must_raise(TypeError)
      end

      it "raises an error when setting the priority to nil" do
        _{
          @message.priority = nil
        }.must_raise(TypeError)
      end

      it "raises an error when setting the priority to a non-number" do
        _{
          @message.priority = "abck"
        }.must_raise(TypeError)
      end

      it "sets the priority to the integer portion when a float" do
        priority = rand(100) / 10
        @message.priority = priority
        _(@message.priority).must_equal(priority.floor)
      end

      it "rejects a priority with too large of a value" do
        _{
          @message.priority = (rand(100) + 256)
        }.must_raise(RangeError)
      end

      it "rejects a negative priority" do
        _{
          @message.priority = (0 - (rand(255) + 1))
        }.must_raise(RangeError)
      end

      it "has a priority" do
        priority = rand(256)
        @message.priority = priority
        _(@message.priority).must_equal(priority)
      end

      it "raises an error when setting the time-to-live to nil" do
        _{
          @message.ttl = nil
        }.must_raise(TypeError)
      end

      it "raises an error when setting the time-to-live to a non-number" do
        _{
          @message.ttl = random_string(5)
        }.must_raise(TypeError)
      end

      it "sets the time-to-live to the integer portion when a float" do
        ttl = (rand(32767) / 10)
        @message.ttl = ttl
        _(@message.ttl).must_equal(ttl.floor)
      end

      it "raises an error when the time-to-live is negative" do
        _{
          @message.ttl = (0 - (rand(1000) + 1))
        }.must_raise(RangeError)
      end

      it "has a time-to-live" do
        ttl = rand(32767)
        @message.ttl = ttl
        _(@message.ttl).must_equal(ttl)
      end

      it "raises an error when setting first acquirer to nil" do
        _{
          @message.first_acquirer = nil
        }.must_raise(TypeError)
      end

      it "raises and error when setting first acquirer to a non-boolean" do
        _{
          @message.first_acquirer = random_string(16)
        }.must_raise(TypeError)
      end

      it "has a first acquirer" do
        @message.first_acquirer = true
        _(@message.first_acquirer?).must_equal(true)

        @message.first_acquirer = false
        _(@message.first_acquirer?).must_equal(false)
      end

      it "raises an error on a nil delivery count" do
        _{
          @message.delivery_count = nil
        }.must_raise(::ArgumentError)
      end

      it "raises an error on a negative delivery count" do
        _{
          @message.delivery_count = -1
        }.must_raise(RangeError)
      end

      it "raises an error on a non-numeric delivery count" do
        _{
          @message.delivery_count = "farkle"
        }.must_raise(::ArgumentError)
      end

      it "converts a floating point delivery count to its integer portion" do
          count = rand(255) / 10.0
          @message.delivery_count = count
          _(@message.delivery_count).must_equal(count.floor)
        end

      it "has a delivery count" do
        count = rand(255)
        @message.delivery_count = count
        _(@message.delivery_count).must_equal(count)
      end

      it "allows setting a nil id" do
        @message.id = nil
        _(@message.id).must_be_nil
      end

      it "has an id" do
        id = random_string(16)
        @message.id = id
        _(@message.id).must_equal(id)
      end

      it "allows setting a nil user id" do
        @message.user_id = nil
        _(@message.user_id).must_equal("")
      end

      it "has a user id" do
        id = random_string(16)
        @message.user_id = id
        _(@message.user_id).must_equal(id)
      end

      it "allows setting a nil address" do
        @message.address = nil
        _(@message.address).must_be_nil
      end

      it "has an address" do
        address = "//0.0.0.0/#{random_string(16)}"
        @message.address = address
        _(@message.address).must_equal(address)
      end

      it "allows setting a nil subject" do
        @message.subject = nil
        _(@message.subject).must_be_nil
      end

      it "has a subject" do
        subject = random_string(50)
        @message.subject = subject
        _(@message.subject).must_equal(subject)
      end

      it "will allow a nil reply-to address" do
        @message.reply_to = nil
        _(@message.reply_to).must_be_nil
      end

      it "has a reply-to address" do
        address = "//0.0.0.0/#{random_string(16)}"
        @message.reply_to = address
        _(@message.reply_to).must_equal(address)
      end

      it "will allow a nil correlation id" do
        @message.correlation_id = nil
        _(@message.correlation_id).must_be_nil
      end

      it "has a correlation id" do
        id = random_string(25)
        @message.correlation_id = id
        _(@message.correlation_id).must_equal(id)
      end

      it "will allow a nil content type" do
        @message.content_type = nil
        _(@message.content_type).must_be_nil
      end

      it "will allow an empty content type" do
        @message.content_type = ""
        _(@message.content_type).must_equal("")
      end

      it "has a content type" do
        content_type = random_string(32)
        @message.content_type = content_type
        _(@message.content_type).must_equal(content_type)
      end

      it "can have nil content encoding" do
        @message.content_encoding = nil
        _(@message.content_encoding).must_be_nil
      end

      it "has a content encoding" do
        encoding = "#{random_string(8)}/#{random_string(8)}"
        @message.content_encoding = encoding
        _(@message.content_encoding).must_equal(encoding)
      end

      it "raises an error on a nil expiry time" do
        _{
          @message.expires = nil
        }.must_raise(TypeError)
      end

      it "raises an error on a negative expiry time" do
        _{
          @message.expires = (0-(rand(65535)))
        }.must_raise(::ArgumentError)
      end

      it "can have a zero expiry time" do
        @message.expires = 0
        _(@message.expires).must_equal(0)
      end

      it "has an expiry time" do
        time = rand(65535)
        @message.expires = time
        _(@message.expires).must_equal(time)
      end

      it "raises an error on a nil creation time" do
        _{
          @message.creation_time = nil
        }.must_raise(TypeError)
      end

      it "raises an error on a negative creation time" do
        _{
          @message.creation_time = (0 - rand(65535))
        }.must_raise(::ArgumentError)
      end

      it "can have a zero creation time" do
        @message.creation_time = 0
        _(@message.creation_time).must_equal(0)
      end

      it "has a creation time" do
        time = rand(65535)
        @message.creation_time = time
        _(@message.creation_time).must_equal(time)
      end

      it "can have a nil group id" do
        @message.group_id = nil
        _(@message.group_id).must_be_nil
      end

      it "can have an empty group id" do
        @message.group_id = ""
        _(@message.group_id).must_equal("")
      end

      it "has a group id" do
        id = random_string(16)
        @message.group_id = id
        _(@message.group_id).must_equal(id)
      end


      it "raises an error on a nil group sequence" do
        _{
          @message.group_sequence = nil
        }.must_raise(TypeError)
      end

      it "can have a zero group sequence" do
        @message.group_sequence = 0
        _(@message.group_sequence).must_equal(0)
      end

      it "has a group sequence" do
        id = rand(4294967295)
        @message.group_sequence = id
        _(@message.group_sequence).must_equal(id)
      end

      it "can have a nil reply-to group id" do
        @message.reply_to_group_id = nil
        _(@message.reply_to_group_id).must_be_nil
      end

      it "can have an empty reply-to group id" do
        @message.reply_to_group_id = ""
        _(@message.reply_to_group_id).must_equal("")
      end

      it "has a reply-to group id" do
        id = random_string(16)
        @message.reply_to_group_id = id
        _(@message.reply_to_group_id).must_equal(id)
      end

      it "has properties" do
        _(@message).must_respond_to(:properties)
        _(@message).must_respond_to(:properties=)
        _(@message).must_respond_to(:[])
        _(@message).must_respond_to(:[]=)

        _(@message.properties).must_be_kind_of({}.class)
      end

      it "can replace the set of properties" do
        values = random_hash(128)

        @message.properties = values.clone
        _(@message.properties).must_equal(values)
      end

      it "can set properties" do
        name = random_string(16)
        value = random_string(128)

        @message[name] = value
        _(@message[name]).must_equal(value)
      end

      it "can update properties" do
        name = random_string(16)
        value = random_string(128)

        @message[name] = value
        _(@message[name]).must_equal(value)

        value = random_string(128)
        @message[name] = value
        _(@message[name]).must_equal(value)
      end

      it "can hold a null property" do
        name = random_string(16)
        value = random_string(128)

        @message[name] = value
        _(@message[name]).must_equal(value)

        @message[name] = nil
        _(@message[name]).must_be_nil
      end

      it "can delete a property" do
        name = random_string(16)
        value = random_string(128)

        @message[name] = value
        _(@message[name]).must_equal(value)

        @message.delete_property(name)
        _(@message.properties.keys).wont_include(name)
      end

      it "has no properties after being cleared" do
        name = random_string(16)
        value = random_string(128)

        @message[name] = value
        _(@message[name]).must_equal(value)

        @message.clear
        _(@message.properties).must_be_empty
      end

      it "has instructions" do
        _(@message).must_respond_to(:instructions)
        _(@message).must_respond_to("instructions=".to_sym)
      end

      it "can set an instruction" do
        name = random_string(16)
        value = random_string(128)

        @message.instructions[name] = value
        _(@message.instructions[name]).must_equal(value)
      end

      it "can update an instruction" do
        name = random_string(16)
        value = random_string(128)

        @message.instructions[name] = value
        _(@message.instructions[name]).must_equal(value)

        value = random_string(128)
        @message.instructions[name] = value
        _(@message.instructions[name]).must_equal(value)
      end

      it "can delete the instructions" do
        name = random_string(16)
        value = random_string(128)

        @message.instructions[name] = value
        _(@message.instructions).wont_be_empty

        @message.instructions = nil
        _(@message.instructions).must_be_nil
      end

      it "can replace the instructions" do
        values = random_hash(rand(128) + 1)

        @message.instructions = values.clone
        _(@message.instructions).must_equal(values)

        values = random_hash(rand(64) + 1)

        @message.instructions = values.clone
        _(@message.instructions).must_equal(values)
      end

      it "can delete the set of instructions" do
        values = random_hash(rand(128) + 1)

        @message.instructions = values.clone
        _(@message.instructions).must_equal(values)

        @message.instructions = nil
        _(@message.instructions).must_be_nil
      end

      it "has no instructions after being cleared" do
        value = random_hash(128)

        @message.instructions = value.clone
        _(@message.instructions).must_equal(value)

         @message.clear
        _(@message.instructions).must_be_empty
      end

      it "has annotations" do
        _(@message).must_respond_to(:annotations)
        _(@message).must_respond_to(:annotations=)
      end

      it "can set an annotation" do
        name = random_hash(32)
        value = random_hash(256)

        @message.annotations[name] = value.clone
        _(@message.annotations[name]).must_equal(value)
      end

      it "can update an annotation" do
        name = random_hash(32)
        value = random_hash(256)

        @message.annotations[name] = value.clone
        _(@message.annotations[name]).must_equal(value)

        value = random_hash(128)

        @message.annotations[name] = value.clone
        _(@message.annotations[name]).must_equal(value)
      end

      it "can delete an annotation" do
        name = random_hash(32)
        value = random_hash(256)

        @message.annotations[name] = value.clone
        _(@message.annotations[name]).must_equal(value)

        @message.annotations[name] = nil
        _(@message.annotations[name]).must_be_nil
      end

      it "can replace all annotations" do
        values = random_hash(rand(128) + 1)

        @message.annotations = values.clone
        _(@message.annotations).must_equal(values)

        values = random_hash(rand(64) + 1)

        @message.annotations = values.clone
        _(@message.annotations).must_equal(values)
      end

      it "can delete the set of annotations" do
        value = random_hash(rand(128) + 1)

        @message.annotations = value.clone
        _(@message.annotations).must_equal(value)

        @message.annotations = nil
        _(@message.annotations).must_be_nil
      end

      it "has no annotations after being cleared" do
        value = random_hash(16)

        @message.annotations = value
        _(@message.annotations).must_equal(value)

        @message.clear
        _(@message.annotations).must_be_empty
      end

      it "has a body property" do
        _(@message).must_respond_to(:body)
        _(@message).must_respond_to(:body=)
      end

      it "has a default body that is nil" do
        _(@message.body).must_be_nil
      end

      it "has no body after being cleared" do
        value = random_string(128)

        @message.body = value
        _(@message.body).must_equal(value)

        @message.clear
        _(@message.body).must_be_nil
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
          _(@message.body).must_equal(value)
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
          _(@message.body).must_equal(value)

          @message.body = nil
          _(@message.body).must_be_nil
        end
      end

    end

  end

end
