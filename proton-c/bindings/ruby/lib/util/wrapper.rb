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

module Qpid::Proton::Util

  # @private
  module Wrapper

    # @private
    def impl=(impl)
      @impl = impl
    end

    # @private
    def impl
      @impl
    end

    def self.registry
      @registry ||= {}
    end

    def self.included(base)
      base.extend(ClassMethods)
    end

    # Adds methods to the target class for storing and retrieving pure Ruby
    # wrappers to underlying Proton structures.
    #
    # Such wrappers are stored in a registry using a key. The key is then
    # attached to the Proton structure as a record. That record lives for as
    # long as the Proton structure lives, and when the structure is released
    # the record acts as hook to also delete the Ruby wrapper object from the
    # registry.
    #
    # @private
    #
    module ClassMethods

      # @private
      def get_key(impl)
        ("%032x" % Cproton.pni_address_of(impl))
      end

      # Stores the given object for later retrieval.
      #
      # @param object [Object] The object.
      # @param attachment_method [Symbol] The Proton attachment method.
      #
      def store_instance(object, attachment_method = nil)
        # ensure the impl has a reference to the wrapper object
        object.impl.instance_eval { @proton_wrapper = object }
        registry_key = get_key(object.impl)
        unless attachment_method.nil?
          record = Cproton.__send__(attachment_method, object.impl)
          rbkey = Cproton.Pn_rbkey_new
          Cproton.Pn_rbkey_set_registry(rbkey, Cproton.pn_rb2void(Qpid::Proton::Util::Wrapper.registry))
          Cproton.Pn_rbkey_set_method(rbkey, "delete")
          Cproton.Pn_rbkey_set_key_value(rbkey, registry_key)
          Cproton.pn_record_def(record, RBCTX, Cproton.Pn_rbkey__class());
          Cproton.pn_record_set(record, RBCTX, rbkey)
        end
        Qpid::Proton::Util::Wrapper.registry[registry_key] = object
      end

      # Retrieves the wrapper object with the supplied Proton struct.
      #
      # @param impl [Object] The wrapper for the Proton struct.
      # @param attachment_method [Symbol] The Proton attachment method.
      #
      # @return [Object] The Ruby wrapper object.
      #
      def fetch_instance(impl, attachment_method = nil)
        # if the impl has a wrapper already attached, then return it
        if impl.instance_variable_defined?(:@proton_wrapper)
          return impl.instance_variable_get(:@proton_wrapper)
        end
        unless attachment_method.nil?
          record = Cproton.__send__(attachment_method, impl)
          rbkey = Cproton.pni_void2rbkey(Cproton.pn_record_get(record, RBCTX))
          # if we don't have a key, then we don't have an object
          return nil if rbkey.nil?
          registry_key = Cproton.Pn_rbkey_get_key_value(rbkey)
        else
          registry_key = get_key(impl)
        end
        # if the object's not in the registry then return
        return nil unless Qpid::Proton::Util::Wrapper.registry.has_key?(registry_key)

        result = Qpid::Proton::Util::Wrapper.registry[registry_key]
        # result = nil unless result.weakref_alive?
        if result.nil?
          raise Qpid::Proton::ProtonError.new("missing object for key=#{registry_key}")
        else
          # update the impl since the Swig wrapper for it may have changed
          result.impl = impl
        end
        return result
      end

    end

  end

  # @private
  RBCTX = Wrapper.hash.to_i

end
