compile:
	g++ -Wall -Wextra -O2 -g server.cpp hashtable.cpp utils.cpp -o server
	g++ -Wall -Wextra -O2 -g client.cpp utils.cpp -o client

clean:
	rm client server

test:
	g++ -Wall -Wextra -O2 -g test_avl.cpp -o test_avl
	./test_avl

.PHONY: test clean