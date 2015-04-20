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

"""
Parsers for XML to dom.
"""
from __future__ import absolute_import

import xml.sax.handler
from .dom import *

class Parser:

  def __init__(self):
    self.tree = Tree()
    self.node = self.tree
    self.nodes = []

  def line(self, id, lineno, colno):
    while self.nodes:
      n = self.nodes.pop()
      n._line(id, lineno, colno)

  def add(self, node):
    self.node.add(node)
    self.nodes.append(node)

  def start(self, name, attrs):
    tag = Tag(name, *attrs)
    self.add(tag)
    self.node = tag

  def end(self, name):
    self.balance(name)
    self.node = self.node.parent

  def data(self, data):
    children = self.node.children
    if children and isinstance(children[-1], Data):
      children[-1].data += data
    else:
      self.add(Data(data))

  def comment(self, comment):
    self.add(Comment(comment))

  def entity(self, ref):
    self.add(Entity(ref))

  def character(self, ref):
    self.add(Character(ref))

  def balance(self, name = None):
    while self.node != self.tree and name != self.node.name:
      self.node.parent.extend(self.node.children)
      del self.node.children[:]
      self.node.singleton = True
      self.node = self.node.parent


class XMLParser(xml.sax.handler.ContentHandler):

  def __init__(self):
    self.parser = Parser()
    self.locator = None

  def line(self):
    if self.locator != None:
      self.parser.line(self.locator.getSystemId(),
                       self.locator.getLineNumber(),
                       self.locator.getColumnNumber())

  def setDocumentLocator(self, locator):
    self.locator = locator

  def startElement(self, name, attrs):
    self.parser.start(name, attrs.items())
    self.line()

  def endElement(self, name):
    self.parser.end(name)
    self.line()

  def characters(self, content):
    self.parser.data(content)
    self.line()

  def skippedEntity(self, name):
    self.parser.entity(name)
    self.line()

