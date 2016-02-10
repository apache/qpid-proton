#ifndef PN_CPP_IMPORTEXPORT_H
#define PN_CPP_IMPORTEXPORT_H

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

/// @cond INTERNAL

/// import/export macros
#if defined(WIN32) && !defined(PN_CPP_DECLARE_STATIC)
  //
  // Import and Export definitions for Windows:
  //
#  define PN_CPP_EXPORT __declspec(dllexport)
#  define PN_CPP_IMPORT __declspec(dllimport)
#  define PN_CPP_CLASS_EXPORT
#  define PN_CPP_CLASS_IMPORT
#else
  //
  // Non-Windows (Linux, etc.) definitions:
  //
#  define PN_CPP_EXPORT __attribute ((visibility ("default")))
#  define PN_CPP_IMPORT
#  define PN_CPP_CLASS_EXPORT __attribute ((visibility ("default")))
#  define PN_CPP_CLASS_IMPORT
#endif

// For qpid-proton-cpp library symbols
#ifdef qpid_proton_cpp_EXPORTS
#  define PN_CPP_EXTERN PN_CPP_EXPORT
#  define PN_CPP_CLASS_EXTERN PN_CPP_CLASS_EXPORT
#else
#  define PN_CPP_EXTERN PN_CPP_IMPORT
#  define PN_CPP_CLASS_EXTERN PN_CPP_CLASS_IMPORT
#endif

/// @endcond

#endif // PN_CPP_IMPORTEXPORT_H
