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

    def __init__(self, name, description=None, info=None):
        self.name = name
        self.description = description
        self.info = info

    def __repr__(self):
        return "Condition(%s)" % ", ".join([repr(x) for x in
                                            (self.name, self.description, self.info)
                                            if x])

    def __eq__(self, o):
        if not isinstance(o, Condition): return False
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
