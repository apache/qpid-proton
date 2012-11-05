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
#

require 'cproton'
require 'optparse'


$options  = {
  :verbose => false,
  :hostname => "0.0.0.0",
  :port => "5672"
}


OptionParser.new do |opts|
  opts.banner = "Usage: mailserver [options] <server-address> [<server-port>] <message-string> [<message-string> ...]"

  opts.on("-s", "--server", String, :REQUIRED,
          "The server hostname (def. #{$options[:hostname]})") do |hostname|
    $options[:hostname] = hostname
  end

  opts.on("-p", "--port", String, :REQUIRED,
          "The server port (def. #{$options[:port]})") do |port|
    $options[:post] = port
  end

  opts.on("-m", "--mailbox", String, :REQUIRED,
          "Name of the mailbox on the server") do |mailbox|
    $options[:mailbox] = mailbox
  end

  opts.on("-v", "--verbose", :NONE,
             "Enable verbose output (def. #{$options[:verbose]})") do
    $options[:verbose] = true
  end

  begin
    ARGV << "-h" if ARGV.empty?
    opts.parse!(ARGV)
  rescue OptionParser::ParseError => error
    STDERR.puts error.message, "\n", opts
    exit 1
  end

  $options[:messages] = ARGV

  abort "No mailbox specified." if $options[:mailbox].nil?
  abort "No messages specified." if $options[:messages].empty?

end

def log(text)
  printf "#{Time.new}: #{text}\n" if $options[:verbose]
end


class PostClient

  attr_reader :sasl
  attr_reader :link
  attr_reader :conn

  def initialize(hostname, port, mailbox, verbose)
    @hostname = hostname
    @port = port
    @mailbox = mailbox
    @verbose = verbose
  end

  def setup
    log "Connection to server host = #{@hostname}:#{@port}"
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
    log "Posting to mailbox = #{@mailbox}"
    @ssn = Cproton::pn_session @conn
    @link = Cproton::pn_sender @ssn, "sender"
    Cproton::pn_link_set_target @link, @mailbox

    # now open all the engien end points
    Cproton::pn_connection_open @conn
    Cproton::pn_session_open @ssn
    Cproton::pn_link_open @link
  end

  def wait
    log "Waiting for events..."
    Cproton::pn_connector_process @cxtr
    Cproton::pn_driver_wait @driver, -1
    Cproton::pn_connector_process @cxtr
    log "..waiting done!"
  end

  def settle
    d = Cproton::pn_unsettled_head @link
    while d
      _next = Cproton::pn_unsettled_next d
      disp = Cproton::pn_delivery_remote_state d

      if disp.nonzero? && disp != Cproton::PN_ACCEPTED
        log "Warning: message was not accepted by the remote!"
      end

      if disp || Cproton::pn_remote_settled(disp)
        Cproton::pn_delivery_settle(d)
      end

      d = _next
    end
  end

end


if __FILE__ == $PROGRAM_NAME
  sender = PostClient.new($options[:hostname],
                          $options[:port],
                          $options[:mailbox],
                          $options[:verbose])
  sender.setup

  sender.wait while ![Cproton::PN_SASL_PASS, Cproton::PN_SASL_FAIL].include? Cproton::pn_sasl_state(sender.sasl)

  abort "Error: Authentication failure" if Cproton::pn_sasl_state(sender.sasl) == Cproton::PN_SASL_FAIL

  while !$options[:messages].empty? do

    while Cproton::pn_link_credit(sender.link).zero?
      log "Waiting for credit"
      sender.wait
    end

    while Cproton::pn_link_credit(sender.link) > 0
      msg = $options[:messages].first
      $options[:messages].delete_at 0
      log "Sending #{msg}"
      d = Cproton::pn_delivery sender.link, "post-deliver-#{$options[:messages].length}"
      rc = Cproton::pn_link_send(sender.link, msg)

      abort "Error: sending message: #{msg}" if rc < 0

      fail unless rc == msg.length

      Cproton::pn_link_advance sender.link
    end

    sender.settle
  end

  while Cproton::pn_link_unsettled(sender.link) > 0
    log "Settling things with the server..."
    sender.wait
    sender.settle
    log "Finished settling."
  end

  Cproton::pn_connection_close sender.conn
  while ((Cproton::pn_connection_state(sender.conn) & Cproton::PN_REMOTE_CLOSED) != Cproton::PN_REMOTE_CLOSED)
    log "Waiting for things to become closed..."
    sender.wait
  end

end
