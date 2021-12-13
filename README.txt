README

1. What does this program do
The program tries to simulate the working of a dispatcher in an OS. Client sends requests to a multi threaded socket server, from which a dispatcher takes the requests, and checks whether it falls under the limits of thread number, memory or number of files open. If it is under the limits, DLL function is invoked. The client sends which function to invoke with which arguments and the server computes the result and returns the answer to the client. The client program ends there while the server continues to listen to more requests.

2. A description of how this program works (i.e. its logic)
First the user enters the command to start the server. This starts the server socket function. This in turn calls the make named socket function, which is used to create, in this case, a server socket. The returned server socket is then used to acept client requests. An infinite loop is created, which keeps listening for client requests. If a client request comes, a new worker thread is created. Worker thread  creates a pthread, which then calls the dispatcher logic. In dispatcher logic, the server reads input sent by the client. It extracts the library, the function to be executed and its arguments.  Arguments of functions are only integers, not fractional numbers. It then uses dlopen to open the library, failure in this case returns an error message. Then dlsym looks the value of the string in the library opened by dlopen. Finally the value of computation is written to the socket. After the computation is over, socket is closed, and pthread is exited.
There are also limits put on number of threads, memory and files. Thread limit is implemented using a boolean array that remains true at a particular index for entirity of thread execution and turns false, when the pthread ends. At every instance before creating the worker thread, false values are searched in the boolen array and if no false value exists, the thread request is denied and the server keeps waiting for more reuests. 
To set file limit, setrlimit is used with RLIMIT_NOFILE and to set memory, setrlimit is used with RLIMIT_AS.
When file limit is reached, client request ends and server continues.


3. How to compile and run this program
First the file name test.c is compiled:	gcc server_client.c -lpthread -o ipc_demo -ldl

Then it is executed:	./ipc_demo server ./cs303_sock 7 100000000 3
here 
file_limit = 7
memory = 100000000 
thread_limit = 3
The format is executable file followed by sever/client followed by socket file.
If server is chosen, then following three inputs are file_limit, memory_limit, thread_limit.

Then client requests are executed:	
./ipc_demo client ./cs303_sock "/lib/x86_64-linux-gnu/libm.so.6 cos 2"

format of the string in client request is <library path function argument>

constraints: 
thread count >= 1 && < 100
file count >= 7
memory >= 100000000

DLL:
library implemented:  /lib/x86_64-linux-gnu/libm.so.6
Functions: cos, sin, tan, abs, ceil, floor, log (single argument functions of math.h)
