#ifndef PROTON_IMPORT_EXPORT_H
#define PROTON_IMPORT_EXPORT_H 1

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

/**
 * @cond INTERNAL
 */

/*
  Compiler specific mechanisms for managing the import and export of
  symbols between shared objects. 
  PN_EXPORT         - Export declaration 
  PN_IMPORT         - Import declaration
*/

#if defined(WIN32) && !defined(PROTON_DECLARE_STATIC)
/* Import and Export definitions for Windows: */
#  define PN_EXPORT __declspec(dllexport)
#  define PN_IMPORT __declspec(dllimport)
#else
/* Non-Windows (Linux, etc.) definitions */
#  define PN_EXPORT __attribute ((visibility ("default")))
#  define PN_IMPORT
#endif


/* For core proton library symbols */
#if defined(qpid_proton_core_EXPORTS) || defined(qpid_proton_EXPORTS)
#  define PN_EXTERN PN_EXPORT
#else
#  define PN_EXTERN PN_IMPORT
#endif

/* For proactor proton symbols */
#if defined(qpid_proton_proactor_EXPORTS) || defined(qpid_proton_EXPORTS)
#  define PNP_EXTERN PN_EXPORT
#else
#  define PNP_EXTERN PN_IMPORT
#endif

/* For extra proton symbols */
#if defined(qpid_proton_EXPORTS)
#  define PNX_EXTERN PN_EXPORT
#else
#  define PNX_EXTERN PN_IMPORT
#endif

/**
 * @endcond
 */

#endif /* import_export.h */
