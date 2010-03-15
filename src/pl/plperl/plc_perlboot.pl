
#  $PostgreSQL$

use 5.008001;

PostgreSQL::InServer::Util::bootstrap();

package PostgreSQL::InServer;

use strict;
use warnings;
use vars qw(%_SHARED);

sub plperl_warn {
	(my $msg = shift) =~ s/\(eval \d+\) //g;
	chomp $msg;
	&::elog(&::WARNING, $msg);
}
$SIG{__WARN__} = \&plperl_warn;

sub plperl_die {
	(my $msg = shift) =~ s/\(eval \d+\) //g;
	die $msg;
}
$SIG{__DIE__} = \&plperl_die;

sub ::encode_array_literal {
	my ($arg, $delim) = @_;
	return $arg
		if ref $arg ne 'ARRAY';
	$delim = ', ' unless defined $delim;
	my $res = '';
	foreach my $elem (@$arg) {
		$res .= $delim if length $res;
		if (ref $elem) {
			$res .= ::encode_array_literal($elem, $delim);
		}
		elsif (defined $elem) {
			(my $str = $elem) =~ s/(["\\])/\\$1/g;
			$res .= qq("$str");
		}
		else {
			$res .= 'NULL';
		}
	}
	return qq({$res});
}

sub ::encode_array_constructor {
	my $arg = shift;
	return ::quote_nullable($arg)
		if ref $arg ne 'ARRAY';
	my $res = join ", ", map {
		(ref $_) ? ::encode_array_constructor($_)
		         : ::quote_nullable($_)
	} @$arg;
	return "ARRAY[$res]";
}

# A hash of private code refs that plperl code won't have access to.
# The $PRIVATE variable is reset to undefined during initialization.
our $PRIVATE = {
	plperl_mkfunc => sub {

		my ($name, $imports, $prolog, $src) = @_;

		my $BEGIN = join "\n", map {
			my $names = $imports->{$_} || [];
			"$_->import(qw(@$names));"
		} sort keys %$imports;
		$BEGIN &&= "BEGIN { $BEGIN }";

		$name =~ s/\\/\\\\/g;
		$name =~ s/::|'/_/g; # avoid package delimiters

		my $code = qq[ package main; sub { $BEGIN $prolog $src } ];

		no strict;   # default to no strict for the eval
		no warnings; # default to no warnings for the eval
		my $ret = eval($code);
		$@ =~ s/\(eval \d+\) //g if $@;
		return $ret;
	},
};
