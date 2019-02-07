# Inizializzo le variabili relative ai parametri passati in ingresso allo script

#!/bin/bash

maxThread=32
runPerThread=4

nthread=1
array=(0 4 8 12 16 20 24 28 1 5 9 13 17 21 25 29 2 6 10 14 18 22 26 30 3 7 11 15 19 23 27 31 0 4 8 12 16 20 24 28 1 5 9 13 17 21 25 29 2 6 10 14 18 22 26 30 3 7 11 15 19 23 27 31)
while [ $nthread -lt $maxThread ]
	do
	k=0
	while [ $k -lt $runPerThread ]
		do
	        echo $nthread
	        j=1
	        affinity=0
	        while [ $j -lt $nthread ]
		do
	                affinity=$(echo $affinity,${array[$j]})
	                j=$[$j+1]
	        done
		# kmeans nice -20 taskset -c $affinity ./kmeans -m15 -n15 -t0.00001 -i inputs/random-n65536-d32-c16.txt -p$nthread > ris0.txt
		# intruder nice -20 taskset -c $affinity ./intruder -a10 -l128 -n262144 -s1 -t$nthread  > ris0.txt
		echo nice -20 taskset -c $affinity ./vacation -n4 -q60 -u90 -r1048576 -t4194304 -c$nthread
		nice -20 taskset -c $affinity ./vacation -n4 -q60 -u90 -r1048576 -t4194304 -c$nthread > ris0.txt

		grep "Time" ris0.txt|sed -E -e "s/[a-zA-Z =:]*//g" > ris1.txt
		grep "Starts" ris0.txt|sed -E -e "s/([a-zA-Z0-9= ]*)Starts=([0-9]*) Aborts[= a-zA-Z0-9]*/\2/" > ris2.txt
		grep "Starts" ris0.txt|sed -E -e "s/([a-zA-Z0-9= ]*)Aborts=([0-9]*)/\2/" > ris3.txt
		exec 4<ris1.txt
		read -u 4 tempo
		exec 5<ris2.txt
		read -u 5 transazioni
		exec 6<ris3.txt
		read -u 6 abort
		echo $nthread,$tempo,$transazioni,$abort >> risultati.csv 

		k=$[$k+1]
	done
	nthread=$[$nthread+1]
done


#ciclo:
#	Avvio il benchmark
#	intercetto la fine del benchmark
#	faccio il parsing dell'output
#	salvo i risultati su file
#	genero nuovi parametri per il benchmark
#fine ciclo
