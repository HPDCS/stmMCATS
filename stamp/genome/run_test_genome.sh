#!/bin/bash

maxThread=16
runPerThread=1

#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo genome >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./genome -s64 -g24576 -n16777216 -t$nthread 			
		nice -20 ./genome -s64 -g24576 -n16777216 -t$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo genome >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./genome -s96 -g16384 -n16777216 -t$nthread                       
                nice -20 ./genome -s96 -g16384 -n16777216 -t$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo genome >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./genome -s32 -g8192 -n8388608 -t$nthread                       
                nice -20 ./genome -s32 -g8192 -n8388608 -t$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
echo genome >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
        do
        k=0
        while [ $k -lt $runPerThread ]
                do
                echo nice -20 ./genome -s32 -g32768 -n8388608 -t$nthread                       
                nice -20 ./genome -s32 -g32768 -n8388608 -t$nthread >> results_analysis.txt
                k=$[$k+1]
        done
        nthread=$[$nthread+1]
done
#------------------------------------------------------------------------------------------------
#------------------------------------------------------------------------------------------------
