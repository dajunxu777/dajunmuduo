#!/bin/bash

set -e

if [ ! -d `pwd`/build ]; then
    mkdir `pwd`/build
fi

rm -rf `pwd`/build/*

#cd `pwd`/build

cd `pwd`/build &&
    cmake .. &&
    make

cd ..

if [ ! -d /usr/include/dajunmuduo ]; then
    mkdir /usr/include/dajunmuduo
fi

for header in `ls *.h`
do 
    cp $header /usr/include/dajunmuduo
done

cp `pwd`/lib/libdajunmuduo.so /usr/lib

ldconfig
