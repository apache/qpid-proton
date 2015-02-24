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

module Qpid::Proton::Util

  # @private
  module Engine

    # Convenience method to receive messages from a delivery.
    #
    # @param delivery [Qpid::Proton::Delivery] The delivery.
    # @param message [Qpid::Proton::Message] The message to use.
    #
    # @return [Qpid::Proton::Message] the message
    #
    def self.receive_message(delivery, msg = nil)
      msg = Qpid::Proton::Message.new if msg.nil?
      msg.decode(delivery.link.receive(delivery.pending))
      delivery.link.advance
      return msg
    end

    def data_to_object(data_impl) # :nodoc:
      object = nil
      unless data_impl.nil?
        data = Qpid::Proton::Codec::Data.new(data_impl)
        data.rewind
        data.next
        object = data.object
        data.rewind
      end
      return object
    end

    def object_to_data(object, data_impl) # :nodoc:
      unless object.nil?
        data = Data.new(data_impl)
        data.object = object
      end
    end

    def condition_to_object(condition) # :nodoc:
      result = nil
      if Cproton.pn_condition_is_set(condition)
        result = Condition.new(Cproton.pn_condition_get_name(condition),
                               Cproton.pn_condition_get_description(condition),
                               data_to_object(Cproton.pn_condition_info(condition)))
      end
      return result
    end

    def object_to_condition(object, condition) # :nodoc:
      Cproton.pn_condition_clear(condition)
      unless object.nil?
        Cproton.pn_condition_set_name(condition, object.name)
        Cproton.pn_condition_set_description(condition, object.description)
        info = Data.new(Cproton.pn_condition_info(condition))
        if object.info?
          info.object = object.info
        end
      end
    end

  end

end
