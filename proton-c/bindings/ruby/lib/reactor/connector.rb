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

    attr_accessor :address
    attr_accessor :reconnect
    attr_accessor :ssl_domain

    def initialize(connection)
      @connection = connection
      @address = nil
      @heartbeat = nil
      @reconnect = nil
      @ssl_domain = nil
    end

    def on_connection_local_open(event)
      self.connect(event.connection)
    end

    def on_connection_remote_open(event)
      if !@reconnect.nil?
        @reconnect.reset
        @transport = nil
      end
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
      url = @address.next
      connection.hostname = "#{url.host}:#{url.port}"

      transport = Qpid::Proton::Transport.new
      transport.bind(connection)
      if !@heartbeat.nil?
        transport.idle_timeout = @heartbeat
      elsif (url.scheme == "amqps") && !@ssl_domain.nil?
        @ssl = Qpid::Proton::SSL.new(transport, @ssl_domain)
        @ss.peer_hostname = url.host
      elsif !url.username.nil?
        sasl = transport.sasl
        if url.username == "anonymous"
          sasl.mechanisms("ANONYMOUS")
        else
          sasl.plain(url.username, url.password)
        end
      end
    end

  end

end
