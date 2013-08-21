#!/usr/bin/env ruby

require 'qpid_proton.rb'

messenger = Qpid::Proton::Messenger.new()
messenger.incoming_window = 1
message = Qpid::Proton::Message.new()

address = ARGV[0]
if not address then
  address = "~0.0.0.0"
end
messenger.subscribe(address)

messenger.start()

while (true) do
  messenger.receive()
  messenger.get(message)
  print "Got: #{message}\n"
  messenger.accept()
end

messenger.stop()
