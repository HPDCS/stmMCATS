#!/bin/bash


runPerThread=8
maxThread=24
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./kmeans/kmeans -m40 -n40 -t0.00001 -i kmeans/inputs/random-n65536-d32-c16.txt  -1 $nthread -w 0 -y 1000000.5 -j 1000 -z 18 -p$nthread
		nice -20 ./kmeans/kmeans -m40 -n40 -t0.00001 -i kmeans/inputs/random-n65536-d32-c16.txt  -1 $nthread -w 0 -y 1000000.5 -j 1000 -z 18 -p$nthread >> kmeans_n_i.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done

