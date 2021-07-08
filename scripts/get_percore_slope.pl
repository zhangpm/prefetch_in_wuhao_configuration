#!/usr/bin/perl
if($#ARGV != 2){
    print "ERROR : Usage : perl get_hitrate.pl <baseline> <dut> <coreid>\n";
    exit;
}

our $baseline_stats_file = shift;
our $dut_stats_file = shift;
our $coreid = shift;

my $baseline_hits = compute_hits($baseline_stats_file, $coreid);
#print "$baseline_hits\n";
my $dut_hits = compute_hits($dut_stats_file, $coreid);
#print "$dut_hits\n";

my $baseline_traffic = compute_dram_traffic($baseline_stats_file);
my $dut_traffic = compute_dram_traffic($dut_stats_file);

my $slope = ($dut_traffic - $baseline_traffic)/($dut_hits - $baseline_hits);
print "$slope\n";

sub compute_dram_traffic
{
    $stats_file = $_[0];   
    my $dram_traffic = 0;
    my $roi = 0;

    foreach (qx[cat $stats_file 2>/dev/null]) {
        $line = $_;

        if ($line =~ m/Region of Interest Statistics/)
        {
            $roi = 1;
        }
        if (($roi == 1) && ($line =~ m/LLC TOTAL[\s\t]+ACCESS:[\s\t]+([\d]+)[\s\t]+HIT:[\s\t]+([\d]+)[\s\t]+MISS:[\s\t]+([\d]+)/))
        {
            $dram_traffic = $dram_traffic + $3;
        }
    }

    unless ( defined($dram_traffic) ) {
        print "ERROR problem with $stats_file\n";
        return $dram_traffic;
    }
    return $dram_traffic;
}

sub compute_hits
{
    $stats_file = $_[0];   
    $core = $_[1];
    my $num_hits = 0;
    my $num_accesses = 0;
    my $roi = 0;
    my $core_stats = 0;

    foreach (qx[cat $stats_file 2>/dev/null]) {
        $line = $_;

        if ($line =~ m/Region of Interest Statistics/)
        {
            $roi = 1;
        }
    
        if ($line =~ m/CPU ([\d]+)/)
        {
            if($1 == $core)
            {
                $core_stats = 1;
            }
            else
            {
                $core_stats = 0;
            }
        }
    
        if (($roi == 1) && ($core_stats == 1) && ($line =~ m/LLC LOAD[\s\t]+ACCESS:[\s\t]+([\d]+)[\s\t]+HIT:[\s\t]+([\d]+)[\s\t]+MISS:[\s\t]+([\d]+)/))
        {
            $num_hits = $num_hits + $2;
            $num_accesses = $num_accesses + $1;
        }
        
        if (($roi == 1) && ($core_stats == 1) && ($line =~ m/LLC RFO[\s\t]+ACCESS:[\s\t]+([\d]+)[\s\t]+HIT:[\s\t]+([\d]+)[\s\t]+MISS:[\s\t]+([\d]+)/))
        {
            $num_hits = $num_hits + $2;
            $num_accesses = $num_accesses + $1;
        }
    }

    $hit_rate = 100*$num_hits/$num_accesses;
    if($hit_rate < 0.01) {
        $hit_rate = 0.01;
    }
    unless ( defined($hit_rate) ) {
        print "ERROR problem with $stats_file\n";
        return $$num_hits;
    }
    return $num_hits;
}

