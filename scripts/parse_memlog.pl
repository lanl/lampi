#! /usr/bin/perl -w

use strict;
use FileHandle;
use Getopt::Long;

my %ele_data = ();
my %chunk_data = ();
my @fields = ();
my ($name, $index, $pid, $max, $ave, $requested, $returned);
my ($key, $val, $summary, $sort_type);

format ELEMENT_LONG_TOP =

                   Element Data (Long)

Name                              Index   Pid    Max   Ave  
----------------------------------------------------------------
.

format ELEMENT_LONG =
@<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< @<<<<   @<<<<< @<<<< @<<<<
$name, $index, $pid, $max, $ave
.

format CHUNK_LONG_TOP =

                  Chunk Data (Long)

Name                              Index  Pid     Requested   Returned
----------------------------------------------------------------------
.

format CHUNK_LONG =
@<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< @<<<<  @<<<<<  @<<<<<      @<<<<<
$name, $index, $pid, $requested, $returned
.
format ELEMENT_TOP =

                   Element Data

Name                              Index   Max   Ave  
----------------------------------------------------------------
.

format ELEMENT =
@<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< @<<<<   @<<<< @<<<<
$name, $index, $max, $ave
.

format CHUNK_TOP =

                  Chunk Data

Name                              Index  Requested   Returned
----------------------------------------------------------------------
.

format CHUNK =
@<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< @<<<<  @<<<<<      @<<<<<
$name, $index, $requested, $returned
.

# check for correct number of arguments
if ($#ARGV < 0) {
    print "parse_memlog.pl <--summary> <files> ";
    exit(0);
}

# parse command line
GetOptions("summary" => \$summary, "sort=s" => \$sort_type);

# extract element, chunk data from specified files
while (<>) {
    @fields = split / : /;
    $key = $summary ? join ',', @fields[1,3] 
        : join ',', @fields[1,2,3];
    
    if (not $summary and (exists($ele_data{$key}) 
        or exists($chunk_data{$key}))) {
        print "Duplicate entries for $fields[0], key = $key\n";
        exit(0);
    }
    
    if (not $summary) {
        $ele_data{$key} = [ @fields[0,4,5,6] ];
        $chunk_data{$key} = [ @fields[0,7,8] ];
    } else { # summary mode
        if (exists($ele_data{$key})) {
            if ($ele_data{$key}[1] < $fields[4]) {
                $ele_data{$key}[1] = $fields[4];
            }
            $ele_data{$key}[2] += $fields[5];
            $ele_data{$key}[3] += $fields[6];
        } else {
            $ele_data{$key} = [ @fields[0,4,5,6] ];
        }
        
        if (exists($chunk_data{$key})) {
            $chunk_data{$key}[1] += $fields[7];
            $chunk_data{$key}[2] += $fields[8];
        } else {
            $chunk_data{$key} = [ @fields[0,7,8] ];
        }
    }
}

# print results    
if ($summary) {
    STDOUT->format_name("ELEMENT");
} else {
    STDOUT->format_name("ELEMENT_LONG");
}
while (($key, $val) = each(%ele_data)) {
    if ($summary) {
        $index = (split /,/, $key)[1];
    } else {
        ($pid, $index) = (split /,/, $key)[1,2];
    }

    $name = $val->[0];
    $name =~ s/^\s+//;
    $name =~ s/\s+$//;
    $max = $val->[1];
    $ave = $val->[3] == 0 ? 0 : $val->[2] / $val->[3];
    write;
}

if ($summary) {
    STDOUT->format_name("CHUNK");
    STDOUT->format_top_name("CHUNK_TOP");
} else {
    STDOUT->format_name("CHUNK_LONG");
    STDOUT->format_top_name("CHUNK_LONG_TOP");
}
STDOUT->format_lines_left(0);
while (($key, $val) = each(%chunk_data)) {
    if ($summary) {
        $index = (split /,/, $key)[1];
    } else {
        ($pid, $index) = (split /,/, $key)[1,2];
    }

    $name = $val->[0];
    $name =~ s/^\s+//;
    $name =~ s/\s+$//;
    $requested = $val->[1];
    $returned = $val->[2];
    write;
}

