#!perl -w

use strict;
use warnings;

use Opcode qw(opset invert_opset opset_to_hex opset_to_ops opdesc);

my $plperl_opmask_h   = shift || "plperl_opmask.h";
my $plperl_opmask_tmp = $plperl_opmask_h."tmp";
END { unlink $plperl_opmask_tmp }

open my $fh, ">", "$plperl_opmask_tmp"
	or die "Could not write to $plperl_opmask_tmp: $!";

printf $fh "#define PLPERL_SET_OPMASK(opmask) \\\n";
printf $fh "  memset(opmask, 1, PL_maxo);\t/* disable all */ \\\n";
printf $fh "  /* then allow some... */                       \\\n";

my $allowed_opset = opset(
	# basic set of opcodes
	qw[:default :base_math !:base_io sort time],
	# require is safe because we redirect the opcode
	# entereval is safe as the opmask is now permanently set
	qw[require entereval],
	# Disalow these opcodes that are in the :base_orig optag
	# (included in :default) but aren't considered sufficiently safe
	qw[!prtf !dbmopen !setpgrp !setpriority],
);

foreach my $opname (opset_to_ops($allowed_opset)) {
	printf $fh qq{  opmask[OP_%-12s] = 0;\t/* %s */ \\\n},
		uc($opname), opdesc($opname);
}
printf $fh "  /* end */ \n";

close $fh
	or die "Error closing $plperl_opmask_tmp: $!";

rename $plperl_opmask_tmp, $plperl_opmask_h
	or die "Error renaming $plperl_opmask_tmp to $plperl_opmask_h: $!";

exit 0;
