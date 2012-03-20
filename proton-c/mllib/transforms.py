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
Useful transforms for dom objects.
"""

import dom
from cStringIO import StringIO

class Visitor:

  def descend(self, node):
    for child in node.children:
      child.dispatch(self)

  def node(self, node):
    self.descend(node)

  def leaf(self, leaf):
    pass

class Identity:

  def descend(self, node):
    result = []
    for child in node.children:
      result.append(child.dispatch(self))
    return result

  def default(self, tag):
    result = dom.Tag(tag.name, *tag.attrs)
    result.extend(self.descend(tag))
    return result

  def tree(self, tree):
    result = dom.Tree()
    result.extend(self.descend(tree))
    return result

  def tag(self, tag):
    return self.default(tag)

  def leaf(self, leaf):
    return leaf.__class__(leaf.data)

class Sexp(Identity):

  def __init__(self):
    self.stack = []
    self.level = 0
    self.out = ""

  def open(self, s):
    self.out += "(%s" % s
    self.level += len(s) + 1
    self.stack.append(s)

  def line(self, s = ""):
    self.out = self.out.rstrip()
    self.out += "\n" + " "*self.level + s

  def close(self):
    s = self.stack.pop()
    self.level -= len(s) + 1
    self.out = self.out.rstrip()
    self.out += ")"

  def tree(self, tree):
    self.open("+ ")
    for child in tree.children:
      self.line(); child.dispatch(self)
    self.close()

  def tag(self, tag):
    self.open("Node(%s) " % tag.name)
    for child in tag.children:
      self.line(); child.dispatch(self)
    self.close()

  def leaf(self, leaf):
    self.line("%s(%s)" % (leaf.__class__.__name__, leaf.data))

class Output:

  def descend(self, node):
    out = StringIO()
    for child in node.children:
      out.write(child.dispatch(self))
    return out.getvalue()

  def default(self, tag):
    out = StringIO()
    out.write("<%s" % tag.name)
    for k, v in tag.attrs:
      out.write(' %s="%s"' % (k, v))
    out.write(">")
    out.write(self.descend(tag))
    if not tag.singleton:
      out.write("</%s>" % tag.name)
    return out.getvalue()

  def tree(self, tree):
    return self.descend(tree)

  def tag(self, tag):
    return self.default(tag)

  def data(self, leaf):
    return leaf.data

  def entity(self, leaf):
    return "&%s;" % leaf.data

  def character(self, leaf):
    raise Exception("TODO")

  def comment(self, leaf):
    return "<!-- %s -->" % leaf.data

class Empty(Output):

  def tag(self, tag):
    return self.descend(tag)

  def data(self, leaf):
    return ""

  def entity(self, leaf):
    return ""

  def character(self, leaf):
    return ""

  def comment(self, leaf):
    return ""

class Text(Empty):

  def data(self, leaf):
    return leaf.data

  def entity(self, leaf):
    return "&%s;" % leaf.data

  def character(self, leaf):
    # XXX: is this right?
    return "&#%s;" % leaf.data
