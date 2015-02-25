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

module Qpid::Proton::Handler

  # A utility that exposes endpoint events; i.e., the open/close of a link,
  # session or connection, in a more intuitive manner.
  #
  # A XXX_opened method will be called when both local and remote peers have
  # opened the link, session or connection. This can be used to confirm a
  # locally initiated action for example.
  #
  # A XXX_opening method will be called when the remote peer has requested
  # an open that was not initiated locally. By default this will simply open
  # locally, which then trigtgers the XXX_opened called.
  #
  # The same applies to close.
  #
  class EndpointStateHandler < Qpid::Proton::BaseHandler

    def initialize(peer_close_is_error = false, delegate = nil)
      @delegate = delegate
      @peer_close_is_error = peer_close_is_error
    end

    def self.print_error(endpoint, endpoint_type)
      if !endpoint.remote_condition.nil?
      elsif self.local_endpoint?(endpoint) && endpoint.remote_closed?
        logging.error("#{endpoint_type} closed by peer")
      end
    end

    def on_link_remote_close(event)
      if !event.link.remote_condition.nil?
        self.on_link_error(event)
      elsif event.link.local_closed?
        self.on_link_closed(event)
      else
        self.on_link_closing(event)
      end
      event.link.close
    end

    def on_session_remote_close(event)
      if !event.session.remote_condition.nil?
        self.on_session_error(event)
      elsif event.session.local_closed?
        self.on_session_closed(event)
      else
        self.on_session_closing(event)
      end
      event.session.close
    end

    def on_connection_remote_close(event)
      if !event.connection.remote_condition.nil?
        self.on_connection_error(event)
      elsif event.connection.local_closed?
        self.on_connection_closed(event)
      else
        self.on_connection_closing(event)
      end
      event.connection.close
    end

    def on_connection_local_open(event)
      self.on_connection_opened(event) if event.connection.remote_active?
    end

    def on_connection_remote_open(event)
      if !(event.connection.state & Qpid::Proton::Endpoint::LOCAL_ACTIVE).zero?
        self.on_connection_opened(event)
      elsif event.connection.local_uninit?
        self.on_connection_opening(event)
        event.connection.open
      end
    end

    def on_session_local_open(event)
      self.on_session_opened(event) if event.session.remote_active?
    end

    def on_session_remote_open(event)
      if !(event.session.state & Qpid::Proton::Endpoint::LOCAL_ACTIVE).zero?
        self.on_session_opened(event)
      elsif event.session.local_uninit?
        self.on_session_opening(event)
        event.session.open
      end
    end

    def on_link_local_open(event)
      self.on_link_opened(event) if event.link.remote_active?
    end

    def on_link_remote_open(event)
      if !(event.link.state & Qpid::Proton::Endpoint::LOCAL_ACTIVE).zero?
        self.on_link_opened(event)
      elsif event.link.local_uninit?
        self.on_link_opening(event)
        event.link.open
      end
    end

    def on_connection_opened(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_session_opened, event) if !@delegate.nil?
    end

    def on_session_opened(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_session_opened, event) if !@delegate.nil?
    end

    def on_link_opened(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_link_opened, event) if !@delegate.nil?
    end

    def on_connection_opening(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_connection_opening, event) if !@delegate.nil?
    end

    def on_session_opening(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_session_opening, event) if !@delegate.nil?
    end

    def on_link_opening(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_link_opening, event) if !@delegate.nil?
    end

    def on_connection_error(event)
      if !@delegate.nil?
        Qpid::Proton::Event.dispatch(@delegate, :on_connection_error, event)
      else
        self.log_error(event.connection, "connection")
      end
    end

    def on_session_error(event)
      if !@delegate.nil?
        Qpid::Proton::Event.dispatch(@delegate, :on_session_error, event)
      else
        self.log_error(event.session, "session")
        event.connection.close
      end
    end

    def on_link_error(event)
      if !@delegate.nil?
        Qpid::Proton::Event.dispatch(@delegate, :on_link_error, event)
      else
        self.log_error(event.link, "link")
        event.conneciton.close
      end
    end

    def on_connection_closed(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_connection_closed, event) if !@delegate.nil?
    end

    def on_session_closed(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_session_closed, event) if !@delegate.nil?
    end

    def on_link_closed(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_link_closed, event) if !@delegate.nil?
    end

    def on_connection_closing(event)
      if !@delegate.nil?
        Qpid::Proton::Event.dispatch(@delegate, :on_connection_closing, event)
      elsif @peer_close_is_error
        self.on_connection_error(event)
      end
    end

    def on_session_closing(event)
      if !@delegate.nil?
        Qpid::Proton::Event.dispatch(@delegate, :on_session_closing, event)
      elsif @peer_close_is_error
        self.on_session_error(event)
      end
    end

    def on_link_closing(event)
      if !@delegate.nil?
        Qpid::Proton::Event.dispatch(@delegate, :on_link_closing, event)
      elsif @peer_close_is_error
        self.on_link_error(event)
      end
    end

    def on_transport_tail_closed(event)
      self.on_transport_closed(event)
    end

    def on_transport_closed(event)
      Qpid::Proton::Event.dispatch(@delegate, :on_disconnected, event) if !@delegate.nil?
    end

  end

end
