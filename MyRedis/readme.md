## Redis mini Lab02

### Task 

### Compile and Run
Compile our programs with the following command line:

```bash
make compile
```

Run `./server` in a window and then run `./client` in another window. You should see the following results:

```bash
$ ./server
```

```bash
$ ./client get k
server says: [2]
$ ./client set k v
server says: [0]
$ ./client get k
server says: [0] v
$ ./client del k
server says: [0]
$ ./client get k
server says: [2]
$ ./client aaa bbb
server says: [1] Unknown cmd
```