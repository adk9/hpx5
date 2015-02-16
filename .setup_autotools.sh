#!/bin/bash

if [ "$#" -lt 1 ]; then
    echo "Must specify an install prefix!"
    exit
fi

PREFIX=$1

DLDIR=$(mktemp)
SAVEDIR=$PWD

M4=m4-1.4.17
AM=automake-1.15
AC=autoconf-2.69
LT=libtool-2.4.2

echo -n "Downloading packages..."
wget -q -P $DLDIR http://ftp.gnu.org/gnu/m4/${M4}.tar.gz 
wget -q -P $DLDIR http://ftp.gnu.org/gnu/automake/${AM}.tar.gz
wget -q -P $DLDIR http://ftp.gnu.org/gnu/autoconf/${AC}.tar.gz
wget -q -P $DLDIR http://mirrors.axint.net/repos/gnu.org/libtool/${LT}.tar.gz
echo "done"

cd $DLDIR

for PKG in $M4 $AM $AC $LT; do
    echo -n "Installing $PKG..."
    tar -xf ${PKG}.tar.gz
    cd $PKG
    ./configure --prefix=${PREFIX}
    make -j 4 && make install
    cd ..
    echo "done"
done

cd $SAVEDIR
rm -rf $DLDIR

cat <<EOF
Packages installed to:

$PREFIX.  

Update your PATH environment variable to use the new autotools.

For example:
   $ export PATH=${PREFIX}/bin:\$PATH
EOF