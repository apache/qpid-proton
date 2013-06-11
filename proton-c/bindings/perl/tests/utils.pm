sub random_integers
{
    my $len = shift;
    my @result;

    foreach (1..$len) {
        my $value = int(rand(100));
        push @result, $value;
    }

    return @result;
}

sub random_hash
{
    my $len = shift;
    my %result;

    foreach (1..$len) {
        my $key = random_string(32);
        my $val = random_string(128);
        $result{$key} = $val;
    }

    return \%result;
}

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

sub random_strings
{
    my $len = $_[0];
    my @result = ();

    foreach (1..$len) {
        my $strlen = rand(64) + 32;
        push(@result, random_string($strlen));
    }

    return @result;
}

sub random_timestamp
{
    my $result = rand(2**63) + 1;

    return $result;
}

1;
