#!/usr/bin/env python
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

from distutils.core import setup, Extension

setup(name='python-qpid-proton',
      version='0.9',
      description='An AMQP based messaging library.',
      author='Apache Qpid',
      author_email='proton@qpid.apache.org',
      url='http://qpid.apache.org/proton/',
      packages=['proton'],
      py_modules=['cproton'],
      license="Apache Software License",
      classifiers=["License :: OSI Approved :: Apache Software License",
                   "Intended Audience :: Developers",
                   "Programming Language :: Python"],
      ext_modules=[Extension('_cproton', ['cproton.i'],
                             libraries=['qpid-proton'])])
