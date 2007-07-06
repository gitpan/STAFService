package SimpleService;

sub new {
    my ($class, $params) = @_;
    print "Params: ", join(", ", map $_."=>".$params->{$_}, keys %$params), "\n";
    return bless {}, $class;
}

sub AcceptRequest {
    my ($self, $params) = @_;
    return (0, 42);
}

1;