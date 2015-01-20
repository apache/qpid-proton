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

  # The SASL layer is responsible for establishing an authenticated and/or
  # encrypted tunnel over which AMQP frames are passed between peers.
  #
  # The peer acting as the SASL client must provide authentication
  # credentials.
  #
  # The peer acting as the SASL server must provide authentication against the
  # received credentials.
  #
  # @example
  #   # SCENARIO: the remote endpoint has not initialized their connection
  #   #           then the local endpoint, acting as a SASL server, decides
  #   #           to allow an anonymous connection.
  #   #
  #   #           The SASL layer locally assumes the role of server and then
  #   #           enables anonymous authentication for the remote endpoint.
  #   #
  #   sasl = @transport.sasl
  #   sasl.server
  #   sasl.mechanisms("ANONYMOUS")
  #   sasl.done(Qpid::Proton::SASL::OK)
  #
  class SASL

    # Negotation has not completed.
    NONE = Cproton::PN_SASL_NONE
    # Authentication succeeded.
    OK = Cproton::PN_SASL_OK
    # Authentication failed due to bad credentials.
    AUTH = Cproton::PN_SASL_AUTH

    # Constructs a new instance for the given transport.
    #
    # @param transport [Transport] The transport.
    #
    # @private A SASL should be fetched only from its Transport
    #
    def initialize(transport)
      @impl = Cproton.pn_sasl(transport.impl)
    end

    # Sets the acceptable SASL mechanisms.
    #
    # @param mechanisms [String] The space-delimited set of mechanisms.
    #
    # @example Use anonymous SASL authentication.
    #  @sasl.mechanisms("GSSAPI CRAM-MD5 PLAIN")
    #
    def mechanisms(mechanisms)
      Cproton.pn_sasl_mechanisms(@impl, mechanisms)
    end

    # Returns the outcome of the SASL negotiation.
    #
    # @return [Fixnum] The outcome.
    #
    def outcome
      outcome = Cprotn.pn_sasl_outcome(@impl)
      return nil if outcome == NONE
      outcome
    end

    # Set the condition of the SASL negotiation.
    #
    # @param outcome [Fixnum] The outcome.
    #
    def done(outcome)
      Cproton.pn_sasl_done(@impl, outcome)
    end

  end

end
