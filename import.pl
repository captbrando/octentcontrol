#!/usr/bin/perl
# ( $Id: import.pl,v 1.4 2003/04/02 18:12:47 brw Exp $ )
#
# This script will parse the nessus plugins and import them into the database.
# Author: Branden R. Williams
# Copyright 2002-2003 Elliptix, LLC.
#
# Known Problems...
#	So I start looking at these files to parse them and there is ONE plugin that does not
#	support multi language files.  So instead of coding around this so that one plugin
#	gets imported correctly, I am going to just look for multi lingual support.  We can
#	import the ONE file manually.  It is this one...
#	simpleserverwww_dos.nasl: script_name("AnalogX SimpleServer:WWW  DoS");
#
#	Also a big Question Mark on the Required Ports field.  Come back atcha.
# 
# $Log: import.pl,v $
# Revision 1.4  2003/04/02 18:12:47  brw
# fixed copyright and added log/id cvs tags
#
#

use DBI();

#Define some reasonably constant items.
$NESSUS_DIR="/usr/local/lib/nessus/plugins";
$LOGFILE="/var/log/pluginimport.log";
$modifications_made=0;

#open logfile
open (LOGFILE, ">>$LOGFILE") || die "Could not open logfile";
print LOGFILE "\n\n\nStarting NASL script parsing run. - ". localtime() ."\n\n";

# initiate DB connection
my $dbh = DBI->connect("DBI:Pg:dbname=scan;host=localhost", "scan", "m3rl1n", {'RaiseError' => 1});

opendir(NDIR,"$NESSUS_DIR");
while($file = readdir(NDIR)) {
   	next if ($file =~ /^\./);
	next if ($file !~ /.*\.nasl/);
	open(NASLFILE, "$NESSUS_DIR/$file") || die "Could not open $file.  Badbadbad.";
	$multiLineRead = 0;
	$solutionRead = 0;
	$problem = "";
	$solution = "";
	$gotFamily = "n";
	$script_bugtraq_id = "";
	$script_cve_id = "";
	while (<NASLFILE>) {
		chomp;
		
		# is this a comment?
		next if ($_ =~ /^\#/);
		# are we done reading?
		last if ($_ =~ /exit\(0\);/);
		
		#First check for a multi-line read in progress...
		if ($multiLineRead == 1) {
			if ($_ =~  /^Risk factor/) {
				$multiLineRead = 0;
				$solutionRead = 0;
				($risk,$junk) = split (/\"/,$_);
				($junk,$risk) = split (/: /,$risk);
			} elsif ($_ =~  /^Solution/) {
				$solutionRead = 1;
				next if ($_ =~  / : $/);
				($junk,$solution) = split (/: /,$_);
			} elsif ($_ =~  /desc\[\"francais\"\]/) {
				$multiLineRead = 0;
			} elsif ($solutionRead == 1) {
				$solution .= $_ . " ";
			} else {
				$problem .= $_ . " ";
			}
		} else {
			if ($_ =~  /script_id/) {
				($junk,$val) = split (/\(/,$_);
				($script_id,$junk) = split (/\)/,$val);
			} elsif ($_ =~  /script_version/) {
				($junk,$script_version,$junk) = split (/\"/,$_);
			} elsif ($_ =~  /script_cve_id/) {
				($junk,$script_cve_id,$junk) = split (/\"/,$_);
			} elsif ($_ =~  /script_bugtraq_id/) {
				($junk,$val) = split (/\(/,$_);
				($script_bugtraq_id,$junk) = split (/\)/,$val);
			} elsif ($_ =~  /name\[\"english\"\]/) {
				if ($_ !~  /script_name/) {
					($junk,$val) = split (/\=/,$_);
					($junk,$name,$junk) = split (/\"/,$val);
				}
			} elsif ($_ =~  /desc\[\"english\"\]/) {
				if ($_ !~  /script_description/) {
					$multiLineRead=1;
				}
			} elsif ($_ =~  /summary\[\"english\"\]/) {
				if ($_ !~  /script_summary/) {
					($junk,$val) = split (/\=/,$_);
					($junk,$summary,$junk) = split (/\"/,$val);
				}
			} elsif ($_ =~ /script_category/) {
				($junk,$val) = split (/script_cat/,$_);
				($junk,$val) = split (/\(/,$val);
				($category,$junk) = split (/\)/,$val);
			} elsif ($_ =~ /script_copyright/ || $copyMultiline == 1) {
				if ($_ =~ /:$/ || $_ =~ /\($/) {
					$copyMultiline = 1;
					next;
				}
				$copyMultiline = 0;
				($junk,$copyright,$junk) = split (/"/,$_);
			} elsif ($_ =~ /family\[\"english\"\]/ && $gotFamily ne "y") {
				if ($_ !~  /script_family/) {
					($junk,$val) = split (/\=/,$_);
					($junk,$family,$junk) = split (/\"/,$val);
					$gotFamily = "y";
				}
			} elsif ($_ =~  /script_family/ && $gotFamily ne "y") {
				if ($_ !~  /family\[\"english\"\]/) {
					($junk,$family,$junk) = split (/\"/,$_);
					$gotFamily = "y";
				}
			}
		}
	}
	
	# just debugging.  Output our values and see if we rock or not.
#	print LOGFILE "Parsing Script file:\t\t$file\n";
#	print "Script file:\t\t$file\n";
#	print "Script ID:\t\t$script_id\n";
#	print "Script Version:\t\t$script_version\n";
#	print "Script CVE ID:\t\t$script_cve_id\n";
#	print "Script Name:\t\t$name\n";
#	print "Script Summary:\t\t$summary\n";
#	print "Script Category:\t$category\n";
#	print "Script Copyright:\t$copyright\n";
#	print "Script Family:\t\t$family\n";
#	print "Script Problem:\t\t$problem\n";
#	print "Script Solution:\t$solution\n";
#	print "Script Risk Factor:\t$risk\n";
#exit;		

	# We need to check to see if the plugin already exists and if the version is newer or the same.  UPDATE if it is newer, INSERT if it is not there, do nothing otherwise.
	my $sql = qq{ SELECT pluginid,version FROM plugins WHERE pluginid = '$script_id' };
	my $sth = $dbh->prepare( $sql );
	$sth->execute or die "Unable to execute query: $dbh->errstr\n"; 
	
	my( $pluginid, $version );
	$sth->bind_columns( undef, \$pluginid, \$version );
	
	while( $sth->fetch() ) {
		if ($version ne $script_version) {
			print LOGFILE "Updating Script file:\t\t$file\n";
			$dbh->do("UPDATE plugins SET version = " . $dbh->quote($script_version) . ", name = " . $dbh->quote($name) . ", cveid = " . $dbh->quote($script_cve_id) . ", problem = " . $dbh->quote($problem) . ", solution = " . $dbh->quote($solution) . ", riskfactor = " . $dbh->quote($risk) . ", family = " . $dbh->quote($family) . ", summary = " . $dbh->quote($summary) . ", category = " . $dbh->quote($category) . ", copyright = " . $dbh->quote($copyright) . ", bugtraqid = " . $dbh->quote($script_bugtraq_id) . " WHERE pluginid = " . $dbh->quote($script_id)) || die "Died trying to update plugin id $script_id: $dbh->errstr\n";
			$modifications_made++;
		#} else {
		#	print LOGFILE "Skipping Script file:\t\t$file\n";
		}
	}
	
	if ($sth->rows() == 0) {
		print LOGFILE "Inserting Script file:\t\t$file\n";
		#print LOGFILE ("INSERT INTO plugins (pluginid, version, name, cveid, problem, solution, riskfactor, family, summary, category, copyright, bugtraqid) VALUES (" . $dbh->quote($script_id) . "," . $dbh->quote($script_version) . "," . $dbh->quote($name) . "," . $dbh->quote($script_cve_id) . "," . $dbh->quote($problem) . "," . $dbh->quote($solution) . "," . $dbh->quote($risk) . "," . $dbh->quote($family) . "," . $dbh->quote($summary) . "," . $dbh->quote($category) . "," . $dbh->quote($copyright) . "," . $dbh->quote($script_bugtraq_id) . ")\n");
		$dbh->do("INSERT INTO plugins (pluginid, version, name, cveid, problem, solution, riskfactor, family, summary, category, copyright, bugtraqid) VALUES (" . $dbh->quote($script_id) . "," . $dbh->quote($script_version) . "," . $dbh->quote($name) . "," . $dbh->quote($script_cve_id) . "," . $dbh->quote($problem) . "," . $dbh->quote($solution) . "," . $dbh->quote($risk) . "," . $dbh->quote($family) . "," . $dbh->quote($summary) . "," . $dbh->quote($category) . "," . $dbh->quote($copyright) . "," . $dbh->quote($script_bugtraq_id) . ")") || die "Died trying to insert plugin id $script_id: $dbh->errstr\n";
			$modifications_made++;
	}
}

$dbh->disconnect();

if ($modifications_made > 0) {
	print LOGFILE "\n\n";
}
print LOGFILE "Finished NASL script parsing run. - ". localtime() ."\n";
close (LOGFILE);


