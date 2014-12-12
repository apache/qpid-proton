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

use Getopt::Std;
use qpid_proton;
use async;

$Getopt::Std::STANDARD_HELP_VERSION = 1;

sub VERSION_MESSAGE() {}

sub HELP_MESSAGE() {
    print "Usage: send_async.pl [OPTIONS] <msg_0> <msg_1> ...\n";
    print "Options:\n";
    print "\t-a     - the message address (def. amqp://0.0.0.0)\n";
    print "\t-r     - the reply-to address: //<domain>[/<name>]\n";
    print "\t msg_# - a text string to send\n";
}

my %optons = ();
getopts("a:r:", \%options) or usage();

our $address = $options{a} || "amqp://0.0.0.0";
our $replyto = $options{r} || "~/#";

package async::Sender;

@ISA = (async::CallbackAdapter);

sub on_start {
    my ($self) = @_;
    my $message = $self->{_message};
    my $messenger = $self->{_messenger};
    my $args = $_[1] || ("Hello world!");

    print "Started\n";

    $message->clear;
    $message->set_address("amqp://0.0.0.0");
    $message->set_reply_to($replyto) if (defined($replyto));

    foreach $arg ($args) {
        $message->set_body($arg);
        if ($replyto) {
            $message->set_reply_to($replyto);
        }
        $self->send($message, "on_status");
    }

    $messenger->receive() if (defined($replyto));
}

sub on_status {
    my ($self) = @_;
    my $messenger = $self->{_messenger};
    my $status = $_[1] || "";

    print "Status: ", $status, "\n";
}

sub on_receive {
    my ($self) = @_;
    my $message = $_[1];
    my $text = $message->get_body || "[empty]";

    print "Received: " . $text . "\n";

    $self->stop();
}

sub on_stop {
    print "Stopped\n";
}


package main;

our $msgr = new qpid::proton::Messenger();
our $app = async::Sender->new($msgr);

$app->run;
