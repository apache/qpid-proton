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

# Thread safe message queue that notifies waiting senders when messages arrive.
class MessageQueue

  def initialize
    @lock = Mutex.new           # Make ations on the queue atomic
    @messages = []              # Messages on the queue
    @waiting = []               # Senders that are waiting for messages
  end

  # Push a message onto the queue and notify any waiting senders
  def push(message)
    @lock.synchronize do
      @messages << message
      unless @waiting.empty?    # Notify waiting senders
        # NOTE: the call to self.send_to is added to the sender's work_queue,
        # and will be executed in the sender's thread
        @waiting.each { |s| s.work_queue.add { self.send_to(s); } }
        @waiting.clear
      end
    end
  end

  # Pop a message off the queue.
  # If no messages available, record sender as waiting and return nil.
  def pop(sender)
    @lock.synchronize do
      if @messages.empty?
        @waiting << sender
        nil
      else
        @messages.shift
      end
    end
  end

  # NOTE: Called in sender's thread.
  # Pull messages from the queue as long as sender has credit.
  # If queue runs out of messages, record sender as waiting.
  def send_to(sender)
    while sender.credit > 0 && (message = pop(sender))
      sender.send(message)
    end
  end

  def forget(sender)
    @lock.synchronize { @waiting.delete(sender) }
  end
end

# Handler for broker connections. In a multi-threaded application you should
# normally create a separate handler instance for each connection.
class BrokerHandler < Qpid::Proton::MessagingHandler

  def initialize(broker)
    @broker = broker
  end

  def on_sender_open(sender)
    if sender.remote_source.dynamic?
      sender.source.address = SecureRandom.uuid
    elsif sender.remote_source.address
      sender.source.address = sender.remote_source.address
    else
      sender.connection.close("no source address")
      return
    end
    q = @broker.queue(sender.source.address)
    q.send_to(sender)
  end

  def on_receiver_open(receiver)
    if receiver.remote_target.address
      receiver.target.address = receiver.remote_target.address
    else
      receiver.connection.close("no target address")
    end
  end

  def on_sender_close(sender)
    q = @broker.queue(sender.source.address)
    q.forget(sender) if q
  end

  def on_connection_close(connection)
    connection.each_sender { |s| on_sender_close(s) }
  end

  def on_transport_close(transport)
    transport.connection.each_sender { |s| on_sender_close(s) }
  end

  def on_sendable(sender)
    @broker.queue(sender.source.address).send_to(sender)
  end

  def on_message(delivery, message)
    @broker.queue(delivery.receiver.target.address).push(message)
  end
end

# Broker manages the queues and accepts incoming connections.
class Broker < Qpid::Proton::Listener::Handler

  def initialize
    @queues = {}
    @connection_options = {}
    ssl_setup
  end

  def ssl_setup
    # Optional SSL setup
    ssl = Qpid::Proton::SSLDomain.new(Qpid::Proton::SSLDomain::MODE_SERVER)
    cert_passsword = "tserverpw"
    if Gem.win_platform?       # Use P12 certs for windows schannel
      ssl.credentials("ssl-certs/tserver-certificate.p12", "", cert_passsword)
    else
      ssl.credentials("ssl-certs/tserver-certificate.pem", "ssl-certs/tserver-private-key.pem", cert_passsword)
    end
    ssl.allow_unsecured_client # SSL is optional, this is not secure.
    @connection_options[:ssl_domain] = ssl if ssl
  rescue
    # Don't worry if we can't set up SSL.
  end

  def on_open(l)
    STDOUT.puts "Listening on #{l.port}\n"; STDOUT.flush
  end

  # Create a new BrokerHandler instance for each connection we accept
  def on_accept(l)
    { :handler => BrokerHandler.new(self) }.update(@connection_options)
  end

  def queue(address)
    @queues[address] ||= MessageQueue.new
  end

end

if ARGV.size != 1
  STDERR.puts "Usage: #{__FILE__} URL
Start an example broker listening on URL"
  return 1
end
url, = ARGV
container = Qpid::Proton::Container.new
container.listen(url, Broker.new)

# Run the container in multiple threads.
threads = 4.times.map { Thread.new {  container.run }}
threads.each { |t| t.join }
