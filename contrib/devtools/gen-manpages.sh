
#!/bin/sh

TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
SRCDIR=${SRCDIR:-$TOPDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

VIDULUMD=${VIDULUMD:-$SRCDIR/vidulumd}
VIDULUMCLI=${VIDULUMCLI:-$SRCDIR/vidulum-cli}
VIDULUMTX=${VIDULUMTX:-$SRCDIR/vidulum-tx}

[ ! -x $VIDULUMD ] && echo "$VIDULUMD not found or not executable." && exit 1