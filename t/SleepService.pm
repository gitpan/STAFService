package SleepService;
use threads;
use threads::shared;
use Thread::Queue;
use strict;
use warnings;
no warnings 'once';

# In this queue the master threads queue jobs for the slave worker
my $work_queue = new Thread::Queue;
my $free_workers : shared = 0;

sub new {
    my ($class, $params) = @_;
    print "Params: ", join(", ", map $_."=>".$params->{$_}, keys %$params), "\n";
    my $self = {
        threads_list => [],
        worker_created => 0,
        max_workers => 5, # do not create more then 5 workers
    };
    return bless $self, $class;
}

sub AcceptRequest {
    my ($self, $params) = @_;
    my @array : shared = ($params->{requestNumber}, $params->{request});
    if ($free_workers <= 0 and $self->{worker_created} < $self->{max_workers}) {
        my $thr = threads->create(\&Worker);
        push @{ $self->{threads_list} }, $thr;
        $self->{worker_created}++;
    } else {
        lock $free_workers;
        $free_workers--;
    }
    $work_queue->enqueue(\@array);
    return $STAF::DelayedAnswer;
}

sub DESTROY {
    my ($self) = @_;
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