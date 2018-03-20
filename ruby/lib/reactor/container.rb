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
  module Reactor

    # @deprecated use {Qpid::Proton::Container}
    class Container < Qpid::Proton::Container
      include Util::Deprecation

      private
      alias super_connect connect # Access to superclass method

      public

      # @deprecated use {Qpid::Proton::Container}
      def initialize(handlers, opts=nil)
        deprecated Qpid::Proton::Reactor::Container, Qpid::Proton::Container
        h = handlers || (opts && opts[:global_handler]) || Handler::ReactorMessagingAdapter.new(nil)
        id = opts && opts[:container_id]
        super(h, id)
      end

      alias container_id id
      alias global_handler handler

      def connect(opts=nil)
        url = opts && (opts[:url] || opts[:address])
        raise ::ArgumentError.new, "no :url or :address option provided" unless url
        super(url, opts)
      end

      def create_sender(context, opts=nil)
        c = context if context.is_a? Qpid::Proton::Connection
        unless c
          url = Qpid::Proton::uri context
          c = super_connect(url, opts)
          opts ||= {}
          opts[:target] ||= url.amqp_address
        end
        c.open_sender opts
      end

      def create_receiver(context, opts=nil)
        c = context if context.is_a? Qpid::Proton::Connection
        unless c
          url = Qpid::Proton::uri context
          c = super_connect(url, opts)
          opts ||= {}
          opts[:source] ||= url.amqp_address
        end
        c.open_receiver opts
      end

      def listen(url, ssl_domain = nil)
        # TODO aconway 2017-11-29: ssl_domain
        super(url)
      end
    end
  end
end
