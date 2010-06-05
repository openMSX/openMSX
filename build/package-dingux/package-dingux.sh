#!/bin/bash
DISTDIR=derived/dist/dingux
ZIPFILE=openmsx-dingux-bin-`date +%Y-%m-%d`.zip
rm -rf $DISTDIR
mkdir -p $DISTDIR
rm -rf $DISTDIR/../$ZIPFILE

# Uncomment this if you want to create a zip in a developer's SVN tree.
# This will also produce patches of what you changed.
#REVISION=`svnversion -n .`
#if echo "$REVISION" | grep -q ':'
#then
#	echo "Mixed-revision working copy; cannot create consistent patch"
#	exit 1
#fi
#PATCH=$DISTDIR/openmsx-dingux-r`echo "$REVISION" | grep -o '[0-9]*'`.diff
#svn diff > $PATCH

cp doc/dingux-readme.txt $DISTDIR/README.txt

mkdir -p $DISTDIR/local
mkdir -p $DISTDIR/local/doc
mkdir -p $DISTDIR/local/sbin
mkdir -p $DISTDIR/local/share
cp -r derived/mipsel-dingux-opt-3rd/bindist/install/bin $DISTDIR/local/bin
cp build/package-dingux/openmsx-start.sh $DISTDIR/local/bin
chmod a+x $DISTDIR/local/bin/openmsx-start.sh
cp -r derived/mipsel-dingux-opt-3rd/bindist/install/doc $DISTDIR/local/doc/openmsx
cp -r derived/mipsel-dingux-opt-3rd/bindist/install/share $DISTDIR/local/share/openmsx

cd $DISTDIR && zip ../$ZIPFILE -r .

echo "Created ZIP: `dirname $DISTDIR`/$ZIPFILE"
