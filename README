*Introduction*
MCATS (Markov Chain-based Adaptive Transaction Scheduler) is an adaptive transaction scheduler for Transactional Memory (TM). The objective of MCATS is the one of maximizing the system throughput by dynamically regulating the maximum number of concurrent transactions admitted to run along the lifetime of the STM application. MCATS exploits a TM performance model based on Markov Chain. 

*Compile*
- You can use "compile.sh" to compile TinySTM (with MCATS) and three applications of STAMP (Intruder, Yada and Vacation)

*MCATS configuration*
- MCATS requires the configuration file "mcast_conf.txt" in the path where the application is executed. The file should containts the following line:
"TX_PER_CYCLE=X INITIAL_MAX_ADMITTED_TX=Y",
where
- X is the length of MCATS observation window in term of subsequect committed transactions ny a thread
- Y is initial number of maximum admitted transactions by the scheduler  

*Run*
To run one of the applications of STAMP, please, refer to "README" file inlcuded in the application code path        

	
	
