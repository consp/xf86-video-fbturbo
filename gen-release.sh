#!/bin/bash
set -e
rm -rf release
mkdir -p release

pushd src
make
popd

pushd dispfd-daemon
make
popd

cp src/fbturbo_drv.la release/
cp -r src/.libs release/
cp 10-d1.conf release/
cp libtool release/
cp release_makefile release/Makefile
cp dispfd-daemon/devterm-r01-dispfd-daemon.service release/
cp dispfd-daemon/r01-dispfd-daemon.elf release/

pushd release
tar czf fbturbo-r01.tar.gz .libs *
popd
