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


module Qpid::Proton

  # Represents an endpoint for an AMQP connection..
  #
  # An AMQP terminus acts as either a source or a target for messages,
  # but never as both. Every Link is associated iwth both a source and
  # a target Terminus that is negotiated during link establishment.
  #
  # A terminus is composed of an AMQP address along with a number of
  # other properties defining the quality of service and behavior of
  # the Link.
  #
  class Terminus
    include Util::Deprecation

    # Indicates a non-existent source or target terminus.
    UNSPECIFIED = Cproton::PN_UNSPECIFIED
    # Indicates a source for messages.
    SOURCE = Cproton::PN_SOURCE
    # Indicates a target for messages.
    TARGET = Cproton::PN_TARGET
    # A special target identifying a transaction coordinator.
    COORDINATOR = Cproton::PN_COORDINATOR

    # The terminus is orphaned when the parent link is closed.
    EXPIRE_WITH_LINK = Cproton::PN_EXPIRE_WITH_LINK
    # The terminus is orphaned whent he parent sessio is closed.
    EXPIRE_WITH_SESSION = Cproton::PN_EXPIRE_WITH_SESSION
    # The terminus is orphaned when the parent connection is closed.
    EXPIRE_WITH_CONNECTION = Cproton::PN_EXPIRE_WITH_CONNECTION
    # The terminus is never considered orphaned.
    EXPIRE_NEVER = Cproton::PN_EXPIRE_NEVER

    # Indicates a non-durable Terminus.
    NONDURABLE = Cproton::PN_NONDURABLE
    # Indicates a Terminus with durably held configuration, but
    # not the delivery state.
    CONFIGURATION = Cproton::PN_CONFIGURATION
    # Indicates a Terminus with both durably held configuration and
    # durably held delivery states.
    DELIVERIES = Cproton::PN_DELIVERIES

    # The behavior is defined by the nod.e
    DIST_MODE_UNSPECIFIED = Cproton::PN_DIST_MODE_UNSPECIFIED
    # The receiver gets all messages.
    DIST_MODE_COPY = Cproton::PN_DIST_MODE_COPY
    # The receives compete for messages.
    DIST_MODE_MOVE = Cproton::PN_DIST_MODE_MOVE

    # @private
    PROTON_METHOD_PREFIX = "pn_terminus"
    # @private
    extend Util::SWIGClassHelper

    # @!attribute type
    #
    # @return [Integer] The terminus type.
    #
    # @see SOURCE
    # @see TARGET
    # @see COORDINATOR
    #
    proton_set_get :type

    # @!attribute address
    #
    # @return [String] The terminus address.
    #
    proton_set_get :address

    # @!attribute durability_mode
    #
    # @return [Integer] The durability mode of the terminus.
    #
    # @see NONDURABLE
    # @see CONFIGURATION
    # @see DELIVERIES
    #
    proton_forward :durability_mode, :get_durability
    proton_forward :durability_mode=, :set_durability

    deprecated_alias :durability, :durability_mode
    deprecated_alias :durability=, :durability_mode=

    # @!attribute expiry_policy
    #
    # @return [Integer] The expiry policy.
    #
    # @see EXPIRE_WITH_LINK
    # @see EXPIRE_WITH_SESSION
    # @see EXPIRE_WITH_CONNECTION
    # @see EXPIRE_NEVER
    #
    proton_set_get :expiry_policy

    # @!attribute timeout
    #
    # @return [Integer] The timeout period.
    #
    proton_set_get :timeout

    # @!attribute dynamic?
    #
    # @return [Boolean] True if the terminus is dynamic.
    #
    proton_set_is :dynamic

    # @!attribute distribution_mode
    #
    # @return [Integer] The distribution mode. Only relevant for a message source.
    #
    # @see DIST_MODE_UNSPECIFIED
    # @see DIST_MODE_COPY
    # @see DIST_MODE_MOVE
    #
    proton_set_get :distribution_mode

    # @private
    include Util::ErrorHandler

    # @private
    attr_reader :impl

    # @private
    def initialize(impl)
      @impl = impl
    end

    # Access and modify the AMQP properties data for the Terminus.
    #
    # This operation will return an instance of Data that is valid until the
    # Terminus is freed due to its parent being freed. Any data contained in
    # the object will be sent as the AMQP properties for the parent Terminus
    # instance.
    #
    # NOTE: this MUST take the form of a symbol keyed map to be valid.
    #
    # @return [Data] The terminus properties.
    #
    def properties
      Codec::Data.new(Cproton.pn_terminus_properties(@impl))
    end

    # Access and modify the AMQP capabilities data for the Terminus.
    #
    # This operation will return an instance of Data that is valid until the
    # Terminus is freed due to its parent being freed. Any data contained in
    # the object will be sent as the AMQP properties for the parent Terminus
    # instance.
    #
    # NOTE: this MUST take the form of a symbol keyed map to be valid.
    #
    # @return [Data] The terminus capabilities.
    #
    def capabilities
      Codec::Data.new(Cproton.pn_terminus_capabilities(@impl))
    end

    # Access and modify the AMQP outcomes for the Terminus.
    #
    # This operaiton will return an instance of Data that is valid until the
    # Terminus is freed due to its parent being freed. Any data contained in
    # the object will be sent as the AMQP properties for the parent Terminus
    # instance.
    #
    # NOTE: this MUST take the form of a symbol keyed map to be valid.
    #
    # @return [Data] The terminus outcomes.
    #
    def outcomes
      Codec::Data.new(Cproton.pn_terminus_outcomes(@impl))
    end

    # Access and modify the AMQP filter set for a source terminus.
    # Only relevant for a message source.
    #
    # This operation will return an instance of Data that is valid until the
    # Terminus is freed due to its parent being freed. Any data contained in
    # the object will be sent as the AMQP properties for the parent Terminus
    # instance.
    #
    # NOTE: this MUST take the form of a symbol keyed map to be valid.
    #
    # @return [Data] The terminus filter.
    #
    def filter
      Codec::Data.new(Cproton.pn_terminus_filter(@impl))
    end

    # Replace the data in this Terminus with the contents of +other+
    # @param other [Terminus] The other instance.
    def replace(other)
      Cproton.pn_terminus_copy(@impl, other.impl)
      self
    end
    deprecated_alias :copy, :replace

    # Apply options to this terminus.
    # @option opts [String] :address the node address
    # @option opts [Boolean] :dynamic (false)
    #   if true, request a new node with a unique address to be created. +:address+ is ignored.
    # @option opts [Integer] :distribution_mode see {#distribution_mode}, only for source nodes
    # @option opts [Integer] :durability_mode see {#durability_mode}
    # @option opts [Integer] :timeout see {#timeout}
    # @option opts [Integer] :expiry_policy see {#expiry_policy}
    # @option opts [Hash] :filter see {#filter}, only for source nodes
    # @option opts [Hash] :capabilities see {#capabilities}
    def apply(opts=nil)
      return unless opts
      if opts.is_a? String      # Shorthand for address
        self.address = opts
      else
        opts.each_pair do |k,v|
          case k
          when :address then self.address = v
          when :dynamic then self.dynamic = !!v
          when :distribution_mode then self.distribution_mode = v
          when :durability_mode then self.durability_mode = v
          when :timeout then self.timeout = v.round # Should be integer seconds
          when :expiry_policy then self.expiry_policy = v
          when :filter then self.filter << v
          when :capabilities then self.capabilities << v
          end
        end
      end
    end

    def inspect()
      "\#<#{self.class}: address=#{address.inspect} dynamic?=#{dynamic?.inspect}>"
    end

    def to_s() inspect; end

    can_raise_error([:type=, :address=, :durability=, :expiry_policy=,
                     :timeout=, :dynamic=, :distribution_mode=, :copy],
                    :error_class => Qpid::Proton::LinkError)
  end
end
