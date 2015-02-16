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

use qpid_proton;
use async;

package async::Receiver;

@ISA = (async::CallbackAdapter);

sub on_start {
    my ($self) = @_;
    my $args = $_[1] || ("amqp://~0.0.0.0");
    my $messenger = $self->{_messenger};

    foreach $arg ($args) {
        $messenger->subscribe($arg);
    }

    $messenger->receive();
}

sub on_receive {
    my ($self) = @_;
    my $msg = $_[1];
    my $message = $self->{_message};
    my $text = "";

    if (defined($msg->get_body)) {
        $text = $msg->get_body;
        if ($text eq "die") {
            $self->stop;
        }
    } else {
        $text = $message->get_subject;
    }

    $text = "" if (!defined($text));

    print "Received: $text\n";

    if ($msg->get_reply_to) {
        print "Sending reply to: " . $msg->get_reply_to . "\n";
        $message->clear;
        $message->set_address($msg->get_reply_to());
        $message->set_body("Reply for ", $msg->get_body);
        $self->send($message);
    }
}

sub on_status {
    my ($self) = @_;
    my $messenger = $self->{_messenger};
    my $status = $_[1];

    print "Status: ", $status, "\n";
}

sub on_stop {
    print "Stopped.\n"
}

package main;

our $messenger = new qpid::proton::Messenger();
our $app = new async::Receiver($messenger);

$app->run();
