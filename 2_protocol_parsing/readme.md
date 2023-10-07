## Redis mini Lab02

### Task 

### Compile and Run
Compile our programs with the following command line:

```bash
g++ -Wall -Wextra -O2 -g server.cpp utils.cpp -o server
g++ -Wall -Wextra -O2 -g client.cpp utils.cpp -o client
```

Or, you can use following command line

```bash
make compile
```

Run `./server` in a window and then run `./client` in another window. You should see the following results:

```bash
$ ./server
client says: hello1
client says: hello2
client says: hello3
EOF

$ ./client
server says: world
server says: world
server says: world
```