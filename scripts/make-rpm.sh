#!/bin/sh

# This script must be run from the repo top directory!

mkdir -p ~/rpmbuild/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
git archive --prefix fw-flash/ -o fw-flash.tar.gz HEAD
mv fw-flash.tar.gz ~/rpmbuild/SOURCES
rpmbuild -ba fw-flash.spec
cp ~/rpmbuild/RPMS/`uname -m`/fw-flash* .
rm -rf ~/rpmbuild
