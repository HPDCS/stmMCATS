#!/bin/bash

maxThread=24
runPerThread=8

echo intruder  >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./intruder/intruder -a10 -l128 -n262144 -s1 -1 $nthread -2 $nthread -3 $nthread -w 0 -y 1000000 -j 4000 -z 32 -t$nthread 			
		nice -20 ./intruder/intruder -a10 -l128 -n262144 -s1 -1 $nthread -2 $nthread -3 $nthread -w 0 -y 1000000 -j 4000 -z 32 -t$nthread >> results_analysis.txt
		
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
	echo " " >> results_analysis.txt
done

echo yada >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./yada/yada -a15 -i yada/inputs/ttimeu1000000.2 -1 $nthread -t$nthread
		nice -20 ./yada/yada -a15 -i yada/inputs/ttimeu1000000.2 -1 $nthread -t$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
	echo " " >> results_analysis.txt
done

echo genome >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./genome/genome -g16384 -s64 -n16777216 -1 $nthread -t$nthread
		nice -20 ./genome/genome -g16384 -s64 -n16777216 -1 $nthread -t$nthread
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
	echo " " >> results_analysis.txt
done

echo vacation >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4194304 -1 $nthread -c$nthread
		nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4194304 -1 $nthread -c$nthread
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
	echo " " >> results_analysis.txt
done

echo labyrinth >> results_analysis.txt
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20./labyrinth/labyrinth -i labyrinth/inputs/random-x512-y512-z7-n512.txt -1 $nthread -t$nthread
		nice -20./labyrinth/labyrinth -i labyrinth/inputs/random-x512-y512-z7-n512.txt -1 $nthread -t$nthread
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
	echo " " >> results_analysis.txt
done
