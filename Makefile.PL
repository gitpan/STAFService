#!/usr/bin/perl -w
use strict;
use Config;
use ExtUtils::Embed;

eval {
    require PLSTAF;
};
if ($@) {
    print "Error: can not load STAF's Perl support.\n";
    print "Please make sure that STAF 3 is installed\n";
    print "and its bin directory is in the Perl5Lib path\n";
    exit(1);
}

my $stafDir = GetStafDirFromCmdLine() || GetStafRootDir();

print_makefile($stafDir);


sub print_makefile {
    my ($stafDir) = @_;
    open my $mf, ">", "Makefile"
        or die "Can not write the Makefile!\n";
    print $mf "PERLSRV.$Config{'so'}: src/perlglue.cpp src/STAFPerlService.cpp\n";
    my $ccflags = ccflags();
    my $perl_include = perl_inc();
    my $src_include = create_include("./src");
    print $mf qq{\t$Config{'cc'} $ccflags $perl_include $src_include -c src/STAFPerlService.cpp src/perlglue.cpp\n};
    print $mf qq{\t$Config{'ld'} $Config{'lddlflags'} STAF$Config{'_a'} $Config{'libperl'} $Config{'libs'} -def:"src/STAFPerlService.def" -out:"PERLSRV.$Config{'so'}" -libpath:"$stafDir/lib" STAFPerlService$Config{'_o'} perlglue$Config{'_o'}\n};
    print $mf "\n";
    print $mf "install:\n";
    print $mf "\t$Config{cp} PERLSRV.$Config{'so'} $stafDir/bin\n";
    print $mf "\n";
    print $mf "clean:\n";
    print $mf "\t$Config{'rm'} *$Config{'_o'} *.$Config{'so'}\n";
    print $mf "\n";
    print $mf "test:\n";
    print $mf "\t$Config{'perlpath'} t/01.pl\n";
    print $mf "\n";
}

sub create_include {
    my $dir = shift;
    $dir = qq["$dir"] if $^O eq 'MSWin32';
    return(" -I$dir ");
}

sub GetStafDirFromCmdLine {
    my ($dirparam) = grep /^-stafdir=/, @ARGV;
    return unless defined $dirparam;

    my ($dir) = $dirparam =~ /=(.*)/;
    return unless -d $dir && -d $dir."/bin";
    return $dir if -e $dir."/bin/staf.cfg";
    return;
}

sub GetStafRootDir {
    my $handle = STAF::STAFHandle->new("My program"); 
    if ($handle->{rc} != $STAF::kOk) { 
        print "Error registering with STAF, RC: $handle->{rc}\n"; 
        die $handle->{rc}; 
    } 
    
    my $result = $handle->submit("local", "VAR", "resolve string {STAF/Config/STAFRoot}"); 
    if ($result->{rc} != $STAF::kOk) { 
        print "Error getting STAF home, RC: $result->{rc}\n"; 
        if (length($result->{result}) != 0) { 
            print "Additional info: $result->{result}\n"; 
        } 
        die $result->{rc}; 
    } 
    
    my $StafRoot = $result->{result}; 
    
    $handle->unRegister();
    
    return $StafRoot;
}
