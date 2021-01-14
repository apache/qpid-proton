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

from __future__ import absolute_import

from cproton import pn_condition_clear, pn_condition_set_name, pn_condition_set_description, pn_condition_info, \
    pn_condition_is_set, pn_condition_get_name, pn_condition_get_description

from ._data import Data, dat2obj


class Condition:
    """
    An AMQP Condition object. Conditions hold exception information
    pertaining to the closing of an AMQP endpoint such as a :class:`Connection`,
    :class:`Session`, or :class:`Link`. Conditions also hold similar information
    pertaining to deliveries that have reached terminal states.
    Connections, Sessions, Links, and Deliveries may all have local and
    remote conditions associated with them.

    The local condition may be modified by the local endpoint to signal
    a particular condition to the remote peer. The remote condition may
    be examined by the local endpoint to detect whatever condition the
    remote peer may be signaling. Although often conditions are used to
    indicate errors, not all conditions are errors *per/se*, e.g.
    conditions may be used to redirect a connection from one host to
    another.

    Every condition has a short symbolic name, a longer description,
    and an additional info map associated with it. The name identifies
    the formally defined condition, and the map contains additional
    information relevant to the identified condition.

    :ivar ~.name: The name of the condition.
    :vartype ~.name: ``str``
    :ivar ~.description: A description of the condition.
    :vartype ~.description: ``str``
    :ivar ~.info: A data object that holds the additional information associated
        with the condition. The data object may be used both to access and to
        modify the additional information associated with the condition.
    :vartype ~.info: :class:`Data`
    """

    def __init__(self, name, description=None, info=None):
        self.name = name
        self.description = description
        self.info = info

    def __repr__(self):
        return "Condition(%s)" % ", ".join([repr(x) for x in
                                            (self.name, self.description, self.info)
                                            if x])

    def __eq__(self, o):
        if not isinstance(o, Condition):
            return False
        return self.name == o.name and \
            self.description == o.description and \
            self.info == o.info


def obj2cond(obj, cond):
    pn_condition_clear(cond)
    if obj:
        pn_condition_set_name(cond, str(obj.name))
        pn_condition_set_description(cond, obj.description)
        info = Data(pn_condition_info(cond))
        if obj.info:
            info.put_object(obj.info)


def cond2obj(cond):
    if pn_condition_is_set(cond):
        return Condition(pn_condition_get_name(cond),
                         pn_condition_get_description(cond),
                         dat2obj(pn_condition_info(cond)))
    else:
        return None
