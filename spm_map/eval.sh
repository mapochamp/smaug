#! /usr/bin/bash

for dir in ./*/
do
    dir=${dir%*/}
    echo "${dir##*/}"
    #(cd $dir && python /home/chunchunmaru/masters/spm/smaug/gurobi/prayers.py)
    (cd $dir && python /home/chunchunmaru/masters/spm/smaug/gurobi/latency_analysis.py)
    echo ""
done
