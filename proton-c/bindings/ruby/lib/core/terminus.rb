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
    include Util::SwigHelper

    # @private
    PROTON_METHOD_PREFIX = "pn_terminus"

    # @!attribute type
    #
    # @return [Fixnum] The terminus type.
    #
    # @see SOURCE
    # @see TARGET
    # @see COORDINATOR
    #
    proton_accessor :type

    # @!attribute address
    #
    # @return [String] The terminus address.
    #
    proton_accessor :address

    # @!attribute durability
    #
    # @return [Fixnum] The durability mode of the terminus.
    #
    # @see NONDURABLE
    # @see CONFIGURATION
    # @see DELIVERIES
    #
    proton_accessor :durability

    # @!attribute expiry_policy
    #
    # @return [Fixnum] The expiry policy.
    #
    # @see EXPIRE_WITH_LINK
    # @see EXPIRE_WITH_SESSION
    # @see EXPIRE_WITH_CONNECTION
    # @see EXPIRE_NEVER
    #
    proton_accessor :expiry_policy

    # @!attribute timeout
    #
    # @return [Fixnum] The timeout period.
    #
    proton_accessor :timeout

    # @!attribute dynamic?
    #
    # @return [Boolean] True if the terminus is dynamic.
    #
    proton_accessor :dynamic, :is_or_get => :is

    # @!attribute distribution_mode
    #
    # @return [Fixnum] The distribution mode.
    #
    # @see DIST_MODE_UNSPECIFIED
    # @see DIST_MODE_COPY
    # @see DIST_MODE_MOVE
    #
    proton_accessor :distribution_mode

    # @private
    include Util::ErrorHandler

    can_raise_error [:type=, :address=, :durability=, :expiry_policy=,
                          :timeout=, :dynamic=, :distribution_mode=, :copy],
                    :error_class => Qpid::Proton::LinkError

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
      Data.new(Cproton.pn_terminus_properties(@impl))
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
      Data.new(Cproton.pn_terminus_capabilities(@impl))
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
      Data.new(Cproton.pn_terminus_outcomes(@impl))
    end

    # Access and modify the AMQP filter set for the Terminus.
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
      Data.new(Cproton.pn_terminus_filter(@impl))
    end

    # Copy another Terminus into this instance.
    #
    # @param source [Terminus] The source instance.
    #
    def copy(source)
      Cproton.pn_terminus_copy(@impl,source.impl)
    end

  end

end
