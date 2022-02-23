# cProxy
A caching proxy server implemented in C99
## Compilation
1. Compilation of one threaded implementation
```
./compile_non_multithreaded
```
2. Compilation of multithreaded implementation
```
./compile_multithreaded
```
## Running
1. Running one threaded implementation
```
./server.out [-h | --help] [-e | --end-to-end] 
[-l <log_level> | --log-level=<log_level>]
```
2. Running multithreaded implementation
```
./mtserver.out [-t <threads_num> | --threads=<threads_num>]
[-h | --help] [-e | --end-to-end] 
[-l <log_level> | --log_level=<log_level>]
```
