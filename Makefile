CXX=g++
CFLAGS=-Wall -Werror

run_all_tests: test1 test2 test3 test4 test5

test1: all
	@echo "\nRunning test 1"
	./client/tcp_client < tests/test1.in || echo "Expected pass"

test2: all 
	@echo "\nRunning negative test 2"
	./client/tcp_client < tests/test2.in || echo "Expected fail"

test3: all 
	@echo "\nRunning negative test 3"
	./client/tcp_client < tests/test2.in || echo "Expected fail"

test4: all 
	@echo "\nRunning test 4"
	./client/tcp_client < tests/test4.in || echo "Expected pass"

test5: all 
	@echo "\nRunning negative test 5"
	./client/tcp_client < tests/test5.in || echo "Expected fail"

all: client server

client: tcp_client_obj

tcp_client_obj: tcp_client_asm
	$(CXX) $(CFLAGS) -C client/tcp_client.s -o client/tcp_client

client_dbg: tcp_client_asm
	$(CXX) $(CFLAGS) -g -C client/tcp_client.s -o client/tcp_client

tcp_client_asm: tcp_client_preproc
	$(CXX) $(CFLAGS) -S client/tcp_client.ii -o client/tcp_client.s 

tcp_client_preproc: tcp_client.C
	mkdir -p client
	$(CXX) $(CFLAGS) -E tcp_client.C -o client/tcp_client.ii

clean_client:
	rm -rf client

run_server: server
	./server/tcp_server

server: tcp_server_obj

tcp_server_obj: tcp_server_asm
	$(CXX) $(CFLAGS) -C server/tcp_server.s -o server/tcp_server

server_dbg: tcp_server_asm
	$(CXX) $(CFLAGS) -g -C server/tcp_server.s -o server/tcp_server

tcp_server_asm: tcp_server_preproc
	$(CXX) $(CFLAGS) -S server/tcp_server.ii -o server/tcp_server.s

tcp_server_preproc: tcp_server.C
	mkdir -p server
	$(CXX) $(CFLAGS) -E tcp_server.C -o server/tcp_server.ii

clean_server:
	rm -rf server

clean: clean_server clean_client

