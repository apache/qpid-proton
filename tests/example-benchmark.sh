#!/bin/bash
# Simple script to time running the examples in various languages.
# Should be run in the build directory.
# Use command line to run different combinations.
# For example to run C broker, Go sender and C++ receiver pass arguments: C GO CPP

MESSAGES=100000

BLD=$(pwd)

C=$BLD/c/examples
C_BROKER="$C/broker"
C_SEND="$C/send localhost amqp x $MESSAGES"
C_RECV="$C/receive localhost amqp x $MESSAGES"

GO=$BLD/go/examples/electron
GO_BROKER="$GO/broker"
GO_SEND="$GO/send -count $MESSAGES /x"
GO_RECV="$GO/receive -count $MESSAGES -prefetch 100 /x"

CPP=$BLD/cpp/examples
CPP_BROKER="$CPP/broker"
CPP_SEND="$CPP/simple_send -a /x -m $MESSAGES"
CPP_RECV="$CPP/simple_recv -a /x -m $MESSAGES"

amqp_busy() { ss -tlp | grep $* amqp; }

start_broker() {
    amqp_busy && { "amqp port busy"; exit 1; }
    "$@" & BROKER_PID=$!
    until amqp_busy -q; do sleep .1; done
}

stop_broker() {
    kill $BROKER_PID
    wait $BROKER_PID 2> /dev/null
}

run() {
    echo
    echo "run: $*"
    BROKER=${!1}
    SEND=${!2}
    RECV=${!3}
    start_broker $BROKER
    time {
        $RECV > /dev/null& RECV_PID=$!
        $SEND
        wait $RECV_PID
    }
    stop_broker
}

if test -z "$*"; then
    # By default run the broker/sender/receiver combo for each language
    run C_BROKER C_SEND C_RECV
    run GO_BROKER GO_SEND GO_RECV
    run CPP_BROKER CPP_SEND CPP_RECV
else
    while test -n "$*"; do
        run ${1}_BROKER ${2}_SEND ${3}_RECV
        shift; shift; shift
    done
fi
