run: server.o threadpool.o
	gcc server.o threadpool.o -o run -lpthread
	
server.o: server.c threadpool.h
	gcc server.c -Wall -c
	
threadpool.o: threadpool.c threadpool.h
	gcc threadpool.c -Wall -c

clean: 
	rm server.o threadpool.o run
