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

def debug(text)
  print "[#{Time.now.strftime('%s')}] #{text}\n" if $options[:debug]
end

class Exchange

  def initialize(dynamic = false)
    @dynamic = dynamic
    @queue = Queue.new
    @consumers = []
  end

  def subscribe(consumer)
    debug("subscribing #{consumer}")
    @consumers << (consumer)
    debug(" there are #{@consumers.size} consumers")
  end

  def unsubscribe(consumer)
    debug("unsubscribing #{consumer}")
    if @consumers.include?(consumer)
      @consumers.delete(consumer)
    else
      debug(" consumer doesn't exist")
    end
    debug("  there are #{@consumers.size} consumers")
    @consumers.empty? && (@dynamic || @queue.empty?)
  end

  def publish(message)
    debug("queueing message: #{message.body}")
    @queue << message
    self.dispatch
  end

  def dispatch(consumer = nil)
    debug("dispatching: consumer=#{consumer}")
    if consumer
      c = [consumer]
    else
      c = @consumers
    end

    while self.deliver_to(c) do
    end
  end

  def deliver_to(consumers)
    debug("delivering to #{consumers.size} consumer(s)")
    result = false
    consumers.each do |consumer|
      debug(" current consumer=#{consumer} credit=#{consumer.credit}")
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
    debug("on_start event")
    @acceptor = event.container.listen(@url)
    print "Listening on #{@url}\n"
  end

  def queue(address)
    debug("fetching queue for #{address}: (there are #{@queues.size} queues)")
    unless @queues.has_key?(address)
      debug(" creating new queue")
      @queues[address] = Exchange.new
    else
      debug(" using existing queue")
    end
    result = @queues[address]
    debug(" returning #{result}")
    return result
  end

  def on_link_opening(event)
    debug("processing on_link_opening")
    debug("link is#{event.link.sender? ? '' : ' not'} a sender")
    if event.link.sender?
      if event.link.remote_source.dynamic?
        address = SecureRandom.uuid
        event.link.source.address = address
        q = Exchange.new(true)
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
    debug("unsubscribing #{link.source.address}")
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
    debug("on_sendable event")
    q = self.queue(event.link.source.address)
    debug(" dispatching #{event.message} to #{q}")
    q.dispatch(event.link)
  end

  def on_message(event)
    debug("on_message event")
    q = self.queue(event.link.target.address)
    debug(" dispatching #{event.message} to #{q}")
    q.publish(event.message)
  end

end

$options = {
  :address => "localhost:5672",
  :debug => false
}

OptionParser.new do |opts|
  opts.banner = "Usage: #{Pathname.new(__FILE__).basename} [$options]"

  opts.on("-a", "--address=ADDRESS", "Send messages to ADDRESS (def. #{$options[:address]}).") do |address|
    $options[:address] = address
  end

  opts.on("-d", "--debug", "Enable debugging output (def. #{$options[:debug]})") do
    $options[:debug] = true
  end

end.parse!

begin
  Qpid::Proton::Reactor::Container.new(Broker.new($options[:address])).run
rescue Interrupt
end
