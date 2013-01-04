#!/bin/bash

set -x

M2_REPO=/home/$USER/.m2/repository
CLASSPATH=.
CLASSPATH=$CLASSPATH:$M2_REPO/junit/junit/4.10/junit-4.10.jar
CLASSPATH=$CLASSPATH:target/test-classes/
CLASSPATH=$CLASSPATH:$M2_REPO/org/apache/qpid/proton-api/1.0-SNAPSHOT/proton-api-1.0-SNAPSHOT.jar

# Java implementation
# CLASSPATH=$CLASSPATH:/home/V510279/.m2/repository/org/apache/qpid/proton/1.0-SNAPSHOT/proton-1.0-SNAPSHOT.jar


# C implementation
PROTON_HOME=../..
CLASSPATH=$CLASSPATH:$PROTON_HOME/proton-c/build/bindings/java/proton-jni-0.2.jar
JNI_PATH=$PROTON_HOME/proton-c/build/bindings/java:$PROTON_HOME/proton-c/build
export LD_LIBRARY_PATH=$JNI_PATH:$LD_LIBRARY_PATH
# JNI_STUFF="-Djava.library.path=$JNI_PATH"

java -cp $CLASSPATH $JNI_STUFF org.junit.runner.JUnitCore org.apache.qpid.proton.systemtests.SimpleTest
