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

    describe "A messenger" do

      before (:each) do
        @messenger = Qpid::Proton::Messenger.new
      end

      after (:each) do
        begin
          @messenger.stop
        rescue ProtonError => error
          # ignore this error
        end
      end

      it "will generate a name if one is not provided" do
        @messenger.name.should_not be_nil
      end

      it "will accept an assigned name" do
        name = random_string(16)
        msgr = Qpid::Proton::Messenger.new(name)
        msgr.name.should eq(name)
      end

      it "raises an error on a nil timeout" do
        expect {
          @messenger.timeout = nil
        }.to raise_error(TypeError)
      end

      it "can have a negative timeout" do
        timeout = (0 - rand(65535))
        @messenger.timeout = timeout
        @messenger.timeout.should eq(timeout)
      end

      it "has a timeout" do
        timeout = rand(65535)
        @messenger.timeout = timeout
        @messenger.timeout.should eq(timeout)
      end

      it "has an error number" do
        @messenger.error?.should_not be_true
        @messenger.errno.should eq(0)
        # force an error
        expect {
          @messenger.subscribe("amqp://~#{random_string}")
        }.to raise_error(ProtonError)
        @messenger.error?.should be_true
        @messenger.errno.should_not eq(0)
      end

      it "has an error message" do
        @messenger.error?.should_not be_true
        @messenger.error.should be_nil
        # force an error
        expect {
          @messenger.subscribe("amqp://~#{random_string}")
        }.to raise_error(ProtonError)
        @messenger.error?.should be_true
        @messenger.errno.should_not be_nil
      end

      it "can be started" do
        expect {
          @messenger.start
        }.to_not raise_error
      end

      it "can be stopped" do
        expect {
          @messenger.stop
        }.to_not raise_error
      end

      it "raises an error when subscribing to a nil address" do
        expect {
          @messenger.subscribe(nil)
        }.to raise_error(TypeError)
      end

      it "raises an error when subscribing to an invalid address" do
        expect {
          @messenger.subscribe("amqp://~#{random_string}")
        }.to raise_error(ProtonError)
        @messenger.error?.should be_true
        @messenger.errno.should_not be_nil
      end

      it "can have a nil certificate" do
        expect {
          @messenger.certificate = nil
          @messenger.certificate.should be_nil
        }.to_not raise_error
      end

      it "can have a certificate" do
        cert = random_string(128)
        @messenger.certificate = cert
        @messenger.certificate.should eq(cert)
      end

      it "can have a nil private key" do
        expect {
          @messenger.private_key = nil
          @messenger.private_key.should be_nil
        }.to_not raise_error
      end

      it "can have a private key" do
        key = random_string(128)
        @messenger.private_key = key
        @messenger.private_key.should eq(key)
      end

      it "can have a nil trusted certificates" do
        expect {
          @messenger.trusted_certificates = nil
          @messenger.trusted_certificates.should be_nil
        }.to_not raise_error
      end

      it "has a list of trusted certificates" do
        certs = random_string(128)
        @messenger.trusted_certificates = certs
        @messenger.trusted_certificates.should eq(certs)
      end

      it "raises an error on a nil outgoing window" do
        expect {
          @messenger.outgoing_window = nil
        }.to raise_error(TypeError)
      end

      it "raises an error on a non-numeric outgoing window" do
        expect {
          @messenger.outgoing_window = random_string(16)
        }.to raise_error(TypeError)
      end

      it "can have a negative outgoing window" do
        window = 0 - (rand(256) + 1)
        @messenger.outgoing_window = window
        @messenger.outgoing_window.should eq(window)
      end

      it "can have a positive outgoing window" do
        window = (rand(256) + 1)
        @messenger.outgoing_window = window
        @messenger.outgoing_window.should eq(window)
      end

      it "can have a zero outgoing window" do
        window = 0
        @messenger.outgoing_window = window
        @messenger.outgoing_window.should eq(window)
      end

      it "raises an error on a nil incoming window" do
        expect {
          @messenger.incoming_window = nil
        }.to raise_error(TypeError)
      end

      it "raises an error on a non-numeric incoming window" do
        expect {
          @messenger.incoming_window = random_string(16)
        }.to raise_error(TypeError)
      end

      it "can have a negative incoming window" do
        window = 0 - (rand(256) + 1)
        @messenger.incoming_window = window
        @messenger.incoming_window.should eq(window)
      end

      it "can have a positive incoming window" do
        window = (rand(256) + 1)
        @messenger.incoming_window = window
        @messenger.incoming_window.should eq(window)
      end

      it "can have a zero incoming window" do
        window = 0
        @messenger.incoming_window = window
        @messenger.incoming_window.should eq(window)
      end

      it "can be put into passive mode" do
        @messenger.passive = true
        @messenger.passive?.should be_true
      end

      it "can be taken out of passive mode" do
        @messenger.passive = false
        @messenger.passive?.should_not be_true
      end

      describe "once started" do

        before (:each) do
          @messenger.start
        end

        after (:each) do
          begin
            @messenger.stop
          rescue ProtonError => error
            # ignore this error
          end
        end

        it "can subscribe to an address" do
          @messenger.subscribe("amqp://~0.0.0.0:#{5700+rand(1024)}").should_not be_nil
        end

        it "returns a tracker's status"

        describe "and subscribed to an address" do

          before (:each) do
            # create a receiver
            @port = 5700 + rand(1024)
            @receiver = Qpid::Proton::Messenger.new("receiver")
            @receiver.subscribe("amqp://~0.0.0.0:#{@port}")
            @messenger.timeout = 0
            @receiver.timeout = 0
            @receiver.start

            Thread.new do
              @receiver.receive(10)
            end

            @msg = Qpid::Proton::Message.new
            @msg.address = "amqp://0.0.0.0:#{@port}"
            @msg.content = "Test sent #{Time.new}"
          end

          after (:each) do
            begin
              @messenger.stop
            rescue ProtonError => error
              # ignore this error
            end
            begin
              @receiver.stop
            rescue
            end
          end

          it "raises an error when queueing a nil message" do
            expect {
              @messenger.put(nil)
            }.to raise_error(TypeError)
          end

          it "raises an error when queueing an invalid object" do
            expect {
              @messenger.put("This is not a message")
            }.to raise_error(ArgumentError)
          end

          it "can place a message in the outgoing queue" do
            expect {
              @messenger.put(@msg)
            }.to_not raise_error
          end

          it "can send with an empty queue"

          describe "with a an outgoing tracker" do

            before(:each) do
              @messenger.put(@msg)
              @tracker = @messenger.outgoing_tracker
            end

            it "has an outgoing tracker" do
              @tracker.should_not be_nil
            end

            it "returns a tracker's status"

            it "raises an error when settling with a nil tracker" do
              expect {
                @messenger.settle(nil)
              }.to raise_error(TypeError)
            end

            it "can settle a tracker's status" do
              @messenger.settle(@tracker)
            end

            it "raises an error when checking status on a nil tracker" do
              expect {
                @messenger.status(nil)
              }.to raise_error(TypeError)
            end

            it "raises an error when checking status on an invalid tracker" do
              expect {
                @messenger.status(random_string(16))
              }.to raise_error(TypeError)
            end

            it "can check the status of a tracker" do
              @messenger.status(@tracker).should_not be_nil
            end

          end

          it "has an incoming tracker"
          it "can reject an incoming message"

          it "raises an error when accepting with an invalid tracker" do
            expect {
              @messenger.accept(random_string(16))
            }.to raise_error(TypeError)
          end

          it "can accept a message"

          it "raises an error when rejecting with an invalid tracker" do
            expect {
              @messenger.accept(random_string(16))
            }.to raise_error(TypeError)
          end

          describe "with messages sent" do

            before (:each) do
              @messenger.put(@msg)
            end

            it "can send messages"

            it "raises an error when receiving with a nil max" do
              expect {
                @messenger.receive(nil)
              }.to raise_error(TypeError)
            end

            it "raises an error when receiving with a non-numeric max" do
              expect {
                @messenger.receive("farkle")
              }.to raise_error(TypeError)
            end

            it "can receive messages"
            it "and create a new message when one wasn't provided"
            it "can get a message from the incoming queue"
            it "can tell how many outgoing messages are pending"
            it "can tell how many incoming messages are queued"

          end

        end

      end

    end

  end

end
