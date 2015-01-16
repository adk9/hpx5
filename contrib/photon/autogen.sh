#!/bin/sh
if [ ! -d config ]; then mkdir config; fi
set -e
autoreconf --force --install -I config || exit 1
rm -rf autom4te.cache
