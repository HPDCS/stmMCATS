# Inizializzo le variabili relative ai parametri passati in ingresso allo script

#!/bin/bash

#export LEARNING=true
export STM_STATS=tru

hidden=8
array=(0 4 8 12 16 20 24 28 1 5 9 13 17 21 25 29)
while [ $hidden -lt 33 ]
do
run=1
while [ $run -lt 11 ]
do
echo $hidden,$run >> risultati.txt
maxThread=17
runPerThread=1
nthread=1
while [ $nthread -lt $maxThread ]
	do
	echo $nthread
	j=1
	affinity=0
	while [ $j -lt $nthread ]
	do
		affinity=$(echo $affinity,${array[$j]})
	        j=$[$j+1]
	done
	cp ./out_$[$hidden]_$[$run].net ./nn.net
	nice -20 taskset -c $affinity ./vacation -n4 -q60 -u90 -r32768 -t524288 -c$nthread > ris0.txt
	grep "Time = " ris0.txt|sed -E -e "s/[a-zA-Z =:]*//g" > ris1.txt
	exec 4<ris1.txt
	read -u 4 tempo
	echo $nthread,$tempo >> risultati.txt
	nthread=$[$nthread+1]
done
run=$[$run+1]
done
hidden=$[$hidden*2]
done

#ciclo:
#	Avvio il benchmark
#	intercetto la fine del benchmark
#	faccio il parsing dell'output
#	salvo i risultati su file
#	genero nuovi parametri per il benchmark
#fine ciclo
