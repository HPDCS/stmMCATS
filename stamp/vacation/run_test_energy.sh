#!/bin/bash

maxThread=16
runPerThread=1

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo intruder >> results_analysis.txt
maxThread=32
runPerThread=1

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo intruder >> results_analysis.txt
th=2
while [ $th -le $maxThread ] 
	do
	echo "TX_PER_CYCLE=1000 INITIAL_MAX_ADMITTED_TX=4 BUSY_WAITING_TIME_THRESHOLD="$th > mcats_conf.txt
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./vacation -n4 -q60 -u90 -r32768 -t524288 -c32                  
                nice -20 ./vacation -n4 -q60 -u90 -r32768 -t524288 -c32 >> results_analysis.txt
                k=$[$k+1]
        done
        th=$[$th+1]
done

