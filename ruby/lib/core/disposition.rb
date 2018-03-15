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

  # @deprecated use {Delivery}
  class Disposition
    include Util::Deprecation

    # @private
    PROTON_METHOD_PREFIX = "pn_disposition"
    # @private
    include Util::Wrapper


    ACCEPTED = Cproton::PN_ACCEPTED
    REJECTED = Cproton::PN_REJECTED
    RELEASED = Cproton::PN_RELEASED
    MODIFIED = Cproton::PN_MODIFIED
    RECEIVED =  Cproton::PN_RECEIVED

    attr_reader :impl

    # @private
    def initialize(impl, local)
      deprecated self.class, Delivery
      @impl = impl
      @local = local
      @data = nil
      @condition = nil
      @annotations = nil
    end

    # @!attribute section_number
    #
    # @return [Integer] The section number of the disposition.
    #
    proton_set_get :section_number

    # @!attribute section_offset
    #
    #  @return [Integer] The section offset of the disposition.
    #
    proton_set_get :section_offset

    # @!attribute failed?
    #
    # @return [Boolean] The failed flag.
    #
    proton_set_is :failed

    # @!attribute undeliverable?
    #
    # @return [Boolean] The undeliverable flag.
    #
    proton_set_is :undeliverable

    # Sets the data for the disposition.
    #
    # @param data [Codec::Data] The data.
    #
    # @raise [AttributeError] If the disposition is remote.
    #
    def data=(data)
      raise AttributeError.new("data attribute is read-only") unless @local
      @data = data
    end

    # Returns the data for the disposition.
    #
    # @return [Codec::Data] The data.
    #
    def data
      if @local
        @data
      else
        Codec::Data.to_object(Cproton.pn_disposition_data(@impl))
      end
    end

    # Sets the annotations for the disposition.
    #
    # @param annotations [Codec::Data] The annotations.
    #
    # @raise [AttributeError] If the disposition is remote.
    #
    def annotations=(annotations)
      raise AttributeError.new("annotations attribute is read-only") unless @local
      @annotations = annotations
    end

    # Returns the annotations for the disposition.
    #
    # @return [Codec::Data] The annotations.
    #
    def annotations
      if @local
        @annotations
      else
        Codec::Data.to_object(Cproton.pn_disposition_annotations(@impl))
      end
    end

   # Sets the condition for the disposition.
    #
    # @param condition [Codec::Data] The condition.
    #
    # @raise [AttributeError] If the disposition is remote.
    #
    def condition=(condition)
      raise AttributeError.new("condition attribute is read-only") unless @local
      @condition = condition
    end

    # Returns the condition of the disposition.
    #
    # @return [Codec::Data] The condition of the disposition.
    #
    def condition
      if @local
        @condition
      else
        Condition.convert(Cproton.pn_disposition_condition(@impl))
      end
    end

  end

end
