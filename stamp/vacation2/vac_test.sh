# Inizializzo le variabili relative ai parametri passati in ingresso allo script

#!/bin/bash

export LEARNING=true
export STM_STATS=true

runPerThread=1
run=1
array=(0 4 8 12 16 20 24 28 1 5 9 13 17 21 25 29 2 6 10 14 18 22 26 30 3 7 11 15 19 23 27 31)
	affinity=0
	nrun=1
	nthread=17

while [ $nthread -lt $nrun ]
	do
 	j=1
	while [ $j -lt $nthread ]
	do
		affinity=$(echo $affinity,${array[$j]})
	        j=$[$j+1]
	done
	echo $nthread
	nice -20 taskset -c $affinity ./vacation -n4 -q60 -u90 -r1048576 -t4194304 -c$nthread > ris0.txt
	grep "Time =" ris0.txt|sed -E -e "s/[a-zA-Z =:]*//g" > ris1.txt
	exec 4<ris1.txt
	read -u 4 tempo
	echo $nthread,$tempo >> risultati.txt
	nthread=$[$nthread+1]
done


#ciclo:
#	Avvio il benchmark
#	intercetto la fine del benchmark
#	faccio il parsing dell'output
#	salvo i risultati su file
#	genero nuovi parametri per il benchmark
#fine ciclo
