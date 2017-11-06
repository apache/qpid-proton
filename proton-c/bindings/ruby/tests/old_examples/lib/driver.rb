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

class Driver

  def initialize
    @selectables = {}
  end

  def add(selectable)
    @selectables[selectable.fileno] = selectable
  end

  def process
    reading = []
    writing = []

    @selectables.each_value do |sel|
      if sel.closed? || sel.fileno.nil?
        @selectables.delete(sel.fileno)
      else
        begin
          reading << sel.to_io if sel.reading?
          writing << sel.to_io if sel.writing?
        rescue Exception => error
          puts "Error: #{error}"
          puts error.backtrace.join("\n");
          # @selectables.delete(sel.fileno)
        end
      end
    end

    read_from, write_to = IO.select(reading, writing, [], 0)

    unless read_from.nil?
      read_from.each do |r|
        sel = @selectables[r.fileno]
        sel.readable unless sel.nil? || sel.closed?
      end
    end

    begin
      unless write_to.nil?
        write_to.each do |w|
          sel = @selectables[w.fileno]
          sel.writable unless sel.nil? || sel.closed?
        end
      end

    end
  end

end
