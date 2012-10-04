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
        @message.should_not be_nil
      end

      it "can be cleared" do
        subject = random_string(16)
        @message.subject = subject
        @message.subject.should eq(subject)
        @message.clear
        @message.subject.should_not eq(subject)
      end

      it "has an error number" do
        @message.error?.should_not be_true
        @message.format = Qpid::Proton::MessageFormat::AMQP
        @message.content = "{"
        @message.error?.should be_true
        @message.errno.should_not eq(0)
      end

      it "has an error message" do
        @message.error?.should_not be_true
        @message.error.should be_nil
        @message.format = Qpid::Proton::MessageFormat::AMQP
        @message.content = "{"
        @message.error?.should be_true
        @message.error.should_not eq("")
      end

      it "can be durable" do
        @message.durable = true
        @message.durable.should be_true
        @message.durable = false
        @message.durable.should_not be_true
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
        @message.priority.should eq(priority.floor)
      end

      it "rejects a priority with too large of a value" do
        expect {
          @message.priority = (rand(100) + 256)
        }.to raise_error(RangeError)
      end

      it "rejects a negative priority" do
        expect {
          @message.priority = (0 - (rand(256)))
        }.to raise_error(RangeError)
      end

      it "has a priority" do
        priority = rand(256)
        @message.priority = priority
        @message.priority.should equal(priority)
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
        @message.ttl.should equal(ttl.floor)
      end

      it "raises an error when the time-to-live is negative" do
        expect {
          @message.ttl = (0 - rand(1000))
        }.to raise_error(RangeError)
      end

      it "has a time-to-live" do
        ttl = rand(32767)
        @message.ttl = ttl
        @message.ttl.should equal(ttl)
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
        @message.first_acquirer?.should be_true

        @message.first_acquirer = false
        @message.first_acquirer?.should_not be_true
      end

      it "raises an error on a nil delivery count" do
        expect {
          @message.delivery_count = nil
        }.to raise_error(ArgumentError)
      end

      it "raises an error on a negative delivery count" do
        expect {
          @message.delivery_count = -1
        }.to raise_error(RangeError)
      end

      it "raises an error on a non-numeric delivery count" do
        expect {
          @message.delivery_count = "farkle"
        }.to raise_error(ArgumentError)
      end

      it "converts a floating point delivery count to its integer portion" do
          count = rand(255) / 10.0
          @message.delivery_count = count
          @message.delivery_count.should eq(count.floor)
        end

      it "has a delivery count" do
        count = rand(255)
        @message.delivery_count = count
        @message.delivery_count.should eq(count)
      end

      it "allows setting a nil id" do
        @message.id = nil
        @message.id.should be_nil
      end

      it "has an id" do
        id = random_string(16)
        @message.id = id
        @message.id.should eq(id)
      end

      it "allows setting a nil user id" do
        @message.user_id = nil
        @message.user_id.should eq("")
      end

      it "has a user id" do
        id = random_string(16)
        @message.user_id = id
        @message.user_id.should eq(id)
      end

      it "allows setting a nil address" do
        @message.address = nil
        @message.address.should be_nil
      end

      it "has an address" do
        address = "//0.0.0.0/#{random_string(16)}"
        @message.address = address
        @message.address.should eq(address)
      end

      it "allows setting a nil subject" do
        @message.subject = nil
        @message.subject.should be_nil
      end

      it "has a subject" do
        subject = random_string(50)
        @message.subject = subject
        @message.subject.should eq(subject)
      end

      it "will allow a nil reply-to address" do
        @message.reply_to = nil
        @message.reply_to.should be_nil
      end

      it "has a reply-to address" do
        address = "//0.0.0.0/#{random_string(16)}"
        @message.reply_to = address
        @message.reply_to.should eq(address)
      end

      it "will allow a nil correlation id" do
        @message.correlation_id = nil
        @message.correlation_id.should be_nil
      end

      it "has a correlation id" do
        id = random_string(25)
        @message.correlation_id = id
        @message.correlation_id.should eq(id)
      end

      it "raises an error when setting  a nil format" do
        expect {
          @message.format = nil
        }.to raise_error(TypeError)
      end

      it "raises an error on an invalid format" do
        expect {
          @message.format = "farkle"
        }.to raise_error(TypeError)
      end

      it "has a format" do
        Qpid::Proton::MessageFormat.formats.each do |format|
          @message.format = format
          @message.format.should eq(format)
        end
      end

      it "will allow a nil content type" do
        @message.content_type = nil
        @message.content_type.should be_nil
      end

      it "will allow an empty content type" do
        @message.content_type = ""
        @message.content_type.should eq("")
      end

      it "has a content type" do
        content_type = random_string(32)
        @message.content_type = content_type
        @message.content_type.should eq(content_type)
      end

      it "can have nil content encoding" do
        @message.content_encoding = nil
        @message.content_encoding.should be_nil
      end

      it "has a content encoding" do
        encoding = "#{random_string(8)}/#{random_string(8)}"
        @message.content_encoding = encoding
        @message.content_encoding.should eq(encoding)
      end

      it "raises an error on a nil expiry time" do
        expect {
          @message.expires = nil
        }.to raise_error(TypeError)
      end

      it "raises an error on a negative expiry time" do
        expect {
          @message.expires = (0-(rand(65535)))
        }.to raise_error(ArgumentError)
      end

      it "can have a zero expiry time" do
        @message.expires = 0
        @message.expires.should equal(0)
      end

      it "has an expiry time" do
        time = rand(65535)
        @message.expires = time
        @message.expires.should equal(time)
      end

      it "raises an error on a nil creation time" do
        expect {
          @message.creation_time = nil
        }.to raise_error(TypeError)
      end

      it "raises an error on a negative creation time" do
        expect {
          @message.creation_time = (0 - rand(65535))
        }.to raise_error(ArgumentError)
      end

      it "can have a zero creation time" do
        @message.creation_time = 0
        @message.creation_time.should equal(0)
      end

      it "has a creation time" do
        time = rand(65535)
        @message.creation_time = time
        @message.creation_time.should equal(time)
      end

      it "can have a nil group id" do
        @message.group_id = nil
        @message.group_id.should be_nil
      end

      it "can have an empty group id" do
        @message.group_id = ""
        @message.group_id.should eq("")
      end

      it "has a group id" do
        id = random_string(16)
        @message.group_id = id
        @message.group_id.should eq(id)
      end


      it "raises an error on a nil group sequence" do
        expect {
          @message.group_sequence = nil
        }.to raise_error(TypeError)
      end

      it "can have a negative group sequence" do
        seq = (0 - rand(32767))
        @message.group_sequence = seq
        @message.group_sequence.should eq(seq)
      end

      it "can have a zero group sequence" do
        @message.group_sequence = 0
        @message.group_sequence.should eq(0)
      end

      it "has a group sequence" do
        id = rand(32767)
        @message.group_sequence = id
        @message.group_sequence.should eq(id)
      end

      it "can have a nil reply-to group id" do
        @message.reply_to_group_id = nil
        @message.reply_to_group_id.should be_nil
      end

      it "can have an empty reply-to group id" do
        @message.reply_to_group_id = ""
        @message.reply_to_group_id.should eq("")
      end

      it "has a reply-to group id" do
        id = random_string(16)
        @message.reply_to_group_id = id
        @message.reply_to_group_id.should eq(id)
      end

      it "can have nil content" do
        @message.content = nil
        @message.content.should be_nil
      end

      it "can have an empty string as content" do
        @message.content = ""
        @message.content.should eq("")
      end

      it "can have content" do
        content = random_string(255)
        @message.content = content
        @message.content.should eq(content)
      end

    end

  end

end

