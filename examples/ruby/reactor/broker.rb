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

require_relative '../lib/debugging'

class Exchange

  include Debugging

  def initialize(dynamic = false)
    @dynamic = dynamic
    @queue = Queue.new
    @consumers = []
  end

  def subscribe(consumer)
    debug("subscribing #{consumer}") if $options[:debug]
    @consumers << (consumer)
    debug(" there are #{@consumers.size} consumers") if $options[:debug]
  end

  def unsubscribe(consumer)
    debug("unsubscribing #{consumer}") if $options[:debug]
    if @consumers.include?(consumer)
      @consumers.delete(consumer)
    else
      debug(" consumer doesn't exist") if $options[:debug]
    end
    debug("  there are #{@consumers.size} consumers") if $options[:debug]
    @consumers.empty? && (@dynamic || @queue.empty?)
  end

  def publish(message)
    debug("queueing message: #{message.body}") if $options[:debug]
    @queue << message
    self.dispatch
  end

  def dispatch(consumer = nil)
    debug("dispatching: consumer=#{consumer}") if $options[:debug]
    if consumer
      c = [consumer]
    else
      c = @consumers
    end

    while self.deliver_to(c) do
    end
  end

  def deliver_to(consumers)
    debug("delivering to #{consumers.size} consumer(s)") if $options[:debug]
    result = false
    consumers.each do |consumer|
      debug(" current consumer=#{consumer} credit=#{consumer.credit}") if $options[:debug]
      if consumer.credit > 0 && !@queue.empty?
        consumer.send(@queue.pop(true))
        result = true
      end
    end
    return result
  end

end

class Broker < Qpid::Proton::Handler::MessagingHandler

  include Debugging

  def initialize(url)
    super()
    @url = url
    @queues = {}
  end

  def on_start(event)
    debug("on_start event") if $options[:debug]
    @acceptor = event.container.listen(@url)
    print "Listening on #{@url}\n"
  end

  def queue(address)
    debug("fetching queue for #{address}: (there are #{@queues.size} queues)") if $options[:debug]
    unless @queues.has_key?(address)
      debug(" creating new queue") if $options[:debug]
      @queues[address] = Exchange.new
    else
      debug(" using existing queue") if $options[:debug]
    end
    result = @queues[address]
    debug(" returning #{result}") if $options[:debug]
    return result
  end

  def on_link_opening(event)
    debug("processing on_link_opening") if $options[:debug]
    debug("link is#{event.link.sender? ? '' : ' not'} a sender") if $options[:debug]
    if event.link.sender?
      if event.link.remote_source.dynamic?
        address = generate_uuid
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
    debug("unsubscribing #{link.address}") if $options[:debug]
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
    debug("on_sendable event") if $options[:debug]
    q = self.queue(event.link.source.address)
    debug(" dispatching #{event.message} to #{q}") if $options[:debug]
    q.dispatch(event.link)
  end

  def on_message(event)
    debug("on_message event") if $options[:debug]
    q = self.queue(event.link.target.address)
    debug(" dispatching #{event.message} to #{q}") if $options[:debug]
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
