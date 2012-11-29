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

sub usage {
    exit(0);
}

my $address = "0.0.0.0";

my %options = ();
getopts("ha:", \%options) or usage();
usage if $options{h};

$address = $options{a} if defined $options{a};

my $msg  = new qpid::proton::Message();
my $messenger = new qpid::proton::Messenger();

$messenger->start();

my @messages = @ARGV;
@messages = ("This is a test. " . localtime(time)) unless $messages[0];

foreach (@messages)
{
    $msg->set_address($address);
    $msg->set_content($_);
    $messenger->put($msg);
}

$messenger->send();
$messenger->stop();

die $@ if ($@);
