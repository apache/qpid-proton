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

module Qpid

  module Proton

    class ExceptionHandlingClass
      include Qpid::Proton::Util::ErrorHandler

      def error
        "This is a test error: #{Time.new}"
      end
    end

    describe "The exception handling mixin" do

      before (:each) do
        @handler = Qpid::Proton::ExceptionHandlingClass.new
      end

      it "does not raise an error on a zero code" do
        expect {
          @handler.check_for_error(0)
        }.to_not raise_error
      end

      it "raises EOS on PN_EOS" do
        expect {
          @handler.check_for_error(Qpid::Proton::Error::EOS)
        }.to raise_error(Qpid::Proton::EOSError)
      end

      it "raises Error on PN_ERR" do
        expect {
          @handler.check_for_error(Qpid::Proton::Error::ERROR)
        }.to raise_error(Qpid::Proton::ProtonError)
      end

      it "raises Overflow on PN_OVERFLOW" do
        expect {
          @handler.check_for_error(Qpid::Proton::Error::OVERFLOW)
        }.to raise_error(Qpid::Proton::OverflowError)
      end

      it "raises Underflow on PN_UNDERFLOW" do
        expect {
          @handler.check_for_error(Qpid::Proton::Error::UNDERFLOW)
        }.to raise_error(Qpid::Proton::UnderflowError)
      end

      it "raises Argument on PN_ARG_ERR" do
        expect {
          @handler.check_for_error(Qpid::Proton::Error::ARGUMENT)
        }.to raise_error(Qpid::Proton::ArgumentError)
      end

      it "raises Timeout on PN_TIMEOUT" do
        expect {
          @handler.check_for_error(Qpid::Proton::Error::TIMEOUT)
        }.to raise_error(Qpid::Proton::TimeoutError)
      end

      it "raises an Ruby ArgumentError on a nil code" do
        expect {
          @handler.check_for_error(nil)
        }.to raise_error(::ArgumentError)
      end

      it "raises a Ruby ArgumentError on an unknown value" do
        expect {
          @handler.check_for_error("farkle")
        }.to raise_error(::ArgumentError)
      end

    end

  end

end
