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
  module Util

    # @private
    # Class methods to help wrapper classes define forwarding methods to proton-C functions
    #
    # The extending class must define PROTON_METHOD_PREFIX, the functions here
    # make it easy to define ruby methods to forward calls to C functions.
    #
    module SWIGClassHelper
      # Define ruby method +name+ to forward arguments to
      # CProton.PROTON_METHOD_PREFIX_+pn_name+(@impl, ...)
      def proton_forward(name, pn_name)
        pn_name = pn_name[0..-2] if pn_name.to_s.end_with? "?" # Drop trailing ? for ruby bool methods
        pn_name = "#{self::PROTON_METHOD_PREFIX}_#{pn_name}".to_sym
        define_method(name.to_sym) { |*args| Cproton.__send__(pn_name, @impl, *args) }
      end

      def proton_caller(*names) names.each { |name| proton_forward(name, name) }; end
      def proton_set(*names) names.each { |name| proton_forward("#{name}=", "set_#{name}") }; end
      def proton_get(*names) names.each { |name| proton_forward(name, "get_#{name}") }; end
      def proton_is(*names) names.each { |name| proton_forward("#{name}?", "is_#{name}") }; end
      def proton_set_get(*names) names.each { |name| proton_get(name); proton_set(name) }; end
      def proton_set_is(*names) names.each { |name| proton_is(name); proton_set(name) }; end

      # Store ruby wrappers as attachments so they can be retrieved from the C pointer.
      #
      # Wrappers are stored in a registry using a key. The key is then attached to
      # the Proton structure as a record. That record lives for as long as the
      # Proton structure lives, and when the structure is released the record acts
      # as hook to also delete the Ruby wrapper object from the registry.

      @@registry = {}

      # @private
      def get_key(impl)
        ("%032x" % Cproton.pni_address_of(impl))
      end

      # @private
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
        @@registry[registry_key] = object
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
        return nil unless @@registry.has_key?(registry_key)

        result = @@registry[registry_key]
        # result = nil unless result.weakref_alive?
        if result.nil?
          raise Qpid::Proton::ProtonError.new("missing object for key=#{registry_key}")
        else
          # update the impl since the Swig wrapper for it may have changed
          result.impl = impl
        end
        return result
      end
      RBCTX = self.hash.to_i
    end

    # @private
    #
    # Instance methods to include in classes that wrap pn_object types
    # that support pn_inspect etc. Automatically extends SWIGClassHelper
    #
    module Wrapper

      def self.included(base)
        base.extend(SWIGClassHelper)
      end

      attr_accessor :impl

      def inspect
        return "#{self.class}<nil>" unless @impl
        pstr = Cproton.pn_string("")
        begin
          Cproton.pn_inspect(@impl, pstr)
          return Cproton.pn_string_get(pstr)
        ensure
          Cproton.pn_free(pstr)
        end
      end

      def to_s() inspect; end

      def self.registry
        @registry ||= {}
      end
    end
  end
end
