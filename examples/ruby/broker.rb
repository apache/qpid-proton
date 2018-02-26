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

class Broker < Qpid::Proton::MessagingHandler

  def initialize(url)
    super()
    @url = url
    @queues = {}
    begin          # Optional SSL setup, ignore if we don't find cert files etc.
      @ssl_domain = Qpid::Proton::SSLDomain.new(Qpid::Proton::SSLDomain::MODE_SERVER)
      cert_passsword = "tserverpw"
      if Gem.win_platform?       # Use P12 certs for windows schannel
        @ssl_domain.credentials("ssl_certs/tserver-certificate.p12", "", cert_passsword)
      else
        @ssl_domain.credentials("ssl_certs/tserver-certificate.pem", "ssl_certs/tserver-private-key.pem", cert_passsword)
      end
      @ssl_domain.allow_unsecured_client # SSL is optional, this is not secure.
    rescue
      @ssl_domain = nil # Don't worry if we can't set up SSL.
    end
  end

  def on_container_start(container)
    # Options for incoming connections, provide SSL configuration if we have it.
    opts = {:ssl_domain => @ssl_domain} if @ssl_domain
    @listener = container.listen(@url, Qpid::Proton::Listener::Handler.new(opts))
    STDOUT.puts "Listening on #{@url.inspect}"; STDOUT.flush
  end

  def queue(address)
    unless @queues.has_key?(address)
      @queues[address] = MessageQueue.new
    end
    @queues[address]
  end

  def on_sender_open(sender)
    if sender.remote_source.dynamic?
      address = SecureRandom.uuid
      sender.source.address = address
      q = MessageQueue.new(true)
      @queues[address] = q
      q.subscribe(sender)
    elsif sender.remote_source.address
      sender.source.address = sender.remote_source.address
      self.queue(sender.source.address).subscribe(sender)
    end
  end

  def on_receiver_open(receiver)
    if receiver.remote_target.address
      receiver.target.address = receiver.remote_target.address
    end
  end

  def unsubscribe(link)
    if @queues.has_key?(link.source.address)
      if @queues[link.source.address].unsubscribe(link)
        @queues.delete(link.source.address)
      end
    end
  end

  def on_sender_close(sender)
    self.unsubscribe(sender)
  end

  def on_connection_close(connection)
    self.remove_stale_consumers(connection)
  end

  def on_transport_close(transport)
    self.remove_stale_consumers(transport.connection)
  end

  def remove_stale_consumers(connection)
    connection.each_sender { |s| unsubscribe(s) }
  end

  def on_sendable(sender)
    q = self.queue(sender.source.address)
    q.dispatch(sender)
  end

  def on_message(delivery, message)
    q = self.queue(delivery.link.target.address)
    q.publish(message)
  end

end

if ARGV.size != 1
  STDERR.puts "Usage: #{__FILE__} URL
Start an example broker listening on URL"
  return 1
end
url, = ARGV
Qpid::Proton::Container.new(Broker.new(url)).run
