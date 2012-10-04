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

    # A +Messenger+ provides a high-level means for sending and
    # receiving AMQP messages.
    #
    # ==== Examples
    #
    class Messenger

      include Qpid::Proton::ExceptionHandling

      # Creates a new +Messenger+.
      #
      # The +name+ parameter is optional. If one is not provided then
      # a unique name is generated.
      #
      # ==== Options
      #
      # * name - the name (def. nil)
      #
      def initialize(name = nil)
        @impl = Cproton.pn_messenger(name)
        ObjectSpace.define_finalizer(self, self.class.finalize!(@impl))
      end

      def self.finalize!(impl) # :nodoc:
        proc {
          Cproton.pn_messenger_stop(impl)
          Cproton.pn_messenger_free(impl)
        }
      end

      # Returns the name.
      #
      def name
        Cproton.pn_messenger_name(@impl)
      end

      # Sets the timeout period, in milliseconds.
      #
      # A negative timeout period implies an infinite timeout.
      #
      # ==== Options
      #
      # * timeout - the timeout period
      #
      def timeout=(timeout)
        raise TypeError.new("invalid timeout: #{timeout}") if timeout.nil?
        Cproton.pn_messenger_set_timeout(@impl, timeout)
      end

      # Returns the timeout period
      #
      def timeout
        Cproton.pn_messenger_get_timeout(@impl)
      end

      # Reports whether an error occurred.
      #
      def error?
        !Cproton.pn_messenger_errno(@impl).zero?
      end

      # Returns the most recent error number.
      #
      def errno
        Cproton.pn_messenger_errno(@impl)
      end

      # Returns the most recent error message.
      #
      def error
        Cproton.pn_messenger_error(@impl)
      end

      # Starts the +Messenger+, allowing it to begin sending and
      # receiving messages.
      #
      def start
        check_for_error(Cproton.pn_messenger_start(@impl))
      end

      # Stops the +Messenger+, preventing it from sending or receiving
      # any more messages.
      #
      def stop
        check_for_error(Cproton.pn_messenger_stop(@impl))
      end

      # Subscribes the +Messenger+ to a remote address.
      #
      def subscribe(address)
        raise TypeError.new("invalid address: #{address}") if address.nil?
        check_for_error(Cproton.pn_messenger_subscribe(@impl, address))
      end

      # Path to a certificate file for the +Messenger+.
      #
      # This certificate is used when the +Messenger+ accepts or establishes
      # SSL/TLS connections.
      #
      # ==== Options
      #
      # * certificate - the certificate
      #
      def certificate=(certificate)
        Cproton.pn_messenger_set_certificate(@impl, certificate)
      end

      # Returns the path to a certificate file.
      #
      def certificate
        Cproton.pn_messenger_get_certificate(@impl)
      end

      # Path to a private key file for the +Messenger+.
      #
      # The property must be specified for the +Messenger+ to accept incoming
      # SSL/TLS connections and to establish client authenticated outgoing
      # SSL/TLS connections.
      #
      # ==== Options
      #
      # * key - the key file
      #
      def private_key=(key)
        Cproton.pn_messenger_set_private_key(@impl, key)
      end

      # Returns the path to a private key file.
      #
      def private_key
        Cproton.pn_messenger_get_private_key(@impl)
      end

      # A path to a database of trusted certificates for use in verifying the
      # peer on an SSL/TLS connection. If this property is +nil+, then the
      # peer will not be verified.
      #
      # ==== Options
      #
      # * certificates - the certificates path
      #
      def trusted_certificates=(certificates)
        Cproton.pn_messenger_set_trusted_certificates(@impl,certificates)
      end

      # The path to the databse of trusted certificates.
      #
      def trusted_certificates
        Cproton.pn_messenger_get_trusted_certificates(@impl)
      end

      # Puts a single message into the outgoing queue.
      #
      # To ensure messages are sent, you should then call ::send.
      #
      # ==== Options
      #
      # * message - the message
      #
      def put(message)
        raise TypeError.new("invalid message: #{message}") if message.nil?
        raise ArgumentError.new("invalid message type: #{message.class}") unless message.kind_of?(Message)
        check_for_error(Cproton.pn_messenger_put(@impl, message.impl))
      end

      # Sends all outgoing messages, blocking until the outgoing queue
      # is empty.
      #
      def send
        check_for_error(Cproton.pn_messenger_send(@impl))
      end

      # Gets a single message incoming message from the local queue.
      #
      # If no message is provided in the argument, then one is created. In
      # either case, the one returned will be the fetched message.
      #
      # ==== Options
      #
      # * msg - the (optional) +Message+ instance to be used
      #
      def get(msg = nil)
        msg = Qpid::Proton::Message.new if msg.nil?
        check_for_error(Cproton.pn_messenger_get(@impl, msg.impl))
        return msg
      end

      # Receives up to the specified number of messages, blocking until at least
      # one message is received.
      #
      # Options ====
      #
      # * max - the maximum number of messages to receive
      #
      def receive(max)
        raise TypeError.new("invalid max: #{max}") if max.nil? || max.to_i.zero?
        raise RangeError.new("negative max: #{max}") if max < 0
        check_for_error(Cproton.pn_messenger_recv(@impl, max))
      end

      # Returns the number messages in the outgoing queue that have not been
      # transmitted.
      #
      def outgoing
        Cproton.pn_messenger_outgoing(@impl)
      end

      # Returns the number of messages in the incoming queue that have not
      # been retrieved.
      #
      def incoming
        Cproton.pn_messenger_incoming(@impl)
      end

    end

  end

end
