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

import pygtk
pygtk.require('2.0')
import gtk, gobject, cairo

class Screen(gtk.DrawingArea):

  __gsignals__ = { "expose-event": "override" }

  def __init__(self):
    gtk.DrawingArea.__init__(self)
    self.widgets = []

  def add(self, widget, x, y, w, h):
    widget.__x = x
    widget.__y = y
    widget.__w = w
    widget.__h = h
    self.widgets.append(widget)

  def remove(self, widget):
    self.widgets.remove(widget)

  def do_expose_event(self, event):
    cr = self.window.cairo_create()
    cr.rectangle(event.area.x, event.area.y,
                 event.area.width, event.area.height)
    cr.clip()
    self.draw(cr, *self.window.get_size())

  def draw(self, cr, width, height):
    cr.set_source_rgb(1.0, 1.0, 1.0)
    cr.rectangle(0, 0, width, height)
    cr.fill()

    cr.translate(0, height)
    cr.scale(width/1.0, -height/1.0)
    cr.scale(9.0/16.0, 1.0)

    for widget in self.widgets:
      cr.save()
      cr.translate(widget.__x, widget.__y)
      cr.scale(widget.__w/1.0, widget.__h/1.0)
      widget.draw(cr, widget.__w, widget.__h)
      cr.restore()

class Window:

  def __init__(self, quit=None, width=1280, height=720, resizable=True, fullscreen=False):
    self.quit = quit
    self.window = gtk.Window()
    if fullscreen:
      self.window.fullscreen()
    elif width and height:
      self.window.set_resizable(resizable)
      self.window.set_geometry_hints(min_width=width, min_height=height)
    if self.quit:
      self.window.connect("delete-event", quit)
    self.screen = Screen()
    self.screen.show()
    self.window.add(self.screen)
    self.window.present()

  def add(self, widget, x, y, w, h):
    self.screen.add(widget, x, y, w, h)

  def remove(self, widget):
    self.screen.remove(widget)

  def redraw(self):
    self.window.queue_draw()
    while gtk.events_pending():
      gtk.main_iteration(block=False)
    return True

  def run(self):
    gtk.main()
