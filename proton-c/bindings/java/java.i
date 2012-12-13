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
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
%module Proton
%include "arrays_java.i"
%include "typemaps.i"
%include "various.i"


%{
/* Includes the header in the wrapper code */
#include <proton/engine.h>
#include <proton/message.h>
#include <proton/sasl.h>
#include <proton/ssl.h>

#include <proton/driver.h>
#include <proton/messenger.h>

%}


%typemap(in) (char *DATA, size_t SIZE) (char *data, jobject arr, jboolean isDirect) {
  /* %typemap(in) void * */
  jclass bbclass = JCALL1(GetObjectClass,jenv, $input);
  jmethodID isDirectId = JCALL3(GetMethodID,jenv, bbclass, "isDirect", "()Z");
  jmethodID positionId = JCALL3(GetMethodID,jenv, bbclass, "position", "()I");
  jmethodID remainingId= JCALL3(GetMethodID,jenv, bbclass, "remaining", "()I");
  isDirect = JCALL2(CallBooleanMethod,jenv, $input, isDirectId);
  jint position = JCALL2(CallIntMethod,jenv, $input, positionId);
  $2 = (long)(JCALL2(CallIntMethod,jenv, $input, remainingId));

  if(isDirect) {
    $1 = JCALL1(GetDirectBufferAddress,jenv, $input) + position;
    data = (char *)0;
    arr = (jobject) 0;
  } else {
    jmethodID arrayId= JCALL3(GetMethodID,jenv, bbclass, "array", "()[B");
    jmethodID arrayOffsetId= JCALL3(GetMethodID,jenv, bbclass, "arrayOffset", "()I");
    jobject a = JCALL2(CallObjectMethod,jenv, $input, arrayId);
    arr = a;
    data = JCALL2(GetPrimitiveArrayCritical, jenv, a, NULL);
    $1 = data + JCALL2(CallIntMethod,jenv, $input, arrayOffsetId) + position;

  }
}

%typemap(jni) (char *DATA, size_t SIZE) "jobject"
%typemap(jtype) (char *DATA, size_t SIZE) "java.nio.ByteBuffer"
%typemap(jstype) (char *DATA, size_t SIZE) "java.nio.ByteBuffer"
%typemap(javain) (char *DATA, size_t SIZE) "$javainput"
%typemap(javaout) (char *DATA, size_t SIZE) {

  return $jnicall;
}

%typemap(freearg) (char *DATA, size_t SIZE)  {

  if(!isDirect$argnum) {
    JCALL3(ReleasePrimitiveArrayCritical, jenv, arr$argnum, data$argnum,0);
  }
}




%typemap(in) (char *DATA, size_t *SIZE) (char *data, jobject array, jboolean isDirect, jclass bbclass) {
  /* %typemap(in) void * */
  bbclass = JCALL1(GetObjectClass,jenv, $input);
  jmethodID isDirectId = JCALL3(GetMethodID,jenv, bbclass, "isDirect", "()Z");
  jmethodID positionId = JCALL3(GetMethodID,jenv, bbclass, "position", "()I");
  jmethodID remainingId= JCALL3(GetMethodID,jenv, bbclass, "remaining", "()I");
  isDirect = JCALL2(CallBooleanMethod,jenv, $input, isDirectId);
  jint position = JCALL2(CallIntMethod,jenv, $input, positionId);
  long size = (long)(JCALL2(CallIntMethod,jenv, $input, remainingId));
  $2 = &size;

  if(isDirect) {
    $1 = JCALL1(GetDirectBufferAddress,jenv, $input) + position;
    data = (char *)0;
    array = (jobject) 0;
  } else {
    jmethodID arrayId= JCALL3(GetMethodID,jenv, bbclass, "array", "()[B");
    jmethodID arrayOffsetId= JCALL3(GetMethodID,jenv, bbclass, "arrayOffset", "()I");
    array = JCALL2(CallObjectMethod,jenv, $input, arrayId);
    //data = JCALL2(GetByteArrayElements, jenv, array, NULL);
    jobject a = array;
    data = (char *) JCALL2(GetPrimitiveArrayCritical, jenv, a, NULL);
    //data = (char *) (*jenv)->GetPrimitiveArrayCritical(jenv, array, NULL);
    //printf("Acquired  %p from %p\n", data, a);

    $1 = data + JCALL2(CallIntMethod,jenv, $input, arrayOffsetId) + position;

  }
}
%typemap(jni) (char *DATA, size_t *SIZE) "jobject"
%typemap(jtype) (char *DATA, size_t *SIZE) "java.nio.ByteBuffer"
%typemap(jstype) (char *DATA, size_t *SIZE) "java.nio.ByteBuffer"
%typemap(javain) (char *DATA, size_t *SIZE) "$javainput"
%typemap(javaout) (char *DATA, size_t *SIZE) {
  return $jnicall;
}

%typemap(freearg) (char *DATA, size_t *SIZE) {


  if(!isDirect$argnum) {

    //JCALL3(ReleaseByteArrayElements, jenv, array, $1 - ((*jenv)->CallIntMethod(jenv, $input, arrayOffsetId) + position),0);
    JCALL3(ReleasePrimitiveArrayCritical, jenv, array$argnum, data$argnum,0);
    //printf("Releasing %p from %p\n", data$argnum, array$argnum);
  }

  jmethodID setpositionId = (*jenv)->GetMethodID(jenv, bbclass$argnum, "position", "(I)Ljava/nio/Buffer;");

  jint pos = (int)*$2;
  // todo - need to increment not just set
  JCALL3(CallObjectMethod,jenv,$input,setpositionId,pos);
}














%rename(pn_connection_get_context) wrap_pn_connection_get_context;
%inline {
  jobject wrap_pn_connection_get_context(pn_connection_t *c) {
    jobject result = (jobject) pn_connection_get_context(c);
    return result;
  }
}
%ignore pn_connection_get_context;

%native (pn_connection_set_context) void pn_connection_set_context(pn_connection_t, jobject);
%{
JNIEXPORT void JNICALL Java_org_apache_qpid_proton_jni_ProtonJNI_pn_1connection_1set_1context(JNIEnv *jenv, jclass jcls,
                                                              jlong jarg1, jobject context)
{
    pn_connection_t *c = *(pn_connection_t **)&jarg1;

    jobject oldContext = (jobject) pn_connection_get_context(c);
    if(oldContext) {
      (*jenv)->DeleteGlobalRef(jenv, oldContext);
    }
    jobject newContext = NULL;
    if(context)
    {
        newContext = (*jenv)->NewGlobalRef(jenv, context);
    }
    pn_connection_set_context(c, newContext);

}
%}
%ignore pn_connection_set_context;


%rename(pn_session_get_context) wrap_pn_session_get_context;
%inline {
  jobject wrap_pn_session_get_context(pn_session_t *c) {
    jobject result = (jobject) pn_session_get_context(c);
    return result;
  }
}
%ignore pn_session_get_context;

%native (pn_session_set_context) void pn_session_set_context(pn_session_t, jobject);
%{
JNIEXPORT void JNICALL Java_org_apache_qpid_proton_jni_ProtonJNI_pn_1session_1set_1context(JNIEnv *jenv, jclass jcls,
                                                              jlong jarg1, jobject context)
{
    pn_session_t *c = *(pn_session_t **)&jarg1;

    jobject oldContext = (jobject) pn_session_get_context(c);
    if(oldContext) {
      (*jenv)->DeleteGlobalRef(jenv, oldContext);
    }
    jobject newContext = NULL;
    if(context)
    {
        newContext = (*jenv)->NewGlobalRef(jenv, context);
    }
    pn_session_set_context(c, newContext);

}
%}
%ignore pn_session_set_context;

%rename(pn_link_get_context) wrap_pn_link_get_context;
%inline {
  jobject wrap_pn_link_get_context(pn_link_t *c) {
    jobject result = (jobject) pn_link_get_context(c);
    return result;
  }
}
%ignore pn_link_get_context;

%native (pn_link_set_context) void pn_link_set_context(pn_link_t, jobject);
%{
JNIEXPORT void JNICALL Java_org_apache_qpid_proton_jni_ProtonJNI_pn_1link_1set_1context(JNIEnv *jenv, jclass jcls,
                                                              jlong jarg1, jobject context)
{
    pn_link_t *c = *(pn_link_t **)&jarg1;
    void* oldContext = pn_link_get_context(c);

    if(oldContext!=NULL) {
      (*jenv)->DeleteGlobalRef(jenv, (jobject)oldContext);
    }
    jobject newContext = NULL;
    if(context!=NULL)
    {
        newContext = (*jenv)->NewGlobalRef(jenv, context);
    }
    pn_link_set_context(c, newContext);

}
%}
%ignore pn_link_set_context;

%rename(pn_delivery_get_context) wrap_pn_delivery_get_context;
%inline {
  jobject wrap_pn_delivery_get_context(pn_delivery_t *c) {
    jobject result = (jobject) pn_delivery_get_context(c);
    return result;
  }
}
%ignore pn_delivery_get_context;

%native (pn_delivery_set_context) void pn_delivery_set_context(pn_delivery_t, jobject);
%{
JNIEXPORT void JNICALL Java_org_apache_qpid_proton_jni_ProtonJNI_pn_1delivery_1set_1context(JNIEnv *jenv, jclass jcls,
                                                              jlong jarg1, jobject context)
{

    pn_delivery_t *c = *(pn_delivery_t **)&jarg1;
    jobject oldContext = (jobject) pn_delivery_get_context(c);
    if(oldContext) {
      (*jenv)->DeleteGlobalRef(jenv, oldContext);
    }
    jobject newContext = NULL;
    if(context)
    {
        newContext = (*jenv)->NewGlobalRef(jenv, context);
    }
    pn_delivery_set_context(c, newContext);

}
%}
%ignore pn_delivery_set_context;

%native (pn_delivery_tag) jbyteArray pn_delivery_tag(pn_delivery_t);
%{
JNIEXPORT jbyteArray JNICALL Java_org_apache_qpid_proton_jni_ProtonJNI_pn_1delivery_1tag(JNIEnv *jenv, jclass jcls,
                                                              jlong jarg1)
{

    pn_delivery_t *c = *(pn_delivery_t **)&jarg1;
    pn_delivery_tag_t tag = pn_delivery_tag(c);
    jbyteArray rval = (*jenv)->NewByteArray(jenv, tag.size);
    jbyte* barr = (*jenv)->GetByteArrayElements(jenv, rval, NULL);
    jint i = 0;
    for(i=0;i<tag.size;i++)
    {
      barr[i] = tag.bytes[i];
    }
    (*jenv)->ReleaseByteArrayElements(jenv, rval, barr, 0);

    return (jbyteArray) (*jenv)->NewGlobalRef(jenv, rval);

}
%}
%ignore pn_delivery_tag;

%native (pn_bytes_to_array) jbyteArray pn_bytes_to_array(pn_bytes_t);
%{
JNIEXPORT jbyteArray JNICALL Java_org_apache_qpid_proton_jni_ProtonJNI_pn_1bytes_1to_1array(JNIEnv *jenv, jclass jcls, jlong jarg1)
{
    pn_bytes_t* b = *(pn_bytes_t **)&jarg1;
    jbyteArray rval = (*jenv)->NewByteArray(jenv, b->size);
    jbyte* barr = (*jenv)->GetByteArrayElements(jenv, rval, NULL);
    jint i = 0;
    for(i=0;i<b->size;i++)
    {
      barr[i] = b->start[i];
    }
    (*jenv)->ReleaseByteArrayElements(jenv, rval, barr, 0);

    return (jbyteArray) (*jenv)->NewGlobalRef(jenv, rval);

}
%}



ssize_t  pn_transport_input(pn_transport_t *transport, char *DATA, size_t SIZE);
%ignore pn_transport_input;

ssize_t pn_transport_output(pn_transport_t *transport, char *DATA, size_t SIZE);
%ignore pn_transport_output;

ssize_t pn_link_recv(pn_link_t *receiver, char *DATA, size_t SIZE);
%ignore pn_link_recv;

ssize_t pn_link_send(pn_link_t *sender, char *DATA, size_t SIZE);
%ignore pn_link_send;

ssize_t pn_sasl_recv(pn_sasl_t *sasl, char *DATA, size_t SIZE);
%ignore pn_sasl_recv;

ssize_t pn_sasl_send(pn_sasl_t *sasl, char *DATA, size_t SIZE);
%ignore pn_sasl_send;

%typemap(jni) char *start "jbyteArray"
%typemap(jtype) char *start "byte[]"
%typemap(jstype) char *start "byte[]"
%typemap(javaout) char *start {
    return $jnicall;
}

%typemap(out) char *start {
    // TODO - RG
    $result = JCALL1(NewByteArray, jenv, arg1->size);
    JCALL4(SetByteArrayRegion, jenv, $result, 0, arg1->size, $1);
}

%typemap(in) char *start {
    jbyte* barr = (*jenv)->GetByteArrayElements(jenv, $input, NULL);
    jsize length = (*jenv)->GetArrayLength(jenv, $input);
    char *buf = malloc(length);
    jint i = 0;
    for(i=0;i<length;i++)
    {
      buf[i] = barr[i];
    }
    (*jenv)->ReleaseByteArrayElements(jenv, $input, barr, 0);
    $1 = buf;
}
%typemap(freearg) char *start  {

}


%rename(pn_delivery) wrap_pn_delivery;
%inline %{
  pn_delivery_t *wrap_pn_delivery(pn_link_t *link, char *STRING, size_t LENGTH) {
    return pn_delivery(link, pn_dtag(STRING, LENGTH));
  }
%}
%ignore pn_delivery;

int pn_data_encode(pn_data_t *data, char *DATA, size_t SIZE);
%ignore pn_data_encode;

int pn_data_decode(pn_data_t *data, char *DATA, size_t SIZE);
%ignore pn_data_decode;

int pn_message_decode(pn_message_t *msg, char *DATA, size_t SIZE);
%ignore pn_message_decode;

int pn_message_encode(pn_message_t *msg, char *DATA, size_t *SIZE);
%ignore pn_message_encode;

int pn_message_load(pn_message_t *msg, char *DATA, size_t SIZE);
%ignore pn_message_load;

int pn_message_load_data(pn_message_t *msg, char *DATA, size_t SIZE);
%ignore pn_message_load_data;

int pn_message_load_text(pn_message_t *msg, char *DATA, size_t SIZE);
%ignore pn_message_load_text;

int pn_message_load_amqp(pn_message_t *msg, char *DATA, size_t SIZE);
%ignore pn_message_load_amqp;

int pn_message_load_json(pn_message_t *msg, char *DATA, size_t SIZE);
%ignore pn_message_load_json;

int pn_message_save(pn_message_t *message, char *DATA, size_t *SIZE);
%ignore pn_message_save;

int pn_message_encode(pn_message_t *msg, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_message_encode;

int pn_message_save(pn_message_t *msg, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_message_save;

int pn_message_save_data(pn_message_t *msg, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_message_save_data;

int pn_message_save_text(pn_message_t *msg, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_message_save_text;

int pn_message_save_amqp(pn_message_t *msg, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_message_save_amqp;

int pn_message_save_json(pn_message_t *msg, char *OUTPUT, size_t *OUTPUT_SIZE);
%ignore pn_message_save_json;

bool pn_ssl_get_cipher_name(pn_ssl_t *ssl, char *DATA, size_t SIZE);
%ignore pn_ssl_get_cipher_name;

bool pn_ssl_get_protocol_name(pn_ssl_t *ssl, char *DATA, size_t SIZE);
%ignore pn_ssl_get_protocol_name;




%include "proton/cproton.i"


