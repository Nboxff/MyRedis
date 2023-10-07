## Redis mini Lab01

### Task 

Write 2 code files: hello_server.cpp and hello_client.cpp. The first program is a server, it accepts connections from clients, reads a single message, and writes a single reply. The second program is a client, it connects to the server, writes a single message, and reads a single reply. Let’s start with the server first.

### Compile and Run
Compile our programs with the following command line:

```bash
g++ -Wall -Wextra -O2 -g server.cpp -o server
g++ -Wall -Wextra -O2 -g client.cpp -o client
```

Or, you can use following command line

```bash
make compile
```

Run `./server` in a window and then run `./client` in another window. You should see the following results:

```bash
$ ./server
client says: hello
$ ./client
server says: world
```