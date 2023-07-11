#! /usr/bin/bash

#for dir in ./*/
#do
#    dir=${dir%*/}
#    echo "${dir##*/}"
#    #(cd $dir && python /home/chunchunmaru/masters/spm/smaug/gurobi/prayers.py 32768)
#    #(cd $dir && python /home/chunchunmaru/masters/spm/smaug/gurobi/latency_analysis.py)
#    echo ""
#done

#      32    64    128    256    512    1024    2045
#sizes=(32768 65536 131072 262144 524288 1048576 2097152)
# we use this order for the size array since thats the
# alphabetical order we get the directories in
sizes=(262144 1048576 32768 131072 524288 65536 2097152)
count=0

for dir in $(find . -maxdepth 2 -mindepth 2 -type d)
do
    echo "$dir"
    (cd $dir && python /home/chunchunmaru/masters/spm/smaug/gurobi/prayers.py "${sizes[$count]}")
    (cd $dir && python /home/chunchunmaru/masters/spm/smaug/gurobi/latency_analysis.py > savings.txt)
    count=$(($count + 1))
    if [[ $count -gt 6 ]]
    then
        count=0
    fi
done
