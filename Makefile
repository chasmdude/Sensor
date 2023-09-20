run :
	@echo -e '\n*******************************'
	@echo -e '*** Compiling for TEST ***'
	@echo -e '*******************************'
	clear
	gcc main.c sbuffer.c connection_manager.c data_operator.c storage_manager.c lib/tcpsock.c lib/dplist.c -lsqlite3 -pthread  -o main -Wall -std=gnu11 -Werror -DSET_MAX_TEMP=18 -DSET_MIN_TEMP=15 -DTIMEOUT=5 -DPORT=1234
	gcc sensor_node.c lib/tcpsock.c lib/dplist.c -o client
	./main
debug :
	@echo -e '\n*******************************'
	@echo -e '*** Compiling for DEBUG ***'
	@echo -e '*******************************'
	clear
	gcc main.c sbuffer.c connection_manager.c data_operator.c storage_manager.c lib/tcpsock.c lib/dplist.c -lsqlite3 -pthread  -o main -Wall -std=gnu11 -Werror -DSET_MAX_TEMP=18 -DSET_MIN_TEMP=15 -DTIMEOUT=5 -DPORT=1234 -DDEBUG_PRINT=1 -g
	gcc sensor_node.c lib/tcpsock.c lib/dplist.c -o client
	gdb ./main