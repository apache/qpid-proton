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


from typing import Any, Callable, List, Optional, Tuple, Type, Union
from types import TracebackType


class LazyHandlers(object):
    def __get__(self, obj: 'Handler', clazz: Any) -> Union['LazyHandlers', List[Any]]:
        if obj is None:
            return self
        ret = []
        obj.__dict__['handlers'] = ret
        return ret


class Handler(object):
    """
    An abstract handler for events which supports child handlers.
    """
    handlers = LazyHandlers()

    # TODO What to do with on_error?
    def add(
            self,
            handler: Any,
            on_error: Optional[Callable[[Tuple[Type[BaseException], BaseException, 'TracebackType']], None]] = None,
    ) -> None:
        """
        Add a child handler

        :param handler: A child handler
        :type handler: :class:`Handler` or one of its derivatives.
        :param on_error: Not used
        """
        self.handlers.append(handler)

    def on_unhandled(self, method: str, *args) -> None:
        """
        The callback for handling events which are not handled by
        any other handler.

        :param method: The name of the intended handler method.
        :param args: Arguments for the intended handler method.
        """
        pass
