#!/bin/sh

SYS_CONF="$MPI_ROOT/etc/lampi.conf"
USR_CONF="$HOME/.lampi.conf"

[ -r $SYS_CONF ] && SYS_DEFAULT=`sed 's/#.*$//' < $SYS_CONF | tr '\n' ' '`
[ -r $USR_CONF ] && USR_DEFAULT=`sed 's/#.*$//' < $USR_CONF | tr '\n' ' '`

echo lampirun $SYS_DEFAULT $USR_DEFAULT "$@"
exec lampirun $SYS_DEFAULT $USR_DEFAULT "$@"
