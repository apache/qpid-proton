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

  # The top-level object that stores the configuration used by one or more
  # SSL sessions.
  #
  # @see SSL
  #
  class SSLDomain

    # The local connection endpoint is an SSL client.
    # @private
    MODE_CLIENT = Cproton::PN_SSL_MODE_CLIENT
    # The local connection endpoint is an SSL server.
    # @private
    MODE_SERVER = Cproton::PN_SSL_MODE_SERVER

    # Require the peer to provide a valid identifying certificate.
    VERIFY_PEER = Cproton::PN_SSL_VERIFY_PEER
    # Do no require a certificate nor a cipher authorization.
    ANONYMOUS_PEER = Cproton::PN_SSL_ANONYMOUS_PEER
    # Require a valid certficate and matching name.
    VERIFY_PEER_NAME = Cproton::PN_SSL_VERIFY_PEER_NAME

    # @private
    include Util::ErrorHandler

    can_raise_error :credentials, :error_class => Qpid::Proton::SSLError
    can_raise_error :trusted_ca_db, :error_class => Qpid::Proton::SSLError
    can_raise_error :peer_authentication, :error_class => Qpid::Proton::SSLError
    can_raise_error :allow_unsecured_client, :error_class => Qpid::Proton::SSLError

    # @private
    attr_reader :impl

    # @private
    def initialize(mode)
      @impl = Cproton.pn_ssl_domain(mode)
      raise SSLUnavailable.new if @impl.nil?
    end

    # Set the certificate that identifies the local node to the remote.
    #
    # This certificate establishes the identity for thelocal node for all SSL
    # sessions created from this domain. It will be sent to the remote if the
    # remote needs to verify the dientify of this node. This may be used for
    # both SSL servers and SSL clients (if client authentication is required by
    # the server).
    #
    # *NOTE:* This setting affects only those instances of SSL created *after*
    # this call returns. SSL objects created before invoking this method will
    # use the domain's previous settings.
    #
    # @param cert_file [String] The filename containing the identify
    #  certificate. For OpenSSL users, this is a PEM file. For Windows SChannel
    #  users, this is the PKCS\#12 file or system store.
    # @param key_file [String] An option key to access the identifying
    #  certificate. For OpenSSL users, this is an optional PEM file containing
    #  the private key used to sign the certificate. For Windows SChannel users,
    #  this is the friendly name of the self-identifying certficate if there are
    #  multiple certfificates in the store.
    # @param password [String] The password used to sign the key, or *nil* if
    #  the key is not protected.
    #
    # @raise [SSLError] If an error occurs.
    #
    def credentials(cert_file, key_file, password)
      Cproton.pn_ssl_domain_set_credentials(@impl,
                                            cert_file, key_file, password)
    end

    # Configures the set of trusted CA certificates used by this domain to
    # verify peers.
    #
    # If the local SSL client/server needs to verify the identify of the remote,
    # it must validate the signature of the remote's certificate. This function
    # sets the database of trusted CAs that will be used to verify the signature
    # of the remote's certificate.
    #
    # *NOTE:# This setting affects only those SSL instances created *after* this
    # call returns. SSL objects created before invoking this method will use the
    # domain's previous setting.
    #
    # @param certificate_db [String] The filename for the databse of trusted
    #   CAs, used to authenticate the peer.
    #
    # @raise [SSLError] If an error occurs.
    #
    def trusted_ca_db(certificate_db)
      Cproton.pn_ssl_domain_set_trusted_ca_db(@impl, certificate_db)
    end

    # Configures the level of verification used on the peer certificate.
    #
    # This method congtrols how the peer's certificate is validated, if at all.
    # By default, neither servers nor clients attempt to verify their peers
    # (*ANONYMOUS_PEER*). Once certficates and trusted CAs are configured, peer
    # verification can be enabled.
    #
    # *NOTE:* In order to verify a peer, a trusted CA must be configured.
    #
    # *NOTE:* Servers must provide their own certficate when verifying a peer.
    #
    # *NOTE:* This setting affects only those SSL instances created after this
    # call returns. SSL instances created before invoking this method will use
    # the domain's previous setting.
    #
    # @param verify_mode [Fixnum] The level of validation to apply to the peer.
    # @param trusted_CAs [String] The path to a database of trusted CAs that
    #   the server will advertise to the peer client if the server has been
    #   configured to verify its peer.
    #
    # @see VERIFY_PEER
    # @see ANONYMOUS_PEER
    # @see VERIFY_PEER_NAME
    #
    # @raise [SSLError] If an error occurs.
    #
    def peer_authentication(verify_mode, trusted_CAs = nil)
      Cproton.pn_ssl_domain_set_peer_authentication(@impl,
                                                    verify_mode, trusted_CAs)
    end

    # Permit a server to accept connection requests from non-SSL clients.
    #
    # This configures the server to "sniff" the incomfing client data stream and
    # dynamically determine whether SSL/TLS is being used. This option is
    # disabled by default: only clients using SSL/TLS are accepted by default.
    #
    # @raise [SSLError] If an error occurs.
    #
    def allow_unsecured_client
      Cproton.pn_ssl_domain_allow_unsecured_client(@impl);
    end

  end

end
