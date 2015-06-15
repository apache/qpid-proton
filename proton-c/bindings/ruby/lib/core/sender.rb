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

  # The sending endpoint.
  #
  # @see Receiver
  #
  class Sender < Link

    # @private
    include Util::ErrorHandler

    # @private
    can_raise_error :stream, :error_class => Qpid::Proton::LinkError

    # Signals the availability of deliveries.
    #
    # @param n [Fixnum] The number of deliveries potentially available.
    #
    def offered(n)
      Cproton.pn_link_offered(@impl, n)
    end

    # Sends the specified data to the remote endpoint.
    #
    # @param object [Object] The content to send.
    # @param tag [Object] The tag
    #
    # @return [Fixnum] The number of bytes sent.
    #
    def send(object, tag = nil)
      if object.respond_to? :proton_send
        object.proton_send(self, tag)
      else
        stream(object)
      end
    end

    # Send the specified bytes as part of the current delivery.
    #
    # @param bytes [Array] The bytes to send.
    #
    # @return n [Fixnum] The number of bytes sent.
    #
    def stream(bytes)
      Cproton.pn_link_send(@impl, bytes)
    end

    def delivery_tag
      @tag_count ||= 0
      result = @tag_count.succ
      @tag_count = result
      return "#{result}"
    end

  end

end
