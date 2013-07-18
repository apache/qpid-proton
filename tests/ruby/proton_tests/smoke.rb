#!/usr/bin/env ruby

require 'test/unit'
require 'qpid_proton'

class SmokeTest < Test::Unit::TestCase

  Messenger = Qpid::Proton::Messenger
  Message = Qpid::Proton::Message

  def setup
    @server = Messenger.new()
    @client = Messenger.new()
    @server.blocking = false
    @client.blocking = false
    @server.subscribe("~0.0.0.0:12345")
    @server.start()
    @client.start()
    pump()
  end

  def pump
    while (@server.work(0) or @client.work(0)) do end
  end

  def teardown
    @server.stop()
    @client.stop()

    pump()

    assert @client.stopped
    assert @server.stopped
  end

  def testSmoke(count=10)
    msg = Message.new()
    msg.address = "0.0.0.0:12345"

    @server.receive()

    count.times {|i|
      msg.content = "Hello World! #{i}"
      @client.put(msg)
    }

    msg2 = Message.new()

    count.times {|i|
      if (@server.incoming == 0) then
        pump()
      end
      @server.get(msg2)
      assert msg2.content == "Hello World! #{i}"
    }

    assert @client.outgoing == 0, @client.outgoing
    assert @server.incoming == 0, @server.incoming
  end

end
