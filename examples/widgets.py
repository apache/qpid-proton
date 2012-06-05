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

class SizeGraph:

  def __init__(self, queue):
    self.queue = queue

  def draw(self, cr, width, height):
    if self.queue.threshold is None:
      level = self.queue.size/1000.0
    else:
      level = (self.queue.size/(self.queue.threshold*1.1))

    cr.set_source_rgb(0.0, 0.0, 0.8)
    cr.rectangle(0, 0, 1, level)
    cr.fill()

    cr.set_source_rgb(0.5, 0.5, 0.5)
    cr.set_line_width(0.01)
    cr.rectangle(0, 0, 1, 1)
    if self.queue.threshold is not None:
      cr.move_to(0, 1/1.1)
      cr.line_to(1, 1/1.1)
    cr.stroke()
