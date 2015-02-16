#!/usr/bin/env perl
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

use strict;
use warnings;
use Getopt::Std;

use qpid_proton;

$Getopt::Std::STANDARD_HELP_VERSION = 1;

sub VERSION_MESSAGE() {
}

sub HELP_MESSAGE() {
    print "Usage: send.pl [OPTIONS] -a <ADDRESS>\n";
    print "Options:\n";
    print "\t-s        - the message subject\n";
    print "\t-C        - the message content\n";
    print "\t<ADDRESS> - amqp://<domain>[/<name>]\n";
    print "\t-h        - this message\n";

    exit;
}

my %options = ();
getopts("a:C:s:h:", \%options) or HELP_MESSAGE();

my $address = $options{a} || "amqp://0.0.0.0";
my $subject = $options{s} || localtime(time);
my $content = $options{C} || "";

my $msg  = new qpid::proton::Message();
my $messenger = new qpid::proton::Messenger();

$messenger->start();

my @messages = @ARGV;
@messages = ("This is a test. " . localtime(time)) unless $messages[0];

foreach (@messages)
{
    $msg->set_address($address);
    $msg->set_subject($subject);
    $msg->set_content($content);
    # try a few different body types
    my $body_type = int(rand(6));
    $msg->set_property("sent", "" . localtime(time));
    $msg->get_instructions->{"fold"} = "yes";
    $msg->get_instructions->{"spindle"} = "no";
    $msg->get_instructions->{"mutilate"} = "no";
    $msg->get_annotations->{"version"} = 1.0;
    $msg->get_annotations->{"pill"} = "RED";

  SWITCH: {
      $body_type == 0 && do { $msg->set_body("It is now " . localtime(time));};
      $body_type == 1 && do { $msg->set_body(rand(65536)); };
      $body_type == 2 && do { $msg->set_body(int(rand(2)), qpid::proton::BOOL); };
      $body_type == 3 && do { $msg->set_body({"foo" => "bar"}); };
      $body_type == 4 && do { $msg->set_body([4, [1, 2, 3.1, 3.4E-5], 8, 15, 16, 23, 42]); };
      $body_type == 5 && do { $msg->set_body(int(rand(65535))); }
    }

    $messenger->put($msg);
    print "Sent: " . $msg->get_body . " [CONTENT TYPE: " . $msg->get_body_type . "]\n";
}

$messenger->send();
$messenger->stop();

die $@ if ($@);
