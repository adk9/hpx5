#!/usr/bin/env bash
if [ ! -d config ]; then mkdir config; fi

URL="http://stout.crest.iu.edu/photon"
DIR="/tmp"
CONTRIB=src/contrib

# set the array of tarball deps here
contrib_tars=("libfabric-1.2.0.tar.bz2")
# and matching target contrib directories
contrib_dirs=("libfabric")

CMD="wget --quiet -O"
if ! type "wget" > /dev/null; then
  CMD="curl --silent -o"
fi
echo -n "Downloading and extracting contrib tarballs..."
for ((i=0; i<${#contrib_tars[@]}; i++)); do
  dir=${contrib_dirs[$i]}
  file=${contrib_tars[$i]}
  echo -n "$dir..."
  $CMD ${DIR}/$file ${URL}/$file
  mkdir -p ${CONTRIB}/$dir
  tar --strip-components=1 -xf ${DIR}/$file -C ${CONTRIB}/$dir
  rm -f ${DIR}/$file
done
echo "DONE"

set -e
autoreconf --force --install -I config || exit 1
rm -rf autom4te.cache
