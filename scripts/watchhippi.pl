$watchlist = "";
sub showif {
  $name = shift;
  $printed = 0;
  
  open(IFACE, "/usr/etc/hipcntl-status $name |") || die "Can run hipcntl\n";;
  while (<IFACE>) {
#    print "IFACE Yields $_\n";
    if ($_ =~ ":") {
    chop;
# ah, perl. The next two lines screw up some (but not all) of the 
# assoc. stores below. Toy. 
# yeesh.
#    s/\t\t*/ /g;
#    s/  */ /g;
    ($key, $val) = split(/:/, $_, 2);
    $w = "$name:$key";
#    print "Check for $w current $val stored $watchlist{$w}\n";
    if ($watchlist{$w} != $val) {
	print("$w OLD: '$watchlist{$w}', NEW '$val'\n");
        $printed++;
#        print "set $w :$watchlist{$w}: to :$val:\n";
     }
    $watchlist{$w} = $val;
    }
  }
#  print "showif $name\n";
#  close(<IFACE>);
  close(IFACE);
#  if ($printed > 0) {
#    print "\n";
#  }
}
open(IFCONFIG, "/usr/etc/ifconfig -a |") || die "Can't start ifconfig\n";
$ifno = 0;
$ifaces = "";
while (<IFCONFIG>) {
        if ($_ =~ "hip") {
		chop;
		s/:.*//;
		$ifaces[$ifno] = $_;
        	$ifno++;
	}
}

for($i = 0; $i < $ifno; $i++) {
	print "Got $ifaces[$i]\n";
}

while (1) {
  for($i = 0; $i < $ifno; $i++)  {
	&showif($ifaces[$i]);
  }
  sleep(1);
}
