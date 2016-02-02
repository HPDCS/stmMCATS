#!/bin/bash

maxThread=16
runPerThread=1

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo yada >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./yada -a15 -i inputs/ttimeu100000.2 -t$nthread 			
		nice -20 ./yada -a15 -i inputs/ttimeu100000.2 -t$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo yada >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./yada -a10 -i inputs/ttimeu1000000.2 -t$nthread                       
                nice -20 ./yada -a10 -i inputs/ttimeu1000000.2 -t$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo yada >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./yada -a20 -i inputs/ttimeu100000.2 -t$nthread                       
                nice -20 ./yada -a20 -i inputs/ttimeu100000.2 -t$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo yada >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./yada -a15 -i inputs/ttimeu1000000.2 -t$nthread                       
                nice -20 ./yada -a15 -i inputs/ttimeu1000000.2 -t$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
