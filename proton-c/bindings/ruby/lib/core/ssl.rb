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

  # The SSL support for Transport.
  #
  # A Transport may be configured ot use SLL for encryption and/or
  # authentication. A Transport can be configured as either the SSL
  # client or the server. An SSL client is the party that proctively
  # establishes a connection to an SSL server. An SSL server is the
  # party that accepts a connection request from the remote SSL client.
  #
  # If either the client or the server needs to identify itself with the
  # remote node, it must have its SSL certificate configured.
  #
  # @see SSLDomain#credentials For setting the SSL certificate.
  #
  # If either the client or the server needs to verify the identify of the
  # remote node, it must have its database of trusted CAs configured.
  #
  # @see SSLDomain#trusted_ca_db Setting the CA database.
  #
  # An SSL server connection may allow the remote client to connect without
  # SS (i.e., "in the clear").
  #
  # @see SSLDomain#allow_unsecured_client Allowing unsecured clients.
  #
  # The level of verification required of the remote may be configured.
  #
  # @see SSLDomain#peer_authentication Setting peer authentication.
  #
  # Support for SSL client session resume is provided as well.
  #
  # @see SSLDomain
  # @see #resume_status
  #
  class SSL

    # Session resume state is unkonnwn or not supported.
    RESUME_UNKNOWN = Cproton::PN_SSL_RESUME_UNKNOWN
    # Session renegotiated and not resumed.
    RESUME_NEW = Cproton::PN_SSL_RESUME_NEW
    # Session resumed from the previous session.
    RESUME_REUSED = Cproton::PN_SSL_RESUME_REUSED

    # @private
    include Util::SwigHelper

    # @private
    PROTON_METHOD_PREFIX = "pn_ssl"

    # @private
    include Util::ErrorHandler

    can_raise_error :peer_hostname=, :error_class => SSLError

    # Returns whether SSL is supported.
    #
    # @return [Boolean] True if SSL support is available.
    #
    def self.present?
      Cproton.pn_ssl_present
    end

    # @private
    def self.create(transport, domain, session_details = nil)
      result = nil
      # like python, make sure we're not creating a different SSL
      # object for a transport with an existing SSL object
      if transport.ssl?
        transport.instance_eval { result = @ssl }
        if ((!domain.nil? && (result.domain != domain)) ||
            (!session_details.nil? && (result.session_details != session_details)))
          raise SSLException.new("cannot re-configure existing SSL object")
        end
      else
        impl = Cproton.pn_ssl(transport.impl)
        session_id = nil
        session_id = session_details.session_id unless session_details.nil?
        result = SSL.new(impl, domain, session_details, session_id)
      end
      return result
    end

    private

    def initialize(impl, domain, session_details, session_id)
      @impl = impl
      @domain = domain.impl unless domain.nil?
      @session_details = session_details
      @session_id = session_id
      Cproton.pn_ssl_init(@impl, @domain, @session_id)
    end

    public

    # Returns the cipher name that is currently in used.
    #
    # Gets the text description of the cipher that is currently active, or
    # returns nil if SSL is not active. Note that the cipher in use my change
    # over time due to renegotiation or other changes to the SSL layer.
    #
    # @return [String, nil] The cipher name.
    #
    def cipher_name
      rc, name = Cproton.pn_ssl_get_cipher_name(@impl, 128)
      return name if rc
      nil
    end

    # Returns the name of the SSL protocol that is currently active, or
    # returns nil if SSL is nota ctive. Not that the protocol may change over
    # time due to renegotation.
    #
    # @return [String, nil] The protocol name.
    #
    def protocol_name
      rc, name = Cproton.pn_ssl_get_protocol_name(@impl, 128)
      retur name if rc
      nil
    end

    # Checks whether or not the state has resumed.
    #
    # Used for client session resume. When called on an active session, it
    # indicates wehther the state has been resumed from a previous session.
    #
    # *NOTE:* This is a best-effort service - there is no guarantee that the
    # remote server will accept the resumed parameters. The remote server may
    # choose to ignore these parameters, and request a renegotation instead.
    #
    def resume_status
      Cproton.pn_ssl_resume_status(@impl)
    end

    # Gets the peer hostname.
    #
    # @return [String] The peer hostname.
    def peer_hostname
      (error, name) = Cproton.pn_ssl_get_peer_hostname(@impl, 1024)
      raise SSLError.new if error < 0
      return name
    end

  end

end
