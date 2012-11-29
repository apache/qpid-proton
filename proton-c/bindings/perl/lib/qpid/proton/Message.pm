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

package qpid::proton::Message;

sub new {
    my ($class) = @_;
    my ($self) = {};

    my $impl = cproton_perl::pn_message();
    $self->{_impl} = $impl;

    bless $self, $class;
    return $self;
}

sub DESTROY {
    my ($self) = @_;
    cproton_perl::pn_message_free($self->{_impl});
}

sub get_impl {
    my ($self) = @_;
    return $self->{_impl};
}

sub clear {
    my ($self) = @_;
    cproton__perl::pn_message_clear($self->{_impl});
}

sub errno {
    my ($self) = @_;
    return cproton_perl::pn_message_errno($self->{_impl});
}

sub error {
    my ($self) = @_;
    return cproton_perl::pn_message_error($self->{_impl});
}

sub set_durable {
    my ($self) = @_;
    cproton_perl::pn_message_set_durable($self->{_impl}, $_[1]);
}

sub get_durable {
    my ($self) = @_;
    return cproton_perl::pn_message_get_durable($self->{_impl});
}

sub set_priority {
    my ($self) = @_;
    cproton_perl::pn_message_set_priority($self->{_impl}, $_[1]);
}

sub get_priority {
    my ($self) = @_;
    return cproton_perl::pn_message_get_priority($self->{_impl});
}

sub set_ttl {
    my ($self) = @_;
    cproton_perl::pn_message_set_ttl($self->{_impl}, $_[1]);
}

sub get_ttl {
    my ($self) = @_;
    return cproton_perl::pn_message_get_ttl($self->{_impl});
}

sub set_first_acquirer {
    my ($self) = @_;
    cproton_perl::pn_message_set_first_acquirer($self->{_impl}, $_[1]);
}

sub get_first_acquirer {
    my ($self) = @_;
    return cproton_perl::pn_message_get_first_acquirer($self->{_impl});
}

sub set_delivery_count {
    my ($self) = @_;
    cproton_perl::pn_message_set_delivery_count($self->{_impl}, $_[1]);
}

sub get_delivery_count {
    my ($self) = @_;
    return cproton_perl::pn_message_get_delivery_count($self->{_impl});
}

sub set_id {
    my ($self) = @_;
    cproton_perl::pn_message_set_id($self->{_impl}, $_[1]);
}

sub get_id {
    my ($self) = @_;
    return cproton_perl::pn_message_get_id($self->{_impl});
}

sub set_user_id {
    my ($self) = @_;
    cproton_perl::pn_message_set_user_id($self->{_impl}, $_[1]);
}

sub get_user_id {
    my ($self) = @_;
    return cproton_perl::pn_message_get_user_id($self->{_impl}, $_[1]);
}

sub set_address {
    my ($self) = @_;
    cproton_perl::pn_message_set_address($self->{_impl}, $_[1]);
}

sub get_address {
    my ($self) = @_;
    return cproton_perl::pn_message_get_address($self->{_impl});
}

sub set_subject {
    my ($self) = @_;
    cproton_perl::pn_message_set_subject($self->{_impl}, $_[1]);
}

sub get_subject {
    my ($self) = @_;
    return cproton_perl::pn_message_get_subject($self->{_impl});
}

sub set_reply_to {
    my ($self) = @_;
    cproton_perl::pn_message_set_reply_to($self->{_impl}, $_[1]);
}

sub get_reply_to {
    my ($self) = @_;
    return cproton_perl::pn_message_get_reply_to($self->{_impl});
}

sub set_correlation_id {
    my ($self) = @_;
    cproton_perl::pn_message_set_correlation_id($self->{_impl}, $_[1]);
}

sub get_correlation_id {
    my ($self) = @_;
    return cproton_perl::pn_message_get_correlation_id($self->{_impl});
}

sub set_format {
    my ($self) = @_;
    cproton_perl::pn_message_set_format($self->{_impl}, $_[1]);
}

sub get_format {
    my ($self) = @_;
    return cproton_perl::pn_message_get_format($self->{_impl});
}

sub set_content_type {
    my ($self) = @_;
    cproton_perl::pn_message_set_content_type($self->{_impl}, $_[1]);
}

sub get_content_type {
    my ($self) = @_;
    return cproton_perl::pn_message_get_content_type($self->{_impl});
}

sub set_content {
    my ($self) = @_;
    my ($content) = $_[1];
    cproton_perl::pn_message_load($self->{_impl}, $content);
}

sub get_content {
    my ($self) = @_;
    return cproton_perl::pn_message_save($self->{_impl}, 1024);
}

sub set_content_encoding {
    my ($self) = @_;
    cproton_perl::pn_message_set_content_encoding($self->{_impl}, $_[1]);
}

sub get_content_encoding {
    my ($self) = @_;
    return cproton_perl::pn_message_get_content_encoding($self->{_impl});
}

sub set_expires {
    my ($self) = @_;
    cproton_perl::pn_message_set_expires($self->{_impl}, $_[1]);
}

sub get_expires {
    my ($self) = @_;
    return cproton_perl::pn_message_get_expires($self->{_impl});
}

sub set_creation_time {
    my ($self) = @_;
    cproton_perl::pn_message_set_creation_time($self->{_impl}, $_[1]);
}

sub get_creation_time {
    my ($self) = @_;
    return cproton_perl::pn_message_get_creation_time($self->{_impl});
}

sub set_group_id {
    my ($self) = @_;
    cproton_perl::pn_message_set_group_id($self->{_impl}, $_[1]);
}

sub get_group_id {
    my ($self) = @_;
    return cproton_perl::pn_message_get_group_id($self->{_impl});
}

sub set_group_sequence {
    my ($self) = @_;
    cproton_perl::pn_message_set_group_sequence($self->{_impl}, $_[1]);
}

sub get_group_sequence {
    my ($self) = @_;
    return cproton_perl::pn_message_get_group_sequence($self->{_impl});
}

sub set_reply_to_group_id {
    my ($self) = @_;
    cproton_perl::pn_message_set_reply_to_group_id($self->{_impl}, $_[1]);
}

sub get_reply_to_group_id {
    my ($self) = @_;
    return cproton_perl::pn_message_get_reply_to_group_id($self->{_impl});
}



package qpid::proton::Messenger;

sub new {
    my ($class) = @_;
    my ($self) = {};

    my $impl = cproton_perl::pn_messenger($_[1]);
    $self->{_impl} = $impl;

    bless $self, $class;
    return $self;
}

sub DESTROY {
    my ($self) = @_;
    cproton_perl::pn_messenger_stop($self->{_impl});
    cproton_perl::pn_messenger_free($self->{_impl});
}

sub get_name {
    my ($self) = @_;
    return cproton_perl::pn_messenger_get_name($self->{_impl});
}

sub set_timeout {
    my ($self) = @_;
    cproton_perl::pn_messenger_set_timeout($self->{_impl}, $_[1]);
}

sub get_timeout {
    my ($self) = @_;
    return cproton_perl::pn_messenger_get_timeout($self->{_impl});
}

sub get_error {
    my ($self) = @_;
    return cproton_perl::pn_messenger_error($self->{_impl});
}

sub get_errno {
    my ($self) = @_;
    return cproton_perl::pn_messenger_errno($self->{_impl});
}

sub start {
    my ($self) = @_;
    cproton_perl::pn_messenger_start($self->{_impl});
}

sub stop {
    my ($self) = @_;
    cproton_perl::pn_messenger_stop($self->{_impl});
}

sub subscribe {
    my ($self) = @_;
    cproton_perl::pn_messenger_subscribe($self->{_impl}, $_[1]);
}

sub set_certificate {
    my ($self) = @_;
    cproton_perl::pn_messenger_set_certificate($self->{_impl}, $_[1]);
}

sub get_certificate {
    my ($self) = @_;
    return cproton_perl::pn_messenger_get_certificate($self->{_impl});
}

sub set_private_key {
    my ($self) = @_;
    cproton_perl::pn_messenger_set_private_key($self->{_impl}, $_[1]);
}

sub get_private_key {
    my ($self) = @_;
    return cproton_perl::pn_messenger_get_private_key($self->{_impl});
}

sub set_trusted_certificates {
    my ($self) = @_;
    cproton_perl::pn_messenger_set_trusted_certificates($self->{_impl}, $_[1]);
}

sub get_trusted_certificates {
    my ($self) = @_;
    return cproton_perl::pn_messenger_get_trusted_certificates($self->{_impl});
}

sub put {
    my ($self) = @_;
    my $message = $_[1];
    cproton_perl::pn_messenger_put($self->{_impl}, $message->get_impl);
}

sub send {
    my ($self) = @_;
    cproton_perl::pn_messenger_send($self->{_impl});
}

sub get {
    my ($self) = @_;

    my $message = $_[1] || new proton::Message();
    cproton_perl::pn_messenger_get($self->{_impl}, $message->get_impl());
    return $message;
}

sub receive {
    my ($self) = @_;
    cproton_perl::pn_messenger_recv($self->{_impl}, $_[1]);
}

sub outgoing {
    my ($self) = @_;
    return cproton_perl::pn_messenger_outgoing($self->{_impl});
}

sub incoming {
    my ($self) = @_;
    return cproton_perl::pn_messenger_incoming($self->{_impl});
}

1;

