

#  $PostgreSQL$

package PostgreSQL::InServer::safe;
 
# Load widely useful pragmas into plperl to make them available.
# SECURITY RISK:
# These must be trusted to not expose a way to execute a string eval
# or any kind of unsafe action that untrusted code could exploit.
# If in ANY doubt about a module, or ANY of the modules down the chain of
# dependencies it loads, then DO NOT add it to this list.

require strict;
require Carp;
require Carp::Heavy;
require warnings;
require feature if $] >= 5.010000;
