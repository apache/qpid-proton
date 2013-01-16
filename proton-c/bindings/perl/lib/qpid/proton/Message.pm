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

our $DATA_FORMAT = $cproton_perl::PN_DATA;
our $TEXT_FORMAT = $cproton_perl::PN_TEXT;
our $AMQP_FORMAT = $cproton_perl::PN_AMQP;
our $JSON_FORMAT = $cproton_perl::PN_JSON;

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
    my $impl = $self->{_impl};

    cproton_perl::pn_message_free($impl);
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
    return cproton_perl::pn_message_is_durable($self->{_impl});
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
    return cproton_perl::pn_message_is_first_acquirer($self->{_impl});
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
    my $id = $_[1];

    die "Message id must be defined" if !defined($id);

    cproton_perl::pn_message_set_id($self->{_impl}, $id);
}

sub get_id {
    my ($self) = @_;
    my $id = cproton_perl::pn_message_get_id($self->{_impl});

    return $id;
}

sub set_user_id {
    my ($self) = @_;
    my $user_id = $_[1];

    die "User id must be defined" if !defined($user_id);

    cproton_perl::pn_message_set_user_id($self->{_impl}, $user_id);
}

sub get_user_id {
    my ($self) = @_;
    my $user_id = cproton_perl::pn_message_get_user_id($self->{_impl});

    return $user_id;
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
    my $format = $_[1];

    die "Format must be defined" if !defined($format);

    cproton_perl::pn_message_set_format($self->{_impl}, $format);
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
    my $content = $_[1];

    cproton_perl::pn_message_load($self->{_impl}, $content);
}

sub get_content {
    my ($self) = @_;
    my $content = cproton_perl::pn_message_save($self->{_impl}, 1024);

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

sub set_expiry_time {
    my ($self) = @_;
    my $expiry_time = $_[1];

    die "Expiry time must be defined" if !defined($expiry_time);

    $expiry_time = int($expiry_time);

    die "Expiry time must be non-negative" if $expiry_time < 0;

    cproton_perl::pn_message_set_expiry_time($self->{_impl}, $expiry_time);
}

sub get_expiry_time {
    my ($self) = @_;
    return cproton_perl::pn_message_get_expiry_time($self->{_impl});
}

sub set_creation_time {
    my ($self) = @_;
    my $creation_time = $_[1];

    die "Creation time must be defined" if !defined($creation_time);

    $creation_time = int($creation_time);

    die "Creation time must be non-negative" if $creation_time < 0;

    cproton_perl::pn_message_set_creation_time($self->{_impl}, $creation_time);
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
    my $group_sequence = $_[1];

    die "Group sequence must be defined" if !defined($group_sequence);

    cproton_perl::pn_message_set_group_sequence($self->{_impl}, int($_[1]));
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

1;

