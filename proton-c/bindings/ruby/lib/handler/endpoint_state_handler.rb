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

# @private
module Qpid::Proton::Handler

  # Mixin to convert raw proton endpoint events to {#MessagingHandler} events
  #
  # A XXX_opening method will be called when the remote peer has requested
  # an open that was not initiated locally. By default this will simply open
  # locally, which then trigtgers the XXX_opened called.
  #
  # A XXX_opened method will be called when both local and remote peers have
  # opened the link, session or connection. This can be used to confirm a
  # locally initiated action for example.
  #
  # The same applies to close.
  #
  module EndpointStateHandler

    def on_link_remote_close(event)
      super
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
      super
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
      super
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
      super
      self.on_connection_opened(event) if event.connection.remote_active?
    end

    def on_connection_remote_open(event)
      super
      if event.connection.local_active?
        self.on_connection_opened(event)
      elsif event.connection.local_uninit?
        self.on_connection_opening(event)
        event.connection.open unless event.connection.local_active?
      end
    end

    def on_session_local_open(event)
      super
      self.on_session_opened(event) if event.session.remote_active?
    end

    def on_session_remote_open(event)
      super
      if !(event.session.state & Qpid::Proton::Endpoint::LOCAL_ACTIVE).zero?
        self.on_session_opened(event)
      elsif event.session.local_uninit?
        self.on_session_opening(event)
        event.session.open
      end
    end

    def on_link_local_open(event)
      super
      self.on_link_opened(event) if event.link.remote_active?
    end

    def on_link_remote_open(event)
      super
      if event.link.local_active?
        self.on_link_opened(event)
      elsif event.link.local_uninit?
        self.on_link_opening(event)
        event.link.open
      end
    end
  end
end
