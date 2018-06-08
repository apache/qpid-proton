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

  # Status of a message transfer on a {Link}
  # Common base class for {Tracker} and {Delivery}.
  class Transfer
    include Util::Deprecation

    # @!private
    PROTON_METHOD_PREFIX = "pn_delivery"
    # @!private
    include Util::Wrapper

    def self.wrap(impl)
      return unless impl
      self.fetch_instance(impl, :pn_delivery_attachments) ||
        (Cproton.pn_link_is_sender(Cproton.pn_delivery_link(impl)) ? Tracker : Delivery).new(impl)
    end

    def initialize(impl)
      @impl = impl
      @inspect = nil
      self.class.store_instance(self, :pn_delivery_attachments)
    end

    public

    State = Disposition::State
    include State

    # @return [String] Unique ID for the transfer in the context of the {#link}
    def id() Cproton.pn_delivery_tag(@impl); end

    # @deprecated use {#id}
    deprecated_alias :tag, :id

    # @return [Boolean] True if the transfer is remotely settled.
    proton_caller :settled?

    # @return [Integer] Remote state of the transfer, one of the values in {State}
    def state() Cproton.pn_delivery_remote_state(@impl); end

    # @return [Link] The parent link.
    def link() Link.wrap(Cproton.pn_delivery_link(@impl)); end

    # @return [Session] The parent session.
    def session() link.session; end

    # @return [Connection] The parent connection.
    def connection() self.session.connection; end

    # @return [Transport] The parent connection's transport.
    def transport() self.connection.transport; end

    # @return [WorkQueue] The parent connection's work-queue.
    def work_queue() self.connection.work_queue; end

    # @deprecated internal use only
    proton_caller :writable?
    # @deprecated internal use only
    proton_caller :readable?
    # @deprecated internal use only
    proton_caller :updated?
    # @deprecated internal use only
    proton_caller :clear
    # @deprecated internal use only
    proton_caller :pending
    # @deprecated internal use only
    proton_caller :partial?
    # @deprecated internal use only
    def update(state) Cproton.pn_delivery_update(@impl, state); end
    # @deprecated internal use only
    proton_caller :buffered?
    # @deprecated internal use only
    def local_state() Cproton.pn_delivery_local_state(@impl); end
    # @deprecated use {#state}
    deprecated_alias :remote_state, :state
    # @deprecated internal use only
    def settle(state = nil)
      update(state) unless state.nil?
      Cproton.pn_delivery_settle(@impl)
      @inspect = inspect # Save the inspect string, the delivery pointer will go bad.
    end

    def inspect() @inspect || super; end
    def to_s() inspect; end
  end
end
