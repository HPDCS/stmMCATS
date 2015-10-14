#!/bin/bash

maxThread1=24
maxThread2=16
runPerThread=8

echo intruder  >> 24th.txt
nthread=2
while [ $nthread -le $maxThread1 ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./intruder/intruder -a10 -l128 -n262144 -s1 -1 $nthread -2 $nthread -3 $nthread -w 0 -y 1000000 -j 4000 -z 32 -t$maxThread1		
		nice -20 ./intruder/intruder -a10 -l128 -n262144 -s1 -1 $nthread -2 $nthread -3 $nthread -w 0 -y 1000000 -j 4000 -z 32 -t$maxThread1 >> 24th.txt
		
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
	echo " " >> 24th.txt
done

echo genome >> 24th.txt
nthread=2
while [ $nthread -le $maxThread1 ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./genome/genome -g16384 -s64 -n16777216 -1 $nthread -t$maxThread1
		nice -20 ./genome/genome -g16384 -s64 -n16777216 -1 $nthread -t$maxThread1 >> 24th.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
	echo " " >> 24th.txt
done

echo vacation >> 24th.txt
nthread=2
while [ $nthread -le $maxThread1 ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4194304 -1 $nthread -c$maxThread1
		nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4194304 -1 $nthread -c$maxThread1 >> 24th.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
	echo " " >> 24th.txt
done

echo kmeans >> 24th.txt
nthread=2
while [ $nthread -le $maxThread1 ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./kmeans/kmeans -m40 -n40 -t0.00001 -i kmeans/inputs/random-n65536-d32-c16.txt  -1 $nthread -w 0 -y 1000000.5 -j 1000 -z 18 -p$maxThread1
		nice -20 ./kmeans/kmeans -m40 -n40 -t0.00001 -i kmeans/inputs/random-n65536-d32-c16.txt  -1 $nthread -w 0 -y 1000000.5 -j 1000 -z 18 -p$maxThread1 >> 24th.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
	echo " " >> 24th.txt
done


echo intruder  >> 16th.txt
nthread=2
while [ $nthread -le $maxThread2 ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./intruder/intruder -a10 -l128 -n262144 -s1 -1 $nthread -2 $nthread -3 $nthread -w 0 -y 1000000 -j 4000 -z 32 -t$maxThread2		
		nice -20 ./intruder/intruder -a10 -l128 -n262144 -s1 -1 $nthread -2 $nthread -3 $nthread -w 0 -y 1000000 -j 4000 -z 32 -t$maxThread2 >> 16th.txt
		
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
	echo " " >> 16th.txt
done

echo genome >> 16th.txt
nthread=2
while [ $nthread -le $maxThread2 ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./genome/genome -g16384 -s64 -n16777216 -1 $nthread -t$maxThread2
		nice -20 ./genome/genome -g16384 -s64 -n16777216 -1 $nthread -t$maxThread2 >> 16th.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
	echo " " >> 16th.txt
done

echo vacation >> 16th.txt
nthread=2
while [ $nthread -le $maxThread2 ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4194304 -1 $nthread -c$maxThread2
		nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4194304 -1 $nthread -c$maxThread2 >> 16th.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
	echo " " >> 16th.txt
done

echo kmeans >> 16th.txt
nthread=2
while [ $nthread -le $maxThread2 ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./kmeans/kmeans -m40 -n40 -t0.00001 -i kmeans/inputs/random-n65536-d32-c16.txt  -1 $nthread -w 0 -y 1000000.5 -j 1000 -z 18 -p$maxThread2
		nice -20 ./kmeans/kmeans -m40 -n40 -t0.00001 -i kmeans/inputs/random-n65536-d32-c16.txt  -1 $nthread -w 0 -y 1000000.5 -j 1000 -z 18 -p$maxThread2 >> 16th.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
	echo " " >> 16th.txt
done

