# Inizializzo le variabili relative ai parametri passati in ingresso allo script

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

echo vacation12++ >> results_analysis.txt
maxThread=12
nthread=4
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		#echo nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4194304 -1 $nthread -w 0 -y 1000000 -j 4000 -z 32 -c12 			
		#nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4194304 -1 $nthread -w 0 -y 1000000 -j 4000 -z 32 -c12 >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done

echo vacation32+ >> results_analysis.txt
maxThread=32
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		#echo nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4096-1 $nthread -w 0 -y 1000000 -j 100 -z 32 -c32 			
		#nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4096 -1 $nthread -w 0 -y 1000000 -j 100 -z 32 -c32 >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done

echo vacation16++ >> results_analysis.txt
maxThread=16
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		#echo nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4194304-1 $nthread -w 0 -y 1000000 -j 100 -z 32 -c16 			
		#nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4194304 -1 $nthread -w 0 -y 1000000 -j 100 -z 32 -c16 >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done

echo vacation24+ >> results_analysis.txt
maxThread=24
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		#echo nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4096-1 $nthread -w 0 -y 1000000 -j 100 -z 32 -c24 			
		#nice -20 ./vacation/vacation -n4 -q60 -u90 -r1048576 -t4096 -1 $nthread -w 0 -y 1000000 -j 100 -z 32 -c24 >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done



echo kmeans24+ >> results_analysis.txt
maxThread=24
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		echo nice -20 ./kmeans/kmeans -m15 -n15 -t0.05 -i kmeans/inputs/random-n16384-d24-c16.txt  -1 $nthread -w 0 -y 1000000.5 -j 1000 -z 24 -p$nthread
		nice -20 ./kmeans/kmeans -m15 -n15 -t0.05 -i kmeans/inputs/random-n16384-d24-c16.txt  -1 $nthread -w 0 -y 1000000.5 -j 1000 -z 24 -p$nthread >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
	echo " " >> results_analysis.txt
done

echo kmeans18+ >> results_analysis.txt
maxThread=18
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		#echo nice -20 ./kmeans/kmeans -m15 -n15 -t0.05 -i kmeans/inputs/random-n16384-d24-c16.txt  -1 $nthread -w 0 -y 1000000.5 -j 1000 -z 18 -p18
		#nice -20 ./kmeans/kmeans -m15 -n15 -t0.05 -i kmeans/inputs/random-n16384-d24-c16.txt  -1 $nthread -w 0 -y 1000000.5 -j 1000 -z 18 -p18 >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done

echo kmeans24++ >> results_analysis.txt
maxThread=24
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		#echo nice -20 ./kmeans/kmeans -m40 -n40 -t0.00001 -i kmeans/inputs/random-n65536-d32-c16.txt  -1 $nthread -w 0 -y 1000000.5 -j 1000 -z 24 -p24
		#nice -20 ./kmeans/kmeans -m40 -n40 -t0.00001 -i kmeans/inputs/random-n65536-d32-c16.txt  -1 $nthread -w 0 -y 1000000.5 -j 1000 -z 24 -p24 >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done

echo kmeans18++ >> results_analysis.txt
maxThread=18
nthread=1
while [ $nthread -le $maxThread ] 
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
		#echo nice -20 ./kmeans/kmeans -m40 -n40 -t0.00001 -i kmeans/inputs/random-n65536-d32-c16.txt  -1 $nthread -w 0 -y 1000000.5 -j 1000 -z 18 -p18
		#nice -20 ./kmeans/kmeans -m40 -n40 -t0.00001 -i kmeans/inputs/random-n65536-d32-c16.txt  -1 $nthread -w 0 -y 1000000.5 -j 1000 -z 18 -p18 >> results_analysis.txt
		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done









