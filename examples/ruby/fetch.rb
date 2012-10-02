#!/usr/bin/env ruby
#
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

require 'cproton'
require 'optparse'

$options = {
  :hostname => "0.0.0.0",
  :port => "5672",
  :count => 1,
  :verbose => false
}
messages = []

OptionParser.new do |opts|
  opts.banner = "Usage: #{File.basename $0} [options] <mailbox>"

  opts.on("-s", "--server <address>", String, :REQUIRED,
          "Address of the server") do |server|
  end

  opts.on("-p", "--port <port>", Integer, :REQUIRED,
          "Port on the server") do |port|
    $options[:port] = port
  end

  opts.on("-c", "--count <#>", Integer, :REQUIRED,
          "Number of messages to read from the mailbox") do |count|
    $options[:count] = count
  end

  opts.on("-v", "--verbose", :NONE,
          "Turn on extra trace messages.") do |verbose|
    $options[:verbose] = true
  end

  begin
    ARGV << "-h" if ARGV.empty?
    opts.parse!
  rescue OptionParser::ParseError => error
    STDERR.puts error.message, "\n", opts
    exit 1
  end

  $options[:mailbox] = ARGV.first unless ARGV.empty?

  abort("No mailbox specified.") if $options[:mailbox].nil?

end

def log(text, return_code = 0)
  STDOUT.puts "#{Time.new}: #{text}" if $options[:verbose] || return_code.nonzero?

  # if we were given a non-zero code, then exit
  exit return_code unless return_code.zero?
end

class FetchClient
  attr_reader :sasl
  attr_reader :link
  attr_reader :conn

  def initialize(hostname, port, mailbox)
    @hostname = hostname
    @port = port
    @mailbox = mailbox
  end

  def setup
    # setup a driver connection to the server
    log "Connecting to server host = #{@hostname}:#{@port}"
    @driver = Cproton::pn_driver
    @cxtr = Cproton::pn_connector(@driver, @hostname, @port, nil)

    # configure SASL
    @sasl = Cproton::pn_connector_sasl @cxtr
    Cproton::pn_sasl_mechanisms @sasl, "ANONYMOUS"
    Cproton::pn_sasl_client @sasl

    # inform the engine about the connection, and link the driver to it.
    @conn = Cproton::pn_connection
    Cproton::pn_connector_set_connection @cxtr, @conn

    # create a session and link for receiving from the mailbox
    log "Fetching from the mailbox = #{@mailbox}"
    @ssn = Cproton::pn_session @conn
    @link = Cproton::pn_receiver @ssn, "receiver"
    Cproton::pn_link_set_source @link, @mailbox

    # now open all the engine endpoints
    Cproton::pn_connection_open @conn
    Cproton::pn_session_open @ssn
    Cproton::pn_link_open @link
  end

  def wait
    log "Waiting for events..."
    Cproton::pn_connector_process @cxtr
    Cproton::pn_driver_wait @driver, -1
    Cproton::pn_connector_process @cxtr
    log "...waiting done!"
  end

  def settle
    # locally settle any remotely settled deliveries
    d = Cproton::pn_unsettled_head @link
    while d && !Cproton::pn_delivery_readable(d)
      # delivery that has not yet been read
      _next = Cproton::pn_unsettled_next d
      Cproton::pn_delivery_settle(d) if Cproton::pn_remote_settled(d)
      d = _next
    end
  end

end

if __FILE__ == $PROGRAM_NAME
  receiver = FetchClient.new($options[:hostname],
                             $options[:port],
                             $options[:mailbox])

  receiver.setup

  # wait until we authenticate with the server
  while ![Cproton::PN_SASL_PASS, Cproton::PN_SASL_FAIL].include? Cproton::pn_sasl_state(receiver.sasl)
    receiver.wait
  end

  if Cproton::pn_sasl_state(receiver.sasl) == Cproton::PN_SASL_FAIL
    log "ERROR: Authentication failure", -1
  end

  # wait until the server has opened the connection
  while ((Cproton::pn_link_state(receiver.link) & Cproton::PN_REMOTE_ACTIVE) != Cproton::PN_REMOTE_ACTIVE)
    receiver.wait
  end

  # check if the server recognizes the mailbox, fail if it does not
  if Cproton::pn_link_remote_source(receiver.link) != $options[:mailbox]
    log "ERROR: mailbox #{$options[:mailbox]} does not exist!", -2
  end

  # Allow the server to send the expected number of messages to the receiver
  # by setting the credit to the expected count
  Cproton::pn_link_flow(receiver.link, $options[:count])

  # main loop: continue fetching messages until all the expected number of
  # messages have been retrieved

  while Cproton::pn_link_credit(receiver.link) > 0
    # wait for some messages to arrive
    receiver.wait if Cproton::pn_queued(receiver.link).zero?

    # read all queued deliveries
    while Cproton::pn_queued(receiver.link) > 0
      delivery = Cproton::pn_current(receiver.link)

      # read all bytes of message
      (rc, msg) = Cproton::pn_link_recv(receiver.link, Cproton::pn_pending(delivery))
      log "Received count/status=#{rc}"

      log("ERROR: Receive failed (#{rc}), exiting...", -3) if rc < 0

      puts "#{msg}"

      # let the server know we accept the message
      Cproton::pn_delivery_update(delivery, Cproton::PN_ACCEPTED)

      # go to the next deliverable
      Cproton::pn_link_advance(receiver.link)
    end

    receiver.settle
  end

  # block until any leftover deliveries are settled
  while Cproton::pn_link_unsettled(receiver.link) > 0
    receiver.wait
    receiver.settle
  end

  # we're done,c lose and wait for the remote to close also
  Cproton::pn_connection_close(receiver.conn)
  while ((Cproton::pn_connection_state(receiver.conn) & Cproton::PN_REMOTE_CLOSED) != Cproton::PN_REMOTE_CLOSED)
    receiver.wait
  end

end
