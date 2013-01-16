
sub random_string
{
    my $len=$_[0];

    my @chars=('a'..'z','A'..'Z','0'..'9','_');
    my $result;
    foreach (1..$len) {
        $result .= $chars[rand @chars];
    }
    return $result;
}

sub random_timestamp
{
    my $result = rand(2**63) + 1;

    return $result;
}

1;
