
#!/bin/bash
## Usage:
##  ./zcutil/build-debian-package.sh

set -e
set -x

BUILD_PATH="/tmp/zcbuild"
PACKAGE_NAME="vidulum"
SRC_PATH=`pwd`
SRC_DEB=$SRC_PATH/contrib/debian
SRC_DOC=$SRC_PATH/doc

umask 022

if [ ! -d $BUILD_PATH ]; then
    mkdir $BUILD_PATH
fi

PACKAGE_VERSION=$($SRC_PATH/src/vidulumd --version | grep version | cut -d' ' -f4 | tr -d v)
DEBVERSION=$(echo $PACKAGE_VERSION | sed 's/-beta/~beta/' | sed 's/-rc/~rc/' | sed 's/-/+/')
BUILD_DIR="$BUILD_PATH/$PACKAGE_NAME-$PACKAGE_VERSION-amd64"

if [ -d $BUILD_DIR ]; then
    rm -R $BUILD_DIR
fi

DEB_BIN=$BUILD_DIR/usr/bin
DEB_CMP=$BUILD_DIR/usr/share/bash-completion/completions
DEB_DOC=$BUILD_DIR/usr/share/doc/$PACKAGE_NAME
DEB_MAN=$BUILD_DIR/usr/share/man/man1
mkdir -p $BUILD_DIR/DEBIAN $DEB_CMP $DEB_BIN $DEB_DOC $DEB_MAN
chmod 0755 -R $BUILD_DIR/*

# Package maintainer scripts (currently empty)
#cp $SRC_DEB/postinst $BUILD_DIR/DEBIAN
#cp $SRC_DEB/postrm $BUILD_DIR/DEBIAN
#cp $SRC_DEB/preinst $BUILD_DIR/DEBIAN
#cp $SRC_DEB/prerm $BUILD_DIR/DEBIAN
# Copy binaries
cp $SRC_PATH/src/vidulumd $DEB_BIN
cp $SRC_PATH/src/vidulum-cli $DEB_BIN
cp $SRC_PATH/zcutil/fetch-params.sh $DEB_BIN/vidulum-fetch-params
# Copy docs
cp $SRC_PATH/doc/release-notes/release-notes-1.0.0.md $DEB_DOC/changelog
cp $SRC_DEB/changelog $DEB_DOC/changelog.Debian
cp $SRC_DEB/copyright $DEB_DOC
cp -r $SRC_DEB/examples $DEB_DOC
# Copy manpages
cp $SRC_DOC/man/vidulumd.1 $DEB_MAN
cp $SRC_DOC/man/vidulum-cli.1 $DEB_MAN
cp $SRC_DOC/man/vidulum-fetch-params.1 $DEB_MAN
# Copy bash completion files
cp $SRC_PATH/contrib/vidulumd.bash-completion $DEB_CMP/vidulumd
cp $SRC_PATH/contrib/vidulum-cli.bash-completion $DEB_CMP/vidulum-cli
# Gzip files
gzip --best -n $DEB_DOC/changelog
gzip --best -n $DEB_DOC/changelog.Debian
gzip --best -n $DEB_MAN/vidulumd.1
gzip --best -n $DEB_MAN/vidulum-cli.1
gzip --best -n $DEB_MAN/vidulum-fetch-params.1

cd $SRC_PATH/contrib

# Create the control file
dpkg-shlibdeps $DEB_BIN/vidulumd $DEB_BIN/vidulum-cli
dpkg-gencontrol -P$BUILD_DIR -v$DEBVERSION

# Create the Debian package
fakeroot dpkg-deb --build $BUILD_DIR
cp $BUILD_PATH/$PACKAGE_NAME-$PACKAGE_VERSION-amd64.deb $SRC_PATH
# Analyze with Lintian, reporting bugs and policy violations
lintian -i $SRC_PATH/$PACKAGE_NAME-$PACKAGE_VERSION-amd64.deb
exit 0