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

FAILED         = 0
CONNECTION_UP  = 1
AUTHENTICATING = 2

$options  = {
  :verbose => false,
  :hostname => "0.0.0.0",
  :port => "5672"
}

OptionParser.new do |opts|
  opts.banner = "Usage: mailserver [options] <server-address> [<server-port>]"

  opts.on("-v", "--verbose", :NONE,
          "Print status messages to stdout") do |f|
    $options[:verbose] = true
  end

  opts.parse!

  $options[:hostname] = ARGV[0] if ARGV.length > 0
  $options[:port] = ARGV[1] if ARGV.length > 1
end

def log(text)
  STDOUT.puts "#{Time.new}: #{text}" if $options[:verbose]
end

class MailServer
  def initialize(hostname, port, verbose)
    @hostname = hostname
    @port = port
    @verbose = verbose
    @counter = 0
    @mailboxes = {}

    @driver = nil
  end

  def setup
    @driver = Cproton::pn_driver
    @listener = Cproton::pn_listener @driver, @hostname, @port, nil
    raise "Error: could not listen on #{@hostname}:#{@port}" if @listener.nil?
  end

  def wait
    log "Driver sleep."
    Cproton::pn_driver_wait @driver, -1
    log "Driver wakeup."
  end

  def accept_connection_requests
    l = Cproton::pn_driver_listener @driver

    while !l.nil?
      log "Accepting connection."
      cxtr = Cproton::pn_listener_accept l
      Cproton::pn_connector_set_context cxtr, AUTHENTICATING
      l = Cproton::pn_driver_listener @driver
    end
  end

  def process_connections
    cxtr = Cproton::pn_driver_connector @driver

    while cxtr
      log "Process connector"

      Cproton::pn_connector_process cxtr

      state = Cproton::pn_connector_context cxtr
      case state
      when AUTHENTICATING
        log "Authenticating..."
        self.authenticate_connector cxtr

      when CONNECTION_UP
        log "Connection established..."
        self.service_connector cxtr

      else
        raise "Unknown connection state #{state}"
      end

      Cproton::pn_connector_process cxtr

      if Cproton::pn_connector_closed cxtr
        log "Closing connector."
        Cproton::pn_connector_free cxtr
      end

      cxtr = Cproton::pn_driver_connector @driver

    end
  end

  def authenticate_connector(cxtr)
    log "Authenticating..."

    sasl = Cproton::pn_connector_sasl cxtr
    state = Cproton::pn_sasl_state sasl
    while [Cproton::PN_SASL_CONF, Cproton::PN_SASL_STEP].include? state

      case state
      when Cproton::PN_SASL_CONF
        log "Authenticating-CONF..."
        Cproton::pn_sasl_mechanisms sasl, "ANONYMOUS"
        Cproton::pn_sasl_server sasl

      when Cproton::PN_SASL_STEP
        log "Authenticating-STEP..."
        mech = Cproton::pn_sasl_remote_mechanisms sasl
        if mech == "ANONYMOUS"
          Cproton::pn_sasl_done sasl, Cproton::PN_SASL_OK
        else
          Cproton::pn_sasl_done sasl, Cproton::PN_SASL_AUTH
        end
      end

      state = Cproton::pn_sasl_state sasl

    end

    case state
    when Cproton::PN_SASL_PASS
      Cproton::pn_connector_set_connection cxtr, Cproton::pn_connection
      Cproton::pn_connector_set_context cxtr, CONNECTION_UP
      log "Authentication-PASSED"

    when Cproton::PN_SASL_FAIL
      Cproton::pn_connector_set_context cxtr, FAILED
      log "Authentication-FAILED"

    else
      log "Authentication-PENDING"
    end

  end

  def service_connector(cxtr)
    log "I/O processing starting."

    conn = Cproton::pn_connector_connection cxtr
    if ((Cproton::pn_connection_state(conn) & Cproton::PN_LOCAL_UNINIT) == Cproton::PN_LOCAL_UNINIT)
      log "Connection opened."
      Cproton::pn_connection_open(conn)
    end

    ssn = Cproton::pn_session_head conn, Cproton::PN_LOCAL_UNINIT
    while ssn
      Cproton::pn_session_open ssn
      log "Session opened."
      ssn = Cproton::pn_session_next ssn, Cproton::PN_LOCAL_UNINIT
    end

    link = Cproton::pn_link_head conn, Cproton::PN_LOCAL_UNINIT
    while link
      setup_link link
      link = Cproton::pn_link_next link, Cproton::PN_LOCAL_UNINIT
    end

    delivery = Cproton::pn_work_head conn
    while delivery
      log "Process delivery #{Cproton::pn_delivery_tag delivery}"
      if Cproton::pn_delivery_readable delivery
        process_receive delivery
      elsif Cproton::pn_delivery_writable delivery
        send_message delivery
      end

      if Cproton::pn_delivery_updated delivery
        log "Remote disposition for #{Cproton::pn_delivery_tag delivery}: #{Cproton::pn_delivery_remote_state delivery}"
        Cproton::pn_delivery_settle(delivery) if Cproton::pn_delivery_remote_state delivery
      end

      delivery = Cproton::pn_work_next delivery
    end

    link = Cproton::pn_link_head conn, Cproton::PN_LOCAL_ACTIVE | Cproton::PN_REMOTE_CLOSED
    while link
      Cproton::pn_link_close link
      log "Link closed."
      link = Cproton::pn_link_next link, Cproton::PN_LOCAL_ACTIVE | Cproton::PN_REMOTE_CLOSED
    end

    ssn = Cproton::pn_session_head conn, Cproton::PN_LOCAL_ACTIVE | Cproton::PN_REMOTE_CLOSED
    while ssn
      Cproton::pn_session_close ssn
      log "Session closed."
      ssn = Cproton::pn_session_next ssn, Cproton::PN_LOCAL_ACTIVE | Cproton::PN_REMOTE_CLOSED
    end

    if Cproton::pn_connection_state(conn) == (Cproton::PN_LOCAL_ACTIVE | Cproton::PN_REMOTE_CLOSED)
      log "Connection closed."
      Cproton::pn_connection_close conn
    end

  end

  def process_receive(delivery)
    link = Cproton::pn_delivery_link delivery
    mbox = Cproton::pn_link_remote_target link
    (rc, msg) = Cproton::pn_link_recv link, 1024

    log "Message received #{rc}"

    while rc >= 0
      unless @mailboxes.include? mbox
        log "Error: cannot send to mailbox #{mbox} - dropping message."
      else
        @mailboxes[mbox] << msg
        log "Mailbox #{mbox} contains: #{@mailboxes[mbox].size}"
      end

      (rc, msg) = Cproton::pn_link_recv link, 1024
    end

    log "Messages accepted."

    Cproton::pn_delivery_update delivery, Cproton::PN_ACCEPTED
    Cproton::pn_delivery_settle delivery
    Cproton::pn_link_advance link

    Cproton::pn_link_flow(link, 1) if Cproton::pn_link_credit(link).zero?
  end

  def send_message(delivery)
    link = Cproton::pn_delivery_link delivery
    mbox = Cproton::pn_link_remote_source link
    log "Request for Mailbox=#{mbox}"

    if @mailboxes.include?(mbox) && !@mailboxes[mbox].empty?
      msg = @mailboxes[mbox].first
      @mailboxes[mbox].delete_at 0
      log "Fetching message #{msg}"
    else
      log "Warning: mailbox #{mbox} is empty, sending empty message"
      msg = ""
    end

    sent = Cproton::pn_link_send link, msg
    log "Message sent: #{sent}"

    if Cproton::pn_link_advance link
      Cproton::pn_delivery link, "server-delivery-#{@counter}"
      @counter += 1
    end
  end

  def setup_link(link)
    r_tgt = Cproton::pn_link_remote_target link
    r_src = Cproton::pn_link_remote_source link

    if Cproton::pn_link_is_sender link
      log "Opening link to read from mailbox: #{r_src}"

      unless @mailboxes.include? r_src
        log "Error: mailbox #{r_src} does not exist!"
        r_src = nil
      end
    else
      log "Opening link to write to mailbox: #{r_tgt}"

      @mailboxes[r_tgt] = [] unless @mailboxes.include? r_tgt
    end

    Cproton::pn_link_set_target link, r_tgt
    Cproton::pn_link_set_source link, r_src

    if Cproton::pn_link_is_sender link
      Cproton::pn_delivery link, "server-delivery-#{@counter}"
      @counter += 1
    else
      Cproton::pn_link_flow link, 1
    end

    Cproton::pn_link_open link

  end

end

#------------------
# Begin entry point
#------------------

if __FILE__ == $PROGRAM_NAME
  server = MailServer.new($options[:hostname],
                          $options[:port],
                          $options[:verbose])

  server.setup
  loop do
    server.wait
    server.accept_connection_requests
    server.process_connections
  end
end

