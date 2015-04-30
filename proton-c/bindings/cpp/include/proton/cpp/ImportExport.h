#ifndef PROTON_CPP_IMPORTEXPORT_H
#define PROTON_CPP_IMPORTEXPORT_H

/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */
#if defined(WIN32) && !defined(PROTON_CPP_DECLARE_STATIC)
  //
  // Import and Export definitions for Windows:
  //
#  define PROTON_CPP_EXPORT __declspec(dllexport)
#  define PROTON_CPP_IMPORT __declspec(dllimport)
#else
  //
  // Non-Windows (Linux, etc.) definitions:
  //
#  define PROTON_CPP_EXPORT
#  define PROTON_CPP_IMPORT
#endif


// For c++ library symbols

#ifdef protoncpp_EXPORTS
#  define PROTON_CPP_EXTERN PROTON_CPP_EXPORT
#else
#  define PROTON_CPP_EXTERN PROTON_CPP_IMPORT
#endif

// TODO:
#define PROTON_CPP_INLINE_EXTERN

#endif  /*!PROTON_CPP_IMPORTEXPORT_H*/
