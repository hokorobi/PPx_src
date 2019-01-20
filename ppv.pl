#
# -*- Perl -*-
#
#  Filter for namazu.
#
#  This file must be encoded in EUC-JP encoding
#

package ppv;
use strict;
use File::Copy;
require 'util.pl';
require 'gfilter.pl';

my $ppvconvpath  = undef;

sub mediatype() {
	return ('application/oasys');
}

sub status() {
	$ppvconvpath = util::checkcmd('ppv.exe');
	return 'yes' if defined $ppvconvpath;
	return 'no';
}

sub recursive() {
	return 0;
}

sub pre_codeconv() {
	return 0;
}

sub post_codeconv () {
	return 0;
}

sub add_magic ($) {
	my ($magic) = @_;

	$magic->addFileExts('\\.oa2$', 'application/oasys');
	return;
}

sub filter ($$$$$) {
	my ($orig_cfile, $cont, $weighted_str, $headings, $fields) = @_;
	my $cfile = defined $orig_cfile ? $$orig_cfile : '';

	my $tmpfile  = util::tmpnam('NMZppv');

	util::vprint("Processing document file ... (using  '$ppvconvpath')\n");

	system("$ppvconvpath $cfile -convert:\"$tmpfile\"");

	open(IN, "< $tmpfile");
	$$cont = "";
	while (<IN>) {$$cont .= $_;}
	close(IN);

	unlink $tmpfile;

	gfilter::line_adjust_filter($cont);
	gfilter::line_adjust_filter($weighted_str);
	gfilter::white_space_adjust_filter($cont);
	$fields->{'title'} = gfilter::filename_to_title($cfile, $weighted_str)
	unless $fields->{'title'};
	gfilter::show_filter_debug_info($cont, $weighted_str,$fields, $headings);

	return undef;
}

1;
