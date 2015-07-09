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

  # A Message represents an addressable quantity of data.
  #
  # ==== Message Body
  #
  # The message body can be set using the #body= method. The message will
  # then attempt to determine how exactly to encode the content.
  #
  # ==== Examples
  #
  # To create a message for sending:
  #
  #   # send a simple text message
  #   msg = Qpid::Proton::Message.new
  #   msg.body = "STATE: update"
  #
  #   # send a binary chunk of data
  #   data = File.binread("/home/qpid/binfile.tar.gz")
  #   msg = Qpid::Proton::Message.new
  #   msg.body = Qpid::Proton::BinaryString.new(data)
  #
  class Message

    # @private
    def proton_send(sender, tag = nil)
      dlv = sender.delivery(tag || sender.delivery_tag)
      encoded = self.encode
      sender.stream(encoded)
      sender.advance
      dlv.settle if sender.snd_settle_mode == Link::SND_SETTLED
      return dlv
    end

    # Decodes a message from supplied AMQP data and returns the number
    # of bytes consumed.
    #
    # ==== Options
    #
    # * encoded - the encoded data
    #
    def decode(encoded)
      check(Cproton.pn_message_decode(@impl, encoded, encoded.length))

      post_decode
    end

    def post_decode # :nodoc:
      # decode elements from the message
      @properties = {}
      props = Codec::Data.new(Cproton::pn_message_properties(@impl))
      if props.next
        @properties = props.type.get(props)
      end
      @instructions = nil
      insts = Codec::Data.new(Cproton::pn_message_instructions(@impl))
      if insts.next
        @instructions = insts.type.get(insts)
      end
      @annotations = nil
      annts = Codec::Data.new(Cproton::pn_message_annotations(@impl))
      if annts.next
        @annotations = annts.type.get(annts)
      end
      @body = nil
      body = Codec::Data.new(Cproton::pn_message_body(@impl))
      if body.next
        @body = body.type.get(body)
      end
    end

    # Encodes the message.
    def encode
      pre_encode
      size = 16
      loop do
        error, data = Cproton::pn_message_encode(@impl, size)
        if error == Qpid::Proton::Error::OVERFLOW
          size *= 2
        else
          check(error)
          return data
        end
      end
    end

    def pre_encode # :nodoc:
      # encode elements from the message
      props = Codec::Data.new(Cproton::pn_message_properties(@impl))
      props.clear
      Codec::Mapping.for_class(@properties.class).put(props, @properties) unless @properties.empty?
      insts = Codec::Data.new(Cproton::pn_message_instructions(@impl))
      insts.clear
      if !@instructions.nil?
        mapping = Codec::Mapping.for_class(@instructions.class)
        mapping.put(insts, @instructions)
      end
      annts = Codec::Data.new(Cproton::pn_message_annotations(@impl))
      annts.clear
      if !@annotations.nil?
        mapping = Codec::Mapping.for_class(@annotations.class)
        mapping.put(annts, @annotations, :keys => :SYMBOL)
      end
      body = Codec::Data.new(Cproton::pn_message_body(@impl))
      body.clear
      if !@body.nil?
        mapping = Codec::Mapping.for_class(@body.class)
        mapping.put(body, @body)
      end
    end

    # Creates a new +Message+ instance.
    def initialize
      @impl = Cproton.pn_message
      ObjectSpace.define_finalizer(self, self.class.finalize!(@impl))
      @properties = {}
      @instructions = {}
      @annotations = {}
      @body = nil
    end

    def to_s
      tmp = Cproton.pn_string("")
      Cproton.pn_inspect(@impl, tmp)
      result = Cproton.pn_string_get(tmp)
      Cproton.pn_free(tmp)
      return result
    end

    # Invoked by garbage collection to clean up resources used
    # by the underlying message implementation.
    def self.finalize!(impl) # :nodoc:
      proc {
        Cproton.pn_message_free(impl)
      }
    end

    # Returns the underlying message implementation.
    def impl # :nodoc:
      @impl
    end

    # Clears the state of the +Message+. This allows a single instance of
    # +Message+ to be reused.
    #
    def clear
      Cproton.pn_message_clear(@impl)
      @properties.clear unless @properties.nil?
      @instructions.clear unless @instructions.nil?
      @annotations.clear unless @annotations.nil?
      @body = nil
    end

    # Returns the most recent error number.
    #
    def errno
      Cproton.pn_message_errno(@impl)
    end

    # Returns the most recent error message.
    #
    def error
      Cproton.pn_error_text(Cproton.pn_message_error(@impl))
    end

    # Returns whether there is currently an error reported.
    #
    def error?
      !Cproton.pn_message_errno(@impl).zero?
    end

    # Sets the durable flag.
    #
    # See ::durable for more details on message durability.
    #
    # ==== Options
    #
    # * state - the durable state
    #
    def durable=(state)
      raise TypeError.new("state cannot be nil") if state.nil?
      Cproton.pn_message_set_durable(@impl, state)
    end

    # Returns the durable property.
    #
    # The durable property indicates that the emessage should be held durably
    # by any intermediaries taking responsibility for the message.
    #
    # ==== Examples
    #
    #  msg = Qpid::Proton::Message.new
    #  msg.durable = true
    #
    def durable
      Cproton.pn_message_is_durable(@impl)
    end

    # Sets the priority.
    #
    # +NOTE:+ Priority values are limited to the range [0,255].
    #
    # ==== Options
    #
    # * priority - the priority value
    #
    def priority=(priority)
      raise TypeError.new("invalid priority: #{priority}") if priority.nil? || !([Float, Fixnum].include?(priority.class))
      raise RangeError.new("priority out of range: #{priority}") if ((priority > 255) || (priority < 0))
      Cproton.pn_message_set_priority(@impl, priority.floor)
    end

    # Returns the priority.
    #
    def priority
      Cproton.pn_message_get_priority(@impl)
    end

    # Sets the time-to-live for the message.
    #
    # ==== Options
    #
    # * time - the time in milliseconds
    #
    def ttl=(time)
      raise TypeError.new("invalid ttl: #{time}") if time.nil? || !([Float, Fixnum].include?(time.class))
      raise RangeError.new("time out of range: #{time}") if ((time < 0))
      Cproton.pn_message_set_ttl(@impl, time.floor)
    end

    # Returns the time-to-live, in milliseconds.
    #
    def ttl
      Cproton.pn_message_get_ttl(@impl)
    end

    # Sets whether this is the first time the message was acquired.
    #
    # See ::first_acquirer? for more details.
    #
    # ==== Options
    #
    # * state - true if claiming the message
    #
    def first_acquirer=(state)
      raise TypeError.new("invalid state: #{state}") if state.nil? || !([TrueClass, FalseClass].include?(state.class))
      Cproton.pn_message_set_first_acquirer(@impl, state)
    end

    # Sets the delivery count for the message.
    #
    # See ::delivery_count for more details.
    #
    # ==== Options
    #
    # * count - the delivery count
    #
    def delivery_count=(count)
      raise ::ArgumentError.new("invalid count: #{count}") if count.nil? || !([Float, Fixnum].include?(count.class))
      raise RangeError.new("count out of range: #{count}") if count < 0

      Cproton.pn_message_set_delivery_count(@impl, count.floor)
    end

    # Returns the delivery count for the message.
    #
    # This is the number of delivery attempts for the given message.
    #
    def delivery_count
      Cproton.pn_message_get_delivery_count(@impl)
    end

    # Returns whether this is the first acquirer.
    #
    #
    def first_acquirer?
      Cproton.pn_message_is_first_acquirer(@impl)
    end

    # Sets the message id.
    #
    # ==== Options
    #
    # * id = the id
    #
    def id=(id)
      Cproton.pn_message_set_id(@impl, id)
    end

    # Returns the message id.
    #
    def id
      Cproton.pn_message_get_id(@impl)
    end

    # Sets the user id.
    #
    # ==== Options
    #
    # * id - the user id
    #
    def user_id=(id)
      Cproton.pn_message_set_user_id(@impl, id)
    end

    # Returns the user id.
    #
    def user_id
      Cproton.pn_message_get_user_id(@impl)
    end

    # Sets the destination address.
    #
    # ==== Options
    #
    # * address - the address
    #
    def address=(address)
      Cproton.pn_message_set_address(@impl, address)
    end

    # Returns the destination address.
    #
    def address
      Cproton.pn_message_get_address(@impl)
    end

    # Sets the subject.
    #
    # ==== Options
    #
    # * subject - the subject
    #
    def subject=(subject)
      Cproton.pn_message_set_subject(@impl, subject)
    end

    # Returns the subject
    #
    def subject
      Cproton.pn_message_get_subject(@impl)
    end

    # Sets the reply-to address.
    #
    # ==== Options
    #
    # * address - the reply-to address
    #
    def reply_to=(address)
      Cproton.pn_message_set_reply_to(@impl, address)
    end

    # Returns the reply-to address
    #
    def reply_to
      Cproton.pn_message_get_reply_to(@impl)
    end

    # Sets the correlation id.
    #
    # ==== Options
    #
    # * id - the correlation id
    #
    def correlation_id=(id)
      Cproton.pn_message_set_correlation_id(@impl, id)
    end

    # Returns the correlation id.
    #
    def correlation_id
      Cproton.pn_message_get_correlation_id(@impl)
    end

    # Sets the content type.
    #
    # ==== Options
    #
    # * content_type - the content type
    #
    def content_type=(content_type)
      Cproton.pn_message_set_content_type(@impl, content_type)
    end

    # Returns the content type
    #
    def content_type
      Cproton.pn_message_get_content_type(@impl)
    end

    # Sets the message content.
    #
    # *WARNING:* This method has been deprecated. Please use #body= instead to
    # set the content of a message.
    #
    # ==== Options
    #
    # * content - the content
    #
    def content=(content)
      Cproton.pn_message_load(@impl, content)
    end

    # Returns the message content.
    #
    # *WARNING:* This method has been deprecated. Please use #body instead to
    # retrieve the content of a message.
    #
    def content
      size = 16
      loop do
        result = Cproton.pn_message_save(@impl, size)
        error = result[0]
        data = result[1]
        if error == Qpid::Proton::Error::OVERFLOW
          size = size * 2
        else
          check(error)
          return data
        end
      end
    end

    # Sets the content encoding type.
    #
    # ==== Options
    #
    # * encoding - the content encoding
    #
    def content_encoding=(encoding)
      Cproton.pn_message_set_content_encoding(@impl, encoding)
    end

    # Returns the content encoding type.
    #
    def content_encoding
      Cproton.pn_message_get_content_encoding(@impl)
    end

    # Sets the expiration time.
    #
    # ==== Options
    #
    # * time - the expiry time
    #
    def expires=(time)
      raise TypeError.new("invalid expiry time: #{time}") if time.nil?
      raise ::ArgumentError.new("expiry time cannot be negative: #{time}") if time < 0
      Cproton.pn_message_set_expiry_time(@impl, time)
    end

    # Returns the expiration time.
    #
    def expires
      Cproton.pn_message_get_expiry_time(@impl)
    end

    # Sets the creation time.
    #
    # ==== Options
    #
    # * time - the creation time
    #
    def creation_time=(time)
      raise TypeError.new("invalid time: #{time}") if time.nil?
      raise ::ArgumentError.new("time cannot be negative") if time < 0
      Cproton.pn_message_set_creation_time(@impl, time)
    end

    # Returns the creation time.
    #
    def creation_time
      Cproton.pn_message_get_creation_time(@impl)
    end

    # Sets the group id.
    #
    # ==== Options
    #
    # * id - the group id
    #
    def group_id=(id)
      Cproton.pn_message_set_group_id(@impl, id)
    end

    # Returns the group id.
    #
    def group_id
      Cproton.pn_message_get_group_id(@impl)
    end

    # Sets the group sequence number.
    #
    # ==== Options
    #
    # * seq - the sequence number
    #
    def group_sequence=(seq)
      raise TypeError.new("invalid seq: #{seq}") if seq.nil?
      Cproton.pn_message_set_group_sequence(@impl, seq)
    end

    # Returns the group sequence number.
    #
    def group_sequence
      Cproton.pn_message_get_group_sequence(@impl)
    end

    # Sets the reply-to group id.
    #
    # ==== Options
    #
    # * id - the id
    #
    def reply_to_group_id=(id)
      Cproton.pn_message_set_reply_to_group_id(@impl, id)
    end

    # Returns the reply-to group id.
    #
    def reply_to_group_id
      Cproton.pn_message_get_reply_to_group_id(@impl)
    end

    # Returns the list of property names for associated with this message.
    #
    # ==== Examples
    #
    #   msg.properties.each do |name|
    #   end
    #
    def properties
      @properties
    end

    # Replaces the entire set of properties with the specified hash.
    #
    def properties=(properties)
      @properties = properties
    end

    # Assigns the value given to the named property.
    #
    # ==== Arguments
    #
    # * name - the property name
    # * value - the property value
    #
    def []=(name, value)
      @properties[name] = value
    end

    # Retrieves the value for the specified property name. If not found, then
    # it returns nil.
    #
    def [](name)
      @properties[name]
    end

    # Deletes the named property.
    #
    def delete_property(name)
      @properties.delete(name)
    end

    # Returns the instructions for this message.
    #
    def instructions
      @instructions
    end

    # Assigns instructions to this message.
    #
    def instructions=(instr)
      @instructions = instr
    end

    # Returns the annotations for this message.
    #
    def annotations
      @annotations
    end

    # Assigns annotations to this message.
    #
    def annotations=(annotations)
      @annotations = annotations
    end

    # Returns the body property of the message.
    #
    def body
      @body
    end

    # Assigns a new value to the body of the message.
    #
    def body=(body)
      @body = body
    end

    private

    def check(err) # :nodoc:
      if err < 0
        raise DataError, "[#{err}]: #{Cproton.pn_message_error(@data)}"
      else
        return err
      end
    end
  end

end
