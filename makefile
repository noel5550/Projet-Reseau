all: client server
client: 
	$ gcc client1.c -lpthread -std=gnu99
	./a.out localhost

server:
	$ gcc -o server server1.c  
	./server 
