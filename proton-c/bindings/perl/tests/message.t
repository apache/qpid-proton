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
my $message = qpid::proton::Message->new();
isa_ok($message, 'qpid::proton::Message');

# Verify the message mutators.

# durable
$message->set_durable(1);
ok($message->get_durable(), 'Durable can be set');
$message->set_durable(0);
ok(!$message->get_durable(), 'Durable can be unset');

# priority
my $priority = int(rand(256) + 1);

dies_ok(sub {$message->set_priority('abc')}, 'Priority must be numeric');
dies_ok(sub {$message->set_priority(0 - $priority)}, 'Priority cannot be negative');

$message->set_priority(0);
ok($message->get_priority() == 0, 'Priority can be zero');
$message->set_priority($priority);
ok($message->get_priority() == $priority, 'Priority can be positive');

# Time to live
my $ttl = int(rand(65535) + 1);

dies_ok(sub {$message->set_ttl('def')}, 'TTL must be numeric');
dies_ok(sub {$message->set_ttl(0 - $ttl)}, 'TTL cannot be negative');

$message->set_ttl(0);
ok($message->get_ttl() == 0, 'TTL can be zero');
$message->set_ttl($ttl);
ok($message->get_ttl() == $ttl, 'TTL can be postive');

# first acquirer
$message->set_first_acquirer(1);
ok($message->get_first_acquirer(), 'First acquirer can be set');
$message->set_first_acquirer(0);
ok(!$message->get_first_acquirer(), 'First acquirer can be unset');

# delivery count
my $delivery_count = int(rand(65535) + 1);

dies_ok(sub {$message->set_delivery_count("abc");},
         'Messages cannot have non-numeric delivery counts');
dies_ok(sub {$message->set_delivery_count(0 - $delivery_count)},
         'Messages cannot have negative delivery counts');
$message->set_delivery_count(0);
ok($message->get_delivery_count() == 0, 'Delivery count can be zero');
$message->set_delivery_count($delivery_count);
ok ($message->get_delivery_count() == $delivery_count, 'Delivery count can be postiive');

# message id
my $message_id = random_string(16);

dies_ok (sub {$message->set_id(undef);},
         'Message id cannot be null');
$message->set_id($message_id);
ok($message->get_id(), 'Message id was set');
ok($message->get_id() eq $message_id, 'Message id was set correctly');

# user id
my $user_id = random_string(16);

dies_ok (sub {$message->set_user_id(undef);},
         'User id cannot be null');
$message->set_user_id($user_id);
ok($message->get_user_id(), 'User id was set');
ok($message->get_user_id() eq $user_id, 'User id was set correctly');

# address
my $address = "amqp://0.0.0.0";

$message->set_address(undef);
ok(!$message->get_address(), 'Address can be null');

$message->set_address($address);
ok($message->get_address() eq $address, 'Address is set correctly');

# subject
my $subject = random_string(25);

$message->set_subject(undef);
ok(!$message->get_subject(), 'Subject can be null');

$message->set_subject($subject);
ok($message->get_subject() eq $subject, 'Subject was set correctly');

# reply to
$reply_to = "amqp://0.0.0.0";

$message->set_reply_to(undef);
ok(!$message->get_reply_to(), "Reply to can be null");

$message->set_reply_to($reply_to);
ok($message->get_reply_to() eq $reply_to, 'Reply to was set correctly');

# correlation id
my $correlation_id = random_string(16);

$message->set_correlation_id(undef);
ok(!$message->get_correlation_id(), 'Correlation id can be null');

$message->set_correlation_id($correlation_id);
ok($message->get_correlation_id() eq $correlation_id,
   'Correlation id was set correctly');

# content type
my $content_type = "text/" . random_string(12);

$message->set_content_type(undef);
ok(!$message->get_content_type(), 'Content type can be null');

$message->set_content_type($content_type);
ok($message->get_content_type() eq $content_type,
   'Content type was set correctly');

# content encoding
my $content_encoding = random_string(16);

$message->set_content_encoding(undef);
ok(!$message->get_content_encoding(), 'Content encoding can be null');

$message->set_content_encoding($content_encoding);
ok($message->get_content_encoding() eq $content_encoding,
   'Content encoding was set correctly');

# expiry time
my $expiry_time = random_timestamp();

dies_ok(sub {$message->set_expiry_time(undef);},
        'Expiry cannot be null');

dies_ok(sub {$message->set_expiry_time(0 - $expiry_time);},
        'Expiry cannot be negative');

$message->set_expiry_time(0);
ok($message->get_expiry_time() == 0,
   'Expiry time can be zero');

$message->set_expiry_time($expiry_time);
ok($message->get_expiry_time() == int($expiry_time),
   'Expiry time was set correctly');

# creation time
my $creation_time = random_timestamp();

dies_ok(sub {$message->set_creation_time(undef);},
        'Creation time cannot be null');

dies_ok(sub {$message->set_creation_time(0 - $creation_time);},
        'Creation time cannot be negative');

$message->set_creation_time($creation_time);
ok($message->get_creation_time() == $creation_time,
   'Creation time was set correctly');

# group id
my $group_id = random_string(16);

$message->set_group_id(undef);
ok(!$message->get_group_id(), 'Group id can be null');

$message->set_group_id($group_id);
ok($message->get_group_id() eq $group_id,
   'Group id was set correctly');

# group sequence
my $group_sequence = rand(2**31) + 1;

dies_ok(sub {$message->set_group_sequence(undef);},
        'Sequence id cannot be null');

$message->set_group_sequence(0 - $group_sequence);
ok($message->get_group_sequence() == int(0 - $group_sequence),
   'Group sequence can be negative');

$message->set_group_sequence(0);
ok($message->get_group_sequence() == 0,
   'Group sequence can be zero');

$message->set_group_sequence($group_sequence);
ok($message->get_group_sequence() == int($group_sequence),
   'Group sequence can be positive');

# reply to group id
my $reply_to_group_id = random_string(16);

$message->set_reply_to_group_id(undef);
ok(!$message->get_reply_to_group_id(), 'Reply-to group id can be null');

$message->set_reply_to_group_id($reply_to_group_id);
ok($message->get_reply_to_group_id() eq $reply_to_group_id,
   'Reply-to group id was set correctly');

# format
my @formats = ($qpid::proton::Message::DATA_FORMAT,
               $qpid::proton::Message::TEXT_FORMAT,
               $qpid::proton::Message::AMQP_FORMAT,
               $qpid::proton::Message::JSON_FORMAT);

dies_ok(sub {$message->set_format(undef);}, 'Format cannot be null');

foreach (@formats)
{
    my $format = $_;

    $message->set_format($format);
    ok($message->get_format() == $format,
       'Format was set correctly');
}

# reset the format
$message->set_format($qpid::proton::Message::TEXT_FORMAT);

# content
my $content_size = rand(512);
my $content = random_string($content_size);

$message->set_content(undef);
ok(!$message->get_content(), 'Content can be null');

$message->set_content($content);
ok($message->get_content() eq $content,
   'Content was saved correctly');

