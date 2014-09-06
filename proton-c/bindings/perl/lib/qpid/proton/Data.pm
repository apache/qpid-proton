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

use Scalar::Util qw(reftype looks_like_number);

=pod

=head1 NAME

qpid::proton::Data

=head1 DESCRIPTION

The B<Data> class provides an interface for decoding, extract, creating and
encoding arbitrary AMQP data. A B<Data> object contains a tree of AMQP values.
Leaf nodes in this tree correspond to scalars in the AMQP type system such as
B<INT> or B<STRING>. Integerior nodes in this tree correspond to compound values
in the AMQP type system such as B<LIST>, B<MAP>, B<ARRAY> or B<DESCRIBED>. The
root node of the tree is the B<Data> object itself and can have an arbitrary
number of children.

A B<Data> object maintains the notion of the current sibling node and a current
parent node. Siblings are ordered within their parent. Values are accessed
and/or added by using the B<next>, B<prev>, B<enter> and B<exit> methods to
navigate to the desired location in the tree and using the supplied variety of
mutator and accessor methods to access or add a value of the desired type.

The mutator methods will always add a vlaue I<after> the current node in the
tree. If the current node has a next sibling the mutaor method will overwrite
the value on this node. If there is no current node or the current node has no
next sibling then one will be added. The accessor methods always set the
add/modified node to the current node. The accessor methods read the value of
the current node and do not change which node is current.

=cut

package qpid::proton::Data;

=pod

=head1 CONSTRUCTOR

Creates a new instance with the specified capacity.

=over

=item my $data = qpid::proton::Data->new( CAPACITY );

=back

=cut

sub new {
    my ($class) = @_;
    my ($self) = {};
    my $capacity = $_[1] || 16;
    my $impl = $capacity;
    $self->{_free} = 0;

    if($capacity) {
        if (::looks_like_number($capacity)) {
            $impl = cproton_perl::pn_data($capacity);
            $self->{_free} = 1;
        }
    }

    $self->{_impl} = $impl;

    bless $self, $class;
    return $self;
}

sub DESTROY {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_free($impl) if $self->{_free};
}

=pod

=head1 ACTIONS

Clear all content for the data object.

=over

=item my $data->clear();

=back

=cut

sub clear {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_clear($impl);
}


=pod

=head1 NAVIGATION

The following methods allow for navigating through the nodes in the tree.

=cut


=pod

=over

=item $doc->enter;

=item if ($doc->enter()) { do_something_with_children; }

Sets the parent node to the current node and clears the current node.

Clearing the current node sets it I<before> the first child.

=item $doc->exit;

=item if ($doc->exit()) { do_something_with_parent; }

Sets the current node to the parent node, and the parent node to its own parent.

=item $doc->next;

=item $doc->prev;

Moves to the next/previous sibling and returns its type. If there is no next or
previous sibling then the current node remains unchanged.

=item $doc->rewind;

Clears the current node and sets the parent to the root node.

=back

=cut

sub enter {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_enter($impl);
}

sub exit {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_exit($impl);
}

sub rewind {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_rewind($impl);
}


=pod

=over

=item $doc->next;

=item if ($doc->next()) { do_something; }

Advances the current node to its next sibling and returns its type.

If there is no next sibling then the current node remains unchanged and
B<undef> is returned.

=item $doc->prev;

=item if ($doc->prev()) { do_something; }

Advances the current node to its previous sibling and returns its type.

If there is no previous sibling then the current node remains unchanged and
undef is returned.

=back

=cut

sub next {
    my ($self) = @_;
    my $impl = $self->{_impl};

    return cproton_perl::pn_data_next($impl);
}

sub prev {
    my ($self) = @_;
    my $impl = $self->{_impl};

    return cproton_perl::pn_data_prev($impl);
}


=pod

=head1 SUPPORTED TYPES

The following methods allow for inserting the various node types into the
tree.

=head2 NODE TYPE

You can retrieve the type of the current node.

=over

=item $type = $doc->get_type;

=back

=cut


sub get_type {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $type = cproton_perl::pn_data_type($impl);

    return qpid::proton::Mapping->find_by_type_value($type);
}


=pod

=head2 SCALAR TYPES

=cut

=pod

=head3 NULL

=over

=item $doc->put_null;

Inserts a null node.

=item $doc->is_null;

Returns true if the current node is null.

=back

=cut

sub put_null() {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_put_null($impl);
}

sub is_null {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_is_null($impl);
}

sub check {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $err = $_[1];

    # if we got a null then just exit
    return $err if !defined($err);

    if($err < 0) {
        die DataException->new("[$err]: " . cproton_perl::pn_data_error($impl));
    } else {
        return $err;
    }
}

=pod

=head3 BOOL

Handles a boolean (B<true>/B<false>) node.

=over

=item $doc->put_bool( VALUE );

=item $doc->get_bool;

=back

=cut

sub put_bool {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1] || 0;

    cproton_perl::pn_data_put_bool($impl, $value);
}

sub get_bool {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_bool($impl);
}

=pod

=head3 UBYTE

Handles an unsigned byte node.

=over

=item $data->put_ubyte( VALUE );

=item $data->get_ubyte;

=back

=cut

sub put_ubyte {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value  = $_[1];

    die "ubyte must be defined" if !defined($value);
    die "ubyte must be non-negative" if $value < 0;

    check(cproton_perl::pn_data_put_ubyte($impl, int($value)));
}

sub get_ubyte {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_ubyte($impl);
}

=pod

=head3 BYTE

Handles a signed byte node.

=over

=item $data->put_byte( VALUE );

=item $data->get_byte;

=back

=cut

sub put_byte {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "byte must be defined" if !defined($value);

    check(cproton_perl::pn_data_put_byte($impl, int($value)));
}

sub get_byte {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_byte($impl);
}

=pod

=head3 USHORT

Handles an unsigned short node.

=over

=item $data->put_ushort( VALUE );

=item $data->get_ushort;

=back

=cut

sub put_ushort {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "ushort must be defined" if !defined($value);

    check(cproton_perl::pn_data_put_ushort($impl, int($value)));
}

sub get_ushort {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_ushort($impl);
}

=pod

=head3 SHORT

Handles a signed short node.

=over

=item $data->put_short( VALUE );

=item $data->get_short;

=back

=cut

sub put_short {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "short must be defined" if !defined($value);

    check(cproton_perl::pn_data_put_short($impl, int($value)));
}

sub get_short {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_short($impl);
}

=pod

=head3 UINT

Handles an unsigned integer node.

=over

=item $data->put_uint( VALUE );

=item $data->get_uint;

=back

=cut

sub put_uint {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "uint must be defined" if !defined($value);

    check(cproton_perl::pn_data_put_uint($impl, int($value)));
}

sub get_uint {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_uint($impl);
}

=pod

=head3 INT

Handles an integer node.

=over

=item $data->put_int( VALUE );

=item $data->get_int;

=back

=cut

sub put_int {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "int must be defined" if !defined($value);

    check(cproton_perl::pn_data_put_int($impl, int($value)));
}

sub get_int {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_int($impl);
}

=pod

=head3 CHAR

Handles a character node.

=over

=item $data->put_char( VALUE );

=item $data->get_char;

=back

=cut

sub put_char {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "char must be defined" if !defined($value);

    check(cproton_perl::pn_data_put_char($impl, int($value)));
}

sub get_char {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_char($impl);
}

=pod

=head3 ULONG

Handles an unsigned long node.

=over

=item $data->set_ulong( VALUE );

=item $data->get_ulong;

=back

=cut

sub put_ulong {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "ulong must be defined" if !defined($value);

    check(cproton_perl::pn_data_put_ulong($impl, $value));
}

sub get_ulong {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_ulong($impl);
}

=pod

=head3 LONG

Handles a signed long node.

=over

=item $data->put_long( VALUE );

=item $data->get_long;

=back

=cut

sub put_long {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "long must be defined" if !defined($value);

    cproton_perl::pn_data_put_long($impl, int($value));
}

sub get_long {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_long($impl);
}

=pod

=head3 TIMESTAMP

Handles a timestamp node.

=over

=item $data->put_timestamp( VALUE );

=item $data->get_timestamp;

=back

=cut

sub put_timestamp {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "timestamp must be defined" if !defined($value);

    check(cproton_perl::pn_data_put_timestamp($impl, int($value)));
}

sub get_timestamp {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_timestamp($impl);
}

=pod

=head3 FLOAT

Handles a floating point node.

=over

=item $data->put_float( VALUE );

=item $data->get_float;

=back

=cut

sub put_float {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "float must be defined" if !defined($value);

    check(cproton_perl::pn_data_put_float($impl, $value));
}

sub get_float {
    my ($self) = @_;
    my $impl = $self->{_impl};

    my $value = cproton_perl::pn_data_get_float($impl);

    cproton_perl::pn_data_get_float($impl);
}

=pod

=head3 DOUBLE

Handles a double node.

=over

=item $data->put_double( VALUE );

=item $data->get_double;

=back

=cut

sub put_double {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "double must be defined" if !defined($value);

    check(cproton_perl::pn_data_put_double($impl, $value));
}

sub get_double {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_double($impl);
}

=pod

=head3 DECIMAL32

Handles a decimal32 node.

=over

=item $data->put_decimal32( VALUE );

=item $data->get_decimal32;

=back

=cut

sub put_decimal32 {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "decimal32 must be defined" if !defined($value);

    check(cproton_perl::pn_data_put_decimal32($impl, $value));
}

sub get_decimal32 {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_decimal32($impl);
}

=pod

=head3 DECIMAL64

Handles a decimal64 node.

=over

=item $data->put_decimal64( VALUE );

=item $data->get_decimal64;

=back

=cut

sub put_decimal64 {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "decimal64 must be defined" if !defined($value);

    check(cproton_perl::pn_data_put_decimal64($impl, $value));
}

sub get_decimal64 {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_decimal64($impl);
}

=pod

=head3 DECIMAL128

Handles a decimal128 node.

=over

=item $data->put_decimal128( VALUE );

=item $data->get_decimal128;

=back

=cut

sub put_decimal128 {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "decimal128 must be defined" if !defined($value);

    my @binary = split //, pack("H[32]", sprintf("%032x", $value));
    my @bytes = ();

    foreach $char (@binary) {
        push(@bytes, ord($char));
    }
    check(cproton_perl::pn_data_put_decimal128($impl, \@bytes));
}

sub get_decimal128 {
    my ($self) = @_;
    my $impl = $self->{_impl};

    my $bytes = cproton_perl::pn_data_get_decimal128($impl);
    my $value = hex(unpack("H[32]", $bytes));

    return $value;
}

=pod

=head3 UUID

Handles setting a UUID value. UUID values can be set using a 128-bit integer
value or else a well-formed string.

=over

=item $data->put_uuid( VALUE );

=item $data->get_uuid;

=back

=cut

use Data::Dumper;

sub put_uuid {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "uuid must be defined" if !defined($value);

    if($value =~ /[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}/) {
        $value =~ s/-//g;
        my @binary = split //, pack("H[32]", $value);
        my @bytes = ();

        foreach $char (@binary) {
            push(@bytes, ord($char));
         }

        check(cproton_perl::pn_data_put_uuid($impl, \@bytes));
    } else {
        die "uuid is malformed: $value";
    }
}

sub get_uuid {
    my ($self) = @_;
    my $impl = $self->{_impl};

    my $bytes = cproton_perl::pn_data_get_uuid($impl);

    my $value = unpack("H[32]", $bytes);
    $value = substr($value, 0, 8) . "-" .
        substr($value, 8, 4) . "-" .
        substr($value, 12, 4) . "-" .
        substr($value, 16, 4) . "-" .
        substr($value, 20);

    return $value;
}

=pod

=head3 BINARY

Handles a binary data node.

=over

=item $data->put_binary( VALUE );

=item $data->get_binary;

=back

=cut

sub put_binary {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "binary must be defined" if !defined($value);

    check(cproton_perl::pn_data_put_binary($impl, $value)) if defined($value);
}

sub get_binary {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_binary($impl);
}

=pod

=head3 STRING

Handles a string node.

=over

=item $data->put_string( VALUE );

=item $data->get_string;

=back

=cut

sub put_string {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "string must be defined" if !defined($value);

    check(cproton_perl::pn_data_put_string($impl, $value));
}

sub get_string {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_string($impl);
}

=pod

=head3 SYMBOL

Handles a symbol value.

=over

=item $data->put_symbol( VALUE );

=item $data->get_symbol;

=back

=cut

sub put_symbol {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $value = $_[1];

    die "symbol must be defined" if !defined($value);

    check(cproton_perl::pn_data_put_symbol($impl, $value));
}

sub get_symbol {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_symbol($impl);
}

=pod

=head3 DESCRIBED VALUE

A described node has two children: the descriptor and the value.

These are specified by entering the node and putting the
described values.

=over

=item $data->put_described;

=item $data->is_described;

=back

=cut

sub put_described {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_put_described($impl);
}

sub is_described {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_is_described($impl);
}

=pod

=head3 ARRAYS

Puts an array value.

Elements may be filled by entering the array node and putting the element values.
The values must all be of the specified array element type.

If an array is B<described> then the first child value of the array is the
descriptor and may be of any type.

=over

B<DESCRIBED> specifies whether the array is described or not.

B<TYPE> specifies the type of elements in the array.

=back

=over

=item $data->put_array( DESCRIBED, TYPE )

=item my ($count, $described, $array_type) = item $data->get_array

=back

=cut

sub put_array {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $described = $_[1] || 0;
    my $array_type = $_[2];

    die "array type must be defined" if !defined($array_type);

    check(cproton_perl::pn_data_put_array($impl,
                                          $described,
                                          $array_type->get_type_value));
}

sub get_array {
    my ($self) = @_;
    my $impl = $self->{_impl};

    my $count = cproton_perl::pn_data_get_array($impl);
    my $described = cproton_perl::pn_data_is_array_described($impl);
    my $type_value = cproton_perl::pn_data_get_array_type($impl);

    $type_value = qpid::proton::Mapping->find_by_type_value($type_value);

    return ($count, $described, $type_value);
}

sub get_array_type {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_array_type($impl);
}

=pod

=head3 LIST

Puts a list value.

Elements may be filled in by entering the list and putting element values.

=over

=item $data->put_list;

=item my $count = $data->get_list

=back

=cut

sub put_list {
    my ($self) = @_;
    my $impl = $self->{_impl};

    check(cproton_perl::pn_data_put_list($impl));
}

sub get_list {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_list($impl);
}

=pod

head3 MAP

Puts a map value.

Elements may be filled by entering the map node and putting alternating
key/value pairs.

=over

=item $data->put_map;

=item my $count = $data->get_map;

=back

=cut

sub put_map {
    my ($self) = @_;
    my $impl = $self->{_impl};

    check(cproton_perl::pn_data_put_map($impl));
}

sub get_map {
    my ($self) = @_;
    my $impl = $self->{_impl};

    cproton_perl::pn_data_get_map($impl);
}

sub put_list_helper {
    my ($self) = @_;
    my ($array) = $_[1];

    $self->put_list;
    $self->enter;

    for my $value (@{$array}) {
        if (qpid::proton::is_num($value)) {
            if (qpid::proton::is_float($value)) {
                $self->put_float($value);
            } else {
                $self->put_int($value);
            }
        } elsif (!defined($value)) {
            $self->put_null;
        } elsif ($value eq '') {
            $self->put_string($value);
        } elsif (ref($value) eq 'HASH') {
            $self->put_map_helper($value);
        } elsif (ref($value) eq 'ARRAY') {
            $self->put_list_helper($value);
        } else {
            $self->put_string($value);
        }
    }

    $self->exit;
}

sub get_list_helper {
    my ($self) = @_;
    my $result = [];
    my $type = $self->get_type;

    if ($cproton_perl::PN_LIST == $type->get_type_value) {
        my $size = $self->get_list;

        $self->enter;

        for(my $count = 0; $count < $size; $count++) {
            if ($self->next) {
                my $value_type = $self->get_type;
                my $value = $value_type->get($self);

                push(@{$result}, $value);
            }
        }

        $self->exit;
    }

    return $result;
}

sub put_map_helper {
    my ($self) = @_;
    my $hash = $_[1];

    $self->put_map;
    $self->enter;

    foreach(keys %{$hash}) {
        my $key = $_;
        my $value = $hash->{$key};

        my $keytype = ::reftype($key);
        my $valtype = ::reftype($value);

        if ($keytype eq ARRAY) {
            $self->put_list_helper($key);
        } elsif ($keytype eq "HASH") {
            $self->put_map_helper($key);
        } else {
            $self->put_string("$key");
        }

        if (::reftype($value) eq HASH) {
            $self->put_map_helper($value);
        } elsif (::reftype($value) eq ARRAY) {
            $self->put_list_helper($value);
        } else {
            $self->put_string("$value");
        }
    }

    $self->exit;
}

sub get_map_helper {
    my ($self) = @_;
    my $result = {};
    my $type = $self->get_type;

    if ($cproton_perl::PN_MAP == $type->get_type_value) {
        my $size = $self->get_map;

        $self->enter;

        for($count = 0; $count < $size; $count++) {
            if($self->next) {
                my $key = $self->get_type->get($self);
                if($self->next) {
                    my $value = $self->get_type->get($self);
                    $result->{$key} = $value;
                }
            }
        }

        $self->exit;

    }

    return $result;
}

1;
