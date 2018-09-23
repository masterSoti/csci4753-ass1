server:
	gcc ./udp_server.c -o server
	./server 8990
client:
	gcc ./udp_client.c -o client
	./client localhost 8990
