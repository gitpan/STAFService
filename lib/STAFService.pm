package STAFService;

our $VERSION = 0.20;

1;
__END__

=head1 NAME

STAFService - Perl extension for writing STAF Services easily.

=head1 SYNOPSIS

On the staf.cfg file:

  SERVICE service_name LIBRARY PERLSRV EXECUTE SimpleService

Or if SimpleService is not in the PERL5LIB:

  SERVICE service_name LIBRARY PERLSRV EXECUTE SimpleService OPTION USELIB="c:/STAF/Perl/handler"

And SimpleService.pm should look like that:

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

=head1 DESCRIPTION

This package supply the nessery dynamic library (or shared object) needed for
running Perl Services under STAF version 3.

In this simple module, every service have it's own Perl Interperter, and the
access is single-threads. (meaning that there won't be two concurent AcceptRequest
calls) For multi-threaded services, see below.

STDOUT is redirected to STAF's log file. So don't worry about this and just print
whatver you think should go to that log file. prints to STDERR will be displayed
in the STAFProc's window.

=head1 INSTALLATION

You know the drill.

  perl Makefile.pl
  make
  make test
  make install

=over
=item *
The installation process needs STAF to be up and running
=item *
Also, need STAF's bin directory in the Perl5Lib
=back

=head1 STAF CONFIGURATION

=head2 SERVICE service_name

The name of the service. Can be whatever you can think about, and not nessesrialy
connected to the package name, or anything else.

=head2 LIBRARY PERLSRV

Tells STAF that this service will be executed using a DLL/SO called PERLSRV.
(The SO might be called libPERLSRV.so, if this is you system convension)

=head2 EXECUTE SimpleService

Tells the PERLSRV DLL to use and new the SimpleService package. The following
steps (basically) will be executed:

  use SimpleService;
  my $obj = SimpleService->new($new_params_hash_ref);
  # Incoming Requests
  ($ret_code, $answer) = $obj->AcceptRequest($request_pararms_hash_ref);
  # And in the end:
  undef($obj);
  
So please supply a DESTROY sub for cleanup, if needed.

=head2 OPTION

You can specify three additional optional options. two of them are standard
in STAF, and the third is not.

=head3 MAXLOGS

The maximum number of log files to keep. Older log files will be deleted.

=head3 MAXLOGSIZE

The maximum size for a log file. (in bytes) The default value is 1MB.
The size is checked only on service's startup.

=head3 USELIB

Use this option to 'use lib' other locations before loading your package. this
option gives the ability to store your package on location other then the STAF's
bin directory.
This option can be specified multiple times for multiple locations.

=head2 PARMS 

Whatever you write after this keyword, will be passed to your service handler.
(in the new call)

=head2 Example

  SERVICE Mufleta LIBRARY PERLSRV EXECUTE SimpleService OPTION USELIB=c:/mylib PARMS "some text"

=head1 SERVICE METHODS

=head2 new

Should create a new service handler. returning anything other then an object,
will be treated as error and the service will be terminated.
'new' will recieve a hash ref, containing the following fields:

  ServiceName
  ServiceType - An integer refering to the type of the service. refere to STAF's documentation.
  WriteLocation - A directory for temporary files, if needed.
  Params - Whatever is writen in the PARMS in the config file.

Note that if a STAF handle is needed for this service, this is a good place to register it.

=head2 AcceptRequest

The worker function. will be called for every request that need to be served.
Should return two values: ($ret_code, $answer), where return code 0 represent success.
for other return code, please refer to STAF's documentation.
returning anything else will be treated as error.
'AcceptRequest' will recieve a hash ref, containing the following fields:

  stafInstanceUUID
  machine
  machineNickname
  request - The request itself.
  user
  endpoint
  physicalInterfaceID
  trustLevel
  isLocalRequest
  diagEnabled
  trustLevel
  requestNumber - needed for threaded services
  handleName - of the requesting process
  handle - the handle number of the requesting process

=head2 DESTROY

If cleanup is needed, you can implement a DESTROY method that will be called then
the service will be shut down.

=head1 USING THREADS

For writing a STAF Service that can serve multiple request concurently, you need to
answer a request with the B<$STAF::DelayedAnswer> special variable.

Asyncronically, Some internal thread inside the service should call:

  STAF::DelayedAnswer($requestNumber, $return_code, $answer);

The request number is supplied with the request. Note that it is your own responsibility
to manage your own threads. For an example, see t/SleepService.pm that is a full blown
multi threaded service.

On the other hand, it is possible to use the same API in a single threaded service.
Usefull when answer to one client has to wait for a request from other client. For an example,
see t/PerlLocks.om that emulate Perl's locking and signaling, using single threaded service.

=head1 BUGS

With SleepService.pm, for every worker thread created an error message "Leaked Scalars: 2"
is displayed. Yet to be resolved.

=head1 SEE ALSO

STAF homepage: L<<a href="http://staf.sourceforge.net/">http://staf.sourceforge.net/</a>>

=head1 AUTHOR

Fomberg Shmuel, E<lt>owner@semuel.co.ilE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2007 by Shmuel Fomberg.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut
