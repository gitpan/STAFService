package STAFService;

our $VERSION = 0.21;

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
running Perl Services under STAF version 3. Version 0.21 is identical to the DLL
that delivered by the STAF distribution v3.2.4. 

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

=head2 MAPING TO STAF SERVICE STEPS

A STAF service has five steps of its life: Construct, Init, ServeRequests, Terminate,
Destroy. and these step are mapped to the following parts of your module:

=over
=item *
Construct: The module is being loaded.
=item *
Init: the new function is called. this is the place to create a STAF handle, if needed.
=item *
ServeRequest: the AcceptRequest function is called for every request
=item *
Terminate: the object is DESTORYed.
=item *
Destroy: the Perl interpreter is closed. END blocks will be executed and global objects
will be destroyed here.
=back

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
see t/PerlLocks.pm that emulate Perl's locking and signaling, using single threaded service.

Third possibility is to mixed approche. For answers that don't take much time to answer,
answer immidiately. (for example, to a query request) And for questions that take time,
take the threaded approche and deglate the work for one of the worker threads.

For example, this is the SleepService.pm used for testings:

    package SleepService;
    use threads;
    use threads::shared;
    use Thread::Queue;
    use strict;
    use warnings;
    
    # In this queue the master threads queue jobs for the slave worker
    my $work_queue = new Thread::Queue;
    # keeps track of how many free worker there are. can be below zero, if the
    # number of worker reached max_workers, and more request are being recieved
    # then being serviced.
    my $free_workers : shared = 0;
    
    sub new {
        my ($class, $params) = @_;
        print "Params: ", join(", ", map $_."=>".$params->{$_}, keys %$params), "\n";
        my $self = {
            threads_list => [],
            worker_created => 0,
            # limiting the number of created worker to some constant.
            max_workers => 5, 
        };
        return bless $self, $class;
    }
    
    sub AcceptRequest {
        my $self = shift;
        my $params = shift;
        # Primilinary checking. maybe we can satisfy this request immidiately,
        # and won't need to hand it off to a worker thread.
        if ($params->{trustLevel} < 3) {
            return (25, "Only trusted client can sleep here"); # kSTAFAccessDenied
        }
        if ($params->{request} == 0) {
            return (0, "still tired");
        }
        # Do we have a waiting worker? if not, create a new one.
        if ($free_workers <= 0 and $self->{worker_created} < $self->{max_workers}) {
            my $thr = threads->create(\&Worker);
            push @{ $self->{threads_list} }, $thr;
            $self->{worker_created}++;
        } else {
            lock $free_workers;
            $free_workers--;
        }
        my @array : shared = ($params->{requestNumber}, $params->{request});
        $work_queue->enqueue(\@array);
        return $STAF::DelayedAnswer;
    }
    
    sub DESTROY {
        my ($self) = @_;
        # The main service object itself is being copied with the worker threads,
        # and in the end of the program they are destroyed. this line make sure
        # that only the main thread will kill the worker thread.
        return unless threads->tid == 0;
        # Ask all the threads to stop, and join them.
        for my $thr (@{ $self->{threads_list} }) {
            $work_queue->enqueue('stop');
        }
        for my $thr (@{ $self->{threads_list} }) {
            eval { $thr->join() };
            print STDERR "On destroy: $@\n" if $@;
        }
    }
    
    sub Worker {
        my $loop_flag = 1;
        while ($loop_flag) {
            eval {
                # Step one - get the work from the queue
                my $array_ref = $work_queue->dequeue();
                if (not ref($array_ref) and $array_ref eq 'stop') {
                    $loop_flag = 0;
                    return;
                }
                my ($reqId, $reqTime) = @$array_ref;
                
                # Step two - sleep, and return an answer
                sleep($reqTime);
                STAF::DelayedAnswer($reqId, 0, "slept well");
                
                # Final Step - increase the number of free threads
                {
                    lock $free_workers;
                    $free_workers++;
                }
            }
        }
        return 1;
    }
    
    1;

=head1 BUGS

Non known.

=head1 SEE ALSO

STAF homepage: http://staf.sourceforge.net/

=head1 AUTHOR

Fomberg Shmuel, E<lt>owner@semuel.co.ilE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2007 by Shmuel Fomberg.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut
