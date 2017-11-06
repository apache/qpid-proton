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

class ExampleSend < Qpid::Proton::Handler::MessagingHandler

  attr_reader :url

  def initialize(url, expected)
    super()
    @url = url
    @sent = 0
    @confirmed = 0
    @expected = expected
  end

  def on_sendable(event)
    while event.sender.credit > 0 && @sent < @expected
      msg = Qpid::Proton::Message.new
      msg.body = "sequence #{@sent}"
      msg.id = @sent
      event.sender.send(msg)
      @sent = @sent + 1
    end
  end

  def on_accepted(event)
    @confirmed = @confirmed + 1
    if self.finished?
      puts "#{@expected > 1 ? 'All ' : ''}#{@expected} message#{@expected > 1 ? 's' : ''} confirmed!"
      event.connection.close
    end
  end

  def on_disconnected(event)
    @sent = @confirmed
  end

  def finished?
    @confirmed == @expected
  end

end

class ExampleReceive < Qpid::Proton::Handler::MessagingHandler

  attr_reader :url

  def initialize(url, expected)
    super()
    @url = url
    @expected = expected
    @received = 0
  end

  def on_message(event)
    if event.message.id.nil? || event.message.id < @received
      puts "Missing or old message id: id=#{event.message.id}"
      return
    end
    if @expected.zero? || (@received < @expected)
      puts "Received: #{event.message.body}"
      @received = @received + 1
      if finished?
        event.receiver.close
        event.connection.close
      end
    end
  end

  def finished?
    @received == @expected
  end

end
