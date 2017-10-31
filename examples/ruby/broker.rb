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

require 'qpid_proton'
require 'optparse'
require 'pathname'

class MessageQueue

  def initialize(dynamic = false)
    @dynamic = dynamic
    @queue = Queue.new
    @consumers = []
  end

  def subscribe(consumer)
    @consumers << (consumer)
  end

  def unsubscribe(consumer)
    if @consumers.include?(consumer)
      @consumers.delete(consumer)
    end
    @consumers.empty? && (@dynamic || @queue.empty?)
  end

  def publish(message)
    @queue << message
    self.dispatch
  end

  def dispatch(consumer = nil)
    if consumer
      c = [consumer]
    else
      c = @consumers
    end

    while self.deliver_to(c) do
    end
  end

  def deliver_to(consumers)
    result = false
    consumers.each do |consumer|
      if consumer.credit > 0 && !@queue.empty?
        consumer.send(@queue.pop(true))
        result = true
      end
    end
    return result
  end

end

class Broker < Qpid::Proton::Handler::MessagingHandler

  def initialize(url)
    super()
    @url = url
    @queues = {}
  end

  def on_start(event)
    @acceptor = event.container.listen(@url)
    print "Listening on #{@url}\n"
  end

  def queue(address)
    unless @queues.has_key?(address)
      @queues[address] = MessageQueue.new
    end
    @queues[address]
  end

  def on_link_opening(event)
    if event.link.sender?
      if event.link.remote_source.dynamic?
        address = SecureRandom.uuid
        event.link.source.address = address
        q = MessageQueue.new(true)
        @queues[address] = q
        q.subscribe(event.link)
      elsif event.link.remote_source.address
        event.link.source.address = event.link.remote_source.address
        self.queue(event.link.source.address).subscribe(event.link)
      end
    elsif event.link.remote_target.address
      event.link.target.address = event.link.remote_target.address
    end
  end

  def unsubscribe(link)
    if @queues.has_key?(link.source.address)
      if @queues[link.source.address].unsubscribe(link)
        @queues.delete(link.source.address)
      end
    end
  end

  def on_link_closing(event)
    self.unsubscribe(event.link) if event.link.sender?
  end

  def on_connection_closing(event)
    self.remove_stale_consumers(event.connection)
  end

  def on_disconnected(event)
    self.remove_stale_consumers(event.connection)
  end

  def remove_stale_consumers(connection)
    l = connection.link_head(Qpid::Proton::Endpoint::REMOTE_ACTIVE)
    while !l.nil?
      self.unsubscribe(l) if l.sender?
      l = l.next(Qpid::Proton::Endpoint::REMOTE_ACTIVE)
    end
  end

  def on_sendable(event)
    q = self.queue(event.link.source.address)
    q.dispatch(event.link)
  end

  def on_message(event)
    q = self.queue(event.link.target.address)
    q.publish(event.message)
  end

end

if ARGV.size != 1
  STDERR.puts "Usage: #{__FILE__} URL
Start an example broker listening on URL"
  return 1
end
url, = ARGV
Qpid::Proton::Container.new(Broker.new(url)).run
