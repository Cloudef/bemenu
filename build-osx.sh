#!/bin/sh
sed 's/-soname/-install_name/' GNUmakefile > GNUmakefile.osx
gmake -f GNUmakefile.osx "$@"
