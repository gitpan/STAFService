#!/usr/bin/perl -w
use strict;
use PLSTAF;
use FindBin;
use Config;

my $uselib = $FindBin::Bin;
my $dll = $FindBin::Bin . "/../PERLSRV.$Config{'so'}";

my $handle = STAF::STAFHandle->new("Test_01"); 
if ($handle->{rc} != $STAF::kOk) { 
    print "Error registering with STAF, RC: $handle->{rc}\n"; 
    die $handle->{rc}; 
}
my_submit($handle, "SERVICE", "ADD SERVICE FirstTest LIBRARY $dll EXECUTE SimpleService OPTION USELIB=\"$uselib\"");
my $result = my_submit($handle, "FirstTest", "get value");
my_submit($handle, "SERVICE", "REMOVE SERVICE FirstTest");
print "Test - ", (defined($result) && ($result==42) ? "OK" : "NOT OK"), "\n";

sub my_submit {
    my ($handle, $srv, $request) = @_;
    my $result = $handle->submit("local", $srv, $request); 
    if ($result->{rc} != $STAF::kOk) { 
        print "Error getting result, request='$request', RC: $result->{rc}\n"; 
        if (length($result->{result}) != 0) { 
            print "Additional info: $result->{result}\n"; 
        } 
        die $result->{rc}; 
    } 
    return $result->{result}; 
}

