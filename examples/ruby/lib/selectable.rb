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

class Selectable

  attr_reader :transport

  def initialize(transport, socket)
    @transport = transport
    @socket = socket
    @socket.autoclose = true
    @write_done = false
    @read_done = false
  end

  def closed?
    return true if @socket.closed?
    return false if !@read_done && !@write_done
    @socket.close
    true
  end

  def fileno
    @socket.fileno unless @socket.closed?
  end

  def to_io
    @socket
  end

  def reading?
    return false if @read_done
    c = @transport.capacity
    if c > 0
      return true
    elsif c < 0
      @read_done = true
      return false
    else
      return false
    end
  end

  def writing?
    return false if @write_done
    begin
      p = @transport.pending
      if p > 0
        return true
      elsif p < 0
        @write_done = true
        return false
      else
        return false
      end
    rescue Qpid::Proton::TransportError => error
      @write_done = true
      return false
    end
  end

  def readable
    c = @transport.capacity
    if c > 0
      begin
        data = @socket.recv(c)
        if data
          @transport.push(data)
        else
          @transport.close_tail
        end
      rescue Exception => error
        puts "read error; #{error}"
        @transport.close_tail
        @read_done = true
      end
    elsif c < 0
      @read_done = true
    end
  end

  def writable
    begin
      p = @transport.pending
      if p > 0
        data = @transport.peek(p)
        n = @socket.send(data, 0)
        @transport.pop(n)
      elsif p < 0
        @write_done = true
      end
    rescue Exception => error
      puts "write error: #{error}"
      puts error.backtrace.join("\n")
      @transport.close_head
      @write_done = true
    end
  end

  def tick(now)
    @transport.tick(now)
  end

end
