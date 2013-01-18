#!/usr/bin/env perl -w
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

use Test::More qw(no_plan);
use Test::Exception;

require 'utils.pm';

BEGIN {use_ok('qpid_proton');}
require_ok('qpid_proton');

# Create a new message.
my $messenger = qpid::proton::Messenger->new();
isa_ok($messenger, 'qpid::proton::Messenger');

# name
ok($messenger->get_name(), 'Messenger has a default name');

{
    my $name = random_string(16);
    my $messenger1 = qpid::proton::Messenger->new($name);

    ok($messenger1->get_name() eq $name, 'Messenger saves name correctly');
}

# certificate
my $certificate = random_string(255);

$messenger->set_certificate(undef);
ok(!$messenger->get_certificate(), 'Certificate can be null');

$messenger->set_certificate($certificate);
ok($messenger->get_certificate() eq $certificate,
   'Certificate was set correctly');

# private key
my $key = random_string(255);

$messenger->set_private_key(undef);
ok(!$messenger->get_private_key(), 'Private key can be null');

$messenger->set_private_key($key);
ok($messenger->get_private_key() eq $key, 'Private key was set correctly');

# password
my $password = random_string(64);

$messenger->set_password(undef);
ok(!$messenger->get_password(), 'Password can be null');

$messenger->set_password($password);
ok($messenger->get_password() eq $password, 'Password set correctly');

# trusted certificates
my $trusted_certificate = random_string(255);

$messenger->set_trusted_certificates(undef);
ok(!$messenger->get_trusted_certificates(), 'Trusted certificates can be null');

$messenger->set_trusted_certificates($trusted_certificate);
ok($messenger->get_trusted_certificates() eq $trusted_certificate,
   'Trusted certificates was set correctly');

# timeout
my $timeout = rand(2**31) + 1;

$messenger->set_timeout(undef);
ok($messenger->get_timeout() == 0, 'Null timeout is treated as 0');

$messenger->set_timeout(0 - $timeout);
ok($messenger->get_timeout() == int(0 - $timeout), 'Timeout can be negative');

$messenger->set_timeout(0);
ok($messenger->get_timeout() == 0, 'Timeout can be zero');

$messenger->set_timeout($timeout);
ok($messenger->get_timeout() == int($timeout), 'Timeout can be positive');

# accept mode
my @accept_modes = ($qpid::proton::Messenger::AUTO_ACCEPT,
                    $qpid::proton::Messenger::MANUAL_ACCEPT);

dies_ok(sub {$messenger->set_accept_mode(undef);},
        'Accept mode cannot be null');
dies_ok(sub {$messenger->set_accept_mode($messenger);},
        'Accept mode rejects an arbitrary value');

foreach (@accept_modes)
{
    my $mode = $_;

    $messenger->set_accept_mode($mode);
    ok($messenger->get_accept_mode() eq $mode,
       'Accept mode was set correctly');
}

# outgoing window
my $outgoing_window = rand(2**9);

$messenger->set_outgoing_window(undef);
ok($messenger->get_outgoing_window() == 0, 'Null outgoing window is treated as zero');

$messenger->set_outgoing_window(0);
ok($messenger->get_outgoing_window() == 0, 'Outgoing window can be zero');

$messenger->set_outgoing_window(0 - $outgoing_window);
ok($messenger->get_outgoing_window() == int(0 - $outgoing_window),
   'Outgoing window can be negative');

$messenger->set_outgoing_window($outgoing_window);
ok($messenger->get_outgoing_window() == int($outgoing_window),
   'Outgoing window can be positive');

# incoming window
my $incoming_window = rand(2**9);

$messenger->set_incoming_window(undef);
ok($messenger->get_incoming_window() == 0, 'Null incoming window is treated as zero');

$messenger->set_incoming_window(0);
ok($messenger->get_incoming_window() == 0, 'Incoming window can be zero');

$messenger->set_incoming_window(0 - $incoming_window);
ok($messenger->get_incoming_window() == int(0 - $incoming_window),
   'Incoming window can be negative');

$messenger->set_incoming_window($incoming_window);
ok($messenger->get_incoming_window() == int($incoming_window),
   'Incoming window can be positive');

