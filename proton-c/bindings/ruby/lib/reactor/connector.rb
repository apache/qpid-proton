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

module Qpid::Proton::Reactor

  class Connector < Qpid::Proton::BaseHandler

    def initialize(connection, url, opts)
      @connection, @opts = connection, opts
      @urls = URLs.new(url) if url
      opts.each do |k,v|
        case k
        when :url, :urls, :address
          @urls = URLs.new(v) unless @urls
        when :reconnect
          @reconnect = v
        end
      end
      raise ::ArgumentError.new("no url for connect") unless @urls

      # TODO aconway 2017-08-17: review reconnect configuration and defaults
      @reconnect = Backoff.new() unless @reconnect
      @ssl_domain = SessionPerConnection.new # TODO seems this should be configurable
      @connection.overrides = self
      @connection.open
    end

    def on_connection_local_open(event)
      self.connect(event.connection)
    end

    def on_connection_remote_open(event)
      @reconnect.reset if @reconnect
    end

    def on_transport_tail_closed(event)
      self.on_transport_closed(event)
    end

    def on_transport_closed(event)
      if !@connection.nil? && !(@connection.state & Qpid::Proton::Endpoint::LOCAL_ACTIVE).zero?
        if !@reconnect.nil?
          event.transport.unbind
          delay = @reconnect.next
          if delay == 0
            self.connect(@connection)
          else
            event.reactor.schedule(delay, self)
          end
        else
          @connection = nil
        end
      end
    end

    def on_timer_task(event)
      self.connect(@connection)
    end

    def on_connection_remote_close(event)
      @connection = nil
    end

    def connect(connection)
      url = @urls.next
      transport = Qpid::Proton::Transport.new
      @opts.each do |k,v|
        case k
        when :user
          connection.user = v
        when :password
          connection.password = v
        when :heartbeat
          transport.idle_timeout = v.to_i
        when :idle_timeout
          transport.idle_timeout = v.(v*1000).to_i
        when :sasl_enabled
          transport.sasl if v
        when :sasl_allow_insecure_mechs
          transport.sasl.allow_insecure_mechs = v
        when :sasl_allowed_mechs, :sasl_mechanisms
          transport.sasl.allowed_mechs = v
        end
      end

      # TODO aconway 2017-08-11: hostname setting is incorrect, reactor only
      connection.hostname = "#{url.host}:#{url.port}"
      connection.user = url.username if url.username && !url.username.empty?
      connection.password = url.password if url.password && !url.password.empty?

      transport.bind(connection)

      if (url.scheme == "amqps") && @ssl_domain
        @ssl = Qpid::Proton::SSL.new(transport, @ssl_domain)
        @ssl.peer_hostname = url.host
      end
    end
  end
end
