#! /usr/bin/bash

#for dir in ./*/
#do
#    dir=${dir%*/}
#    echo "${dir##*/}"
#    (cd $dir && mkdir 32kb 64kb 128kb 256kb 512kb 1024kb 2048kb)
#    echo ""
#done
sizes=(262144 1048576 32768 131072 524288 65536 2097152)
count=0

for dir in $(find . -maxdepth 2 -mindepth 2 -type d)
do
    echo "$dir"
    (cd $dir && cp ../*.txt .)
done
