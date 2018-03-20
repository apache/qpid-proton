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

  # The SASL layer is responsible for establishing an authenticated and/or
  # encrypted tunnel over which AMQP frames are passed between peers.
  #
  # The peer acting as the SASL client must provide authentication
  # credentials.
  #
  # The peer acting as the SASL server must provide authentication against the
  # received credentials.
  #
  # @note Do not instantiate directly, use {Transport#sasl} to create a SASL object.
  class SASL
    include Util::Deprecation

    # Negotation has not completed.
    NONE = Cproton::PN_SASL_NONE
    # Authentication succeeded.
    OK = Cproton::PN_SASL_OK
    # Authentication failed due to bad credentials.
    AUTH = Cproton::PN_SASL_AUTH

    private

    extend Util::SWIGClassHelper
    PROTON_METHOD_PREFIX = "pn_sasl"

    public

    # @private
    # @note Do not instantiate directly, use {Transport#sasl} to create a SASL object.
    def initialize(transport)
      @impl = Cproton.pn_sasl(transport.impl)
    end

    # @!attribute allow_insecure_mechs
    #   @return [Bool] true if clear text authentication is allowed on insecure connections.
    proton_set_get :allow_insecure_mechs

    # @!attribute user [r]
    #   @return [String] the authenticated user name
    proton_get :user

    # Set the mechanisms allowed for SASL negotation
    # @param mechanisms [String] space-delimited list of allowed mechanisms
    def allowed_mechs=(mechanisms)
      Cproton.pn_sasl_allowed_mechs(@impl, mechanisms)
    end

    # @deprecated use {#allowed_mechs=}
    deprecated_alias :mechanisms, :allowed_mechs=

    # True if extended SASL negotiation is supported
    #
    # All implementations of Proton support ANONYMOUS and EXTERNAL on both
    # client and server sides and PLAIN on the client side.
    #
    # Extended SASL implememtations use an external library (Cyrus SASL)
    # to support other mechanisms.
    #
    # @return [Bool] true if extended SASL negotiation is supported
    def self.extended?()
      Cproton.pn_sasl_extended()
    end

    class << self
      include Util::Deprecation

      # Set the sasl configuration path
      #
      # This is used to tell SASL where to look for the configuration file.
      # In the current implementation it can be a colon separated list of directories.
      #
      # The environment variable PN_SASL_CONFIG_PATH can also be used to set this path,
      # but if both methods are used then this pn_sasl_config_path() will take precedence.
      #
      # If not set the underlying implementation default will be used.
      #
      # @param path the configuration path
      #
      def config_path=(path)
        Cproton.pn_sasl_config_path(nil, path)
        path
      end

      # Set the configuration file name, without extension
      #
      # The name with an a ".conf" extension will be searched for in the
      # configuration path.  If not set, it defaults to "proton-server" or
      # "proton-client" for a server (incoming) or client (outgoing) connection
      # respectively.
      #
      # @param name the configuration file name without extension
      #
      def config_name=(name)
        Cproton.pn_sasl_config_name(nil, name)
      end

      # @deprecated use {config_path=}
      deprecated_alias :config_path, :config_path=
      # @deprecated use {config_name=}
      deprecated_alias :config_name, :config_name=
    end
  end
end
