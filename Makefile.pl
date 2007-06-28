#!/usr/bin/perl -w
use strict;
use PLSTAF;
use Config;

my $stafDir = GetStafRootDir();

print_makefile($stafDir);


sub print_makefile {
    my ($stafDir) = @_;
    open my $mf, ">", "Makefile"
        or die "Can not write the Makefile!\n";
    print $mf "PERLSRV.$Config{'so'}: perlglue.cpp STAFPerlService.cpp\n";
    print $mf qq{\t$Config{'cc'} $Config{'ccflags'} -GX -I "$stafDir/include" -I "$Config{'privlib'}/CORE" -D "WIN32" -D "_DEBUG" -D "_WINDOWS" -D "_MBCS" -D "_USRDLL" -c STAFPerlService.cpp perlglue.cpp\n};
    # -MTd -nologo -W3 -Gm -ZI -Od -YX -FD -GZ
    # /Fo"Debug/"  - objects dir
    my $linker_option = $Config{'lddlflags'};
    $linker_option =~ s/-nodefaultlib|-nostdlib//;
    print $mf qq{\t$Config{'ld'} $linker_option STAF$Config{'_a'} $Config{'libperl'} -def:"STAFPerlService.def" -out:"PERLSRV.$Config{'so'}" -libpath:"$stafDir/lib" STAFPerlService$Config{'_o'} perlglue$Config{'_o'}\n};
    # -nologo -dll -machine:I386
    # -libpath:"$Config{'privlib'}/CORE"  -pdbtype:sept 
    # /implib:"Debug/PERLSRV.lib" /pdb:"Debug/PERLSRV.pdb" $Config{'lddlflags'} $Config{'perllibs'}
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
