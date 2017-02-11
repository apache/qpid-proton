#ifndef _PROTON_EXAMPLES_C_PROACTOR_THREADS_H
#define _PROTON_EXAMPLES_C_PROACTOR_THREADS_H

/*
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
 * software distributed under the License is distributed on an * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/* EXAMPLE USE ONLY. Simulate the subset of POSIX threads used by examples for windows */

#ifdef _WIN32

#include <windows.h>
#include <time.h>
#define _WIN32_WINNT 0x500 /* WINBASE.H - Enable SignalObjectAndWait */
#include <process.h>
#include <windows.h>

#define pthread_function DWORD WINAPI
#define pthread_function_return DWORD
#define pthread_t HANDLE
#define pthread_create(thhandle,attr,thfunc,tharg) (int)((*thhandle=(HANDLE)_beginthreadex(NULL,0,(DWORD WINAPI(*)())thfunc,tharg,0,NULL))==NULL)
#define pthread_join(thread, result) ((WaitForSingleObject((thread),INFINITE)!=WAIT_OBJECT_0) || !CloseHandle(thread))
#define pthread_mutex_T HANDLE
#define pthread_mutex_init(pobject,pattr) (*pobject=CreateMutex(NULL,FALSE,NULL))
#define pthread_mutex_destroy(pobject) CloseHandle(*pobject)
#define pthread_mutex_lock(pobject) WaitForSingleObject(*pobject,INFINITE)
#define pthread_mutex_unlock(pobject) ReleaseMutex(*pobject)

#else

#include <pthread.h>

#endif

#endif
