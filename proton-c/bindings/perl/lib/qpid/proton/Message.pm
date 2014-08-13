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
    $self->{_properties} = {};
    $self->{_instructions} = {};
    $self->{_annotations} = {};
    $self->{_body} = undef;
    $self->{_body_type} = undef;

    bless $self, $class;
    return $self;
}

use overload fallback => 1,
    '""' => sub {
        my ($self) = @_;
        my $tmp = cproton_perl::pn_string("");
        cproton_perl::pn_inspect($self->{_impl}, $tmp);
        my $result = cproton_perl::pn_string_get($tmp);
        cproton_perl::pn_free($tmp);
        return $result;
};

sub DESTROY {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_message_free($impl);
}

sub get_impl {
    my ($self) = @_;
    my $impl = $self->{_impl};
    return $impl;
}

sub clear {
    my ($self) = @_;
    cproton__perl::pn_message_clear($self->{_impl});
    $self->{_body} = undef;
    $self->{_properties} = {};
    $self->{_instructions} = {};
    $self->{_annotations} = {};
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

=pod

=head2 PROPERTIES

Allows for accessing and updating the set of properties associated with the
message.

=over

=item my $props = $msg->get_properties;

=item $msg->set_properties( [VAL] );

=item my $value = $msg->get_property( [KEY] );

=item $msg->set_propert( [KEY], [VALUE] );

=back

=cut

sub get_properties {
    my ($self) = @_;

    return $self->{_properties};
}

sub set_properties {
    my ($self) = @_;
    my ($properties) = $_[1];

    $self->{_properties} = $properties;
}

sub get_property {
    my ($self) = @_;
    my $name = $_[1];
    my $properties = $self->{_properties};

    return $properties{$name};
}

sub set_property {
    my ($self) = @_;
    my $name = $_[1];
    my $value = $_[2];
    my $properties = $self->{_properties};

    $properties->{"$name"} = $value;
}

=pod

=head2 ANNOTATIONS

Allows for accessing and updatin ghte set of annotations associated with the
message.

=over

=item my $annotations = $msg->get_annotations;

=item $msg->get_annotations->{ [KEY] } = [VALUE];

=item $msg->set_annotations( [VALUE ]);

=back

=cut

sub get_annotations {
    my ($self) = @_;
    return $self->{_annotations};
}

sub set_annotations {
    my ($self) = @_;
    my $annotations = $_[1];

    $self->{_annotations} = $annotations;
}

=pod

=cut

sub get_instructions {
    my ($self) = @_;
    return $self->{_instructions};
}

sub set_instructions {
    my ($self) = @_;
    my $instructions = $_[1];

    $self->{_instructions} = $instructions;
}

=pod

=head2 BODY

The body of the message. When setting the body value a type must be specified,
such as I<qpid::proton::INT>. If unspecified, the body type will default to
B<qpid::proton::STRING>.

=over

=item $msg->set_body( [VALUE], [TYPE] );

=item $msg->get_body();

=item $msg->get_body_type();

=back

=cut

sub set_body {
    my ($self) = @_;
    my $body = $_[1];
    my $body_type = $_[2] || undef;

    # if no body type was defined, then attempt to infer what it should
    # be, which is going to be a best guess
    if (!defined($body_type)) {
        if (qpid::proton::is_num($body)) {
            if (qpid::proton::is_float($body)) {
                $body_type = qpid::proton::FLOAT;
            } else {
                $body_type = qpid::proton::INT;
            }
        } elsif (!defined($body)) {
            $body_type =  qpid::proton::NULL;
        } elsif ($body eq '') {
            $body_type =  qpid::proton::STRING;
        } elsif (ref($body) eq 'HASH') {
            $body_type =  qpid::proton::MAP;
        } elsif (ref($body) eq 'ARRAY') {
            $body_type =  qpid::proton::LIST;
        } else {
            $body_type =  qpid::proton::STRING;
        }
    }

    $self->{_body} = $body;
    $self->{_body_type} = $body_type;
}

sub get_body {
    my ($self) = @_;
    my $body = $self->{_body};

    return $body;
}

sub get_body_type {
    my ($self) = @_;

    return $self->{_body_type};
}

sub preencode() {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $my_body = $self->{_body};
    my $body_type = $self->{_body_type};
    my $body = new qpid::proton::Data(cproton_perl::pn_message_body($impl));

    $body->clear();
    $body_type->put($body, $my_body) if(defined($my_body) && $body_type);

    my $my_props = $self->{_properties};
    my $props = new qpid::proton::Data(cproton_perl::pn_message_properties($impl));
    $props->clear();
    qpid::proton::MAP->put($props, $my_props) if $my_props;

    my $my_insts = $self->{_instructions};
    my $insts = new qpid::proton::Data(cproton_perl::pn_message_instructions($impl));
    $insts->clear;
    qpid::proton::MAP->put($insts, $my_insts) if $my_insts;

    my $my_annots = $self->{_annotations};
    my $annotations = new qpid::proton::Data(cproton_perl::pn_message_annotations($impl));
    $annotations->clear();
    qpid::proton::MAP->put($annotations, $my_annots);
}

sub postdecode() {
    my ($self) = @_;
    my $impl = $self->{_impl};

    $self->{_body} = undef;
    $self->{_body_type} = undef;
    my $body = new qpid::proton::Data(cproton_perl::pn_message_body($impl));
    if ($body->next()) {
        $self->{_body_type} = $body->get_type();
        $self->{_body} = $body->get_type()->get($body);
    }

    my $props = new qpid::proton::Data(cproton_perl::pn_message_properties($impl));
    $props->rewind;
    if ($props->next) {
        my $properties = $props->get_type->get($props);
        $self->{_properties} = $props->get_type->get($props);
    }

    my $insts = new qpid::proton::Data(cproton_perl::pn_message_instructions($impl));
    $insts->rewind;
    if ($insts->next) {
        $self->{_instructions} = $insts->get_type->get($insts);
    }

    my $annotations = new qpid::proton::Data(cproton_perl::pn_message_annotations($impl));
    $annotations->rewind;
    if ($annotations->next) {
        my $annots = $annotations->get_type->get($annotations);
        $self->{_annotations} = $annots;
    } else {
        $self->{_annotations} = {};
    }
}

1;

