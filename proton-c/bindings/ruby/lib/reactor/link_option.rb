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

  class LinkOption
    def apply(link)
    end

    # Subclasses should override this to selectively apply an option.
    def test(link)
      true
    end
  end

  class AtMostOne < LinkOption
    def apply(link)
      link.snd_settle_mod = Link::SND_SETTLED
    end
  end

  class AtLeastOnce < LinkOption
    def apply(link)
      link.snd_settle_mode = Link::SND_UNSETTLED
      link.rcv_settle_mode = Link::RCV_FIRST
    end
  end

  class SenderOption < LinkOption
    def test(link)
      link.sender?
    end
  end

  class ReceiverOption < LinkOption
    def test(link)
      link.receiver?
    end
  end

  class DynamicNodeProperties < LinkOption
    def initialize(properties = {})
      @properties = []
      properties.each do |property|
        @properties << property.to_sym
      end
    end

    def apply(link)
      if link.receiver?
        link.source.properties.dict = @properties
      else
        link.target.properties.dict = @properties
      end
    end
  end

  class Filter < ReceiverOption
    def initialize(filter_set = {})
      @filter_set = filter_set
    end

    def apply(receiver)
      receiver.source.filter.dict = @filter_set
    end
  end

  #class Selector < Filter
  #  def initialize(value, name = 'selector')
  #
  #  end
  #end

end
