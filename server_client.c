#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/un.h>
#include <stddef.h>
#include <dlfcn.h>
#include <sys/resource.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAXN 100 //max limit of number of threads
bool create_worker_thread(int fd, int index); 
bool thread_available[MAXN]; //boolean array which checks if more threads are allowed or not
int threadN = 0; //number of threads, taken input from user as CLI

struct arg_dispatcher{ //structure to pass multiple arguments to dispatcher_logic function
    int arg1;
    int arg2;
};

/**
 * This code is adapted from the samples available at:
 * https://opensource.com/article/19/4/interprocess-communication-linux-networking and
 * https://www.gnu.org/software/libc/manual/html_node/Local-Socket-Example.html
 *
 * Compile it using: gcc local_socket_client_server.c -lpthread -o ipc_demo
 
 * Needless to say, this code is not perfect and may have some subtle bugs. A purpose
 * if this code is to show how to write a socket based client server program that
 * off-loads the client connection to a new thread for processing.
 */

void log_msg(const char *msg, bool terminate) {
    printf("%s\n", msg);
    if (terminate) exit(-1); /* failure */
}

/**
 * Create a named (AF_LOCAL) socket at a given file path.
 * @param socket_file
 * @param is_client whether to create a client socket or server socket
 * @return Socket descriptor
 */
int make_named_socket(const char *socket_file, bool is_client) {
    printf("Creating AF_LOCAL socket at path %s\n", socket_file);
    if (!is_client && access(socket_file, F_OK) != -1) {
        log_msg("An old socket file exists, removing it.", false);
        if (unlink(socket_file) != 0) {
            log_msg("Failed to remove the existing socket file.", true);
        }
    }
    struct sockaddr_un name;
    /* Create the socket. */
    int sock_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        log_msg("Failed to create socket.", true);
    }

    /* Bind a name to the socket. */
    name.sun_family = AF_LOCAL;
    strncpy (name.sun_path, socket_file, sizeof(name.sun_path));
    name.sun_path[sizeof(name.sun_path) - 1] = '\0';

    /* The size of the address is
       the offset of the start of the socket_file,
       plus its length (not including the terminating null byte).
       Alternatively you can just do:
       size = SUN_LEN (&name);
   */
    size_t size = (offsetof(struct sockaddr_un, sun_path) +
                   strlen(name.sun_path));
    if (is_client) {
        if (connect(sock_fd, (struct sockaddr *) &name, size) < 0) {
            log_msg("connect failed", 1);
        }
    } else {
        if (bind(sock_fd, (struct sockaddr *) &name, size) < 0) {
            log_msg("bind failed", 1);
        }
    }
    return sock_fd;
}

/**
 * Starts a server socket that waits for incoming client connections.
 * @param socket_file
 * @param max_connects
 */
_Noreturn void start_server_socket(char *socket_file, int max_connects) {
    int sock_fd = make_named_socket(socket_file, false);

    /* listen for clients, up to MaxConnects */
    if (listen(sock_fd, max_connects) < 0) {
        log_msg("Listen call on the socket failed. Terminating.", true); /* terminate */
    }
    log_msg("Listening for client connections...\n", false);
    /* Listens indefinitely */
    while (1) {
        struct sockaddr_in caddr; /* client address */
        int len = sizeof(caddr);  /* address length could change */

        printf("Waiting for incoming connections...\n");
        int client_fd = accept(sock_fd, (struct sockaddr *) &caddr, &len);  /* accept blocks */

        if (client_fd < 0) {
            log_msg("accept() failed. Continuing to next.", 0); /* don't terminate, though there's a problem */
            continue;
        }
        /*check if thread limit has reached or not*/
        int index = 0;
        bool ch = true;
        for(int i=0;i<threadN;i++){
            if (thread_available[i] == false){
                index = i;
                ch = false;
                thread_available[i] = true;
                break;
            }
        }
        if (ch == true){
            log_msg("Failed to create worker thread due to thread limit reached. Continuing to next.", 0);
            continue;
        }
        /* Start a worker thread to handle the received connection. */
        if (!create_worker_thread(client_fd, index)) {
            log_msg("Failed to create worker thread. Continuing to next.", 0);
            continue;
        }

    }  /* while(1) */
}


/**
 * This functions is executed in a separate thread.
 * @param sock_fd
 */
void dispatcher_logic(void *arguments) {
    // 0. Check limits on memory and file
    // 1. Extract the DLL invocation request info from client socket
    // 2. Load the DLLs
    // 3. Prepare the function params
    // 4. Invoke the DLL function with the above params
    struct arg_dispatcher *args = (struct arg_dispatcher *) arguments;
    int sock_fd = args->arg1;
    int index = args->arg2;
    log_msg("SERVER: dispatcher_logic: starting", false);
    char buffer[5000];
    memset(buffer, '\0', sizeof(buffer));
    int count = read(sock_fd, buffer, sizeof(buffer));
    sleep(2);
    if (count > 0) {
        printf("SERVER: Received from client: %s\n", buffer);
        //write(sock_fd, buffer, sizeof(buffer)); /* echo as confirmation */
        void *h;
        double (*c)(double);
        char *error;
        char *t1;
        char *t2;
        char *t3;
        t1 = strtok(buffer, " ");
        t2 = strtok(NULL, " ");
        t3 = strtok(NULL, " ");
        //printf("%s %s %s\n", t1, t2, t3);
        h = dlopen(t1, RTLD_LAZY);
        if (!h){
            char *er = "error";
            printf("ERROR in opening dll\n");
            write(sock_fd, er, sizeof(er));
            close(sock_fd); /* break connection */
            log_msg("SERVER: dispatcher_logic: Done. Worker thread terminating.", false);
            thread_available[index] = false; // space is available at index after end of thread execution
            pthread_exit(NULL); // Must be the last statement
            return;
        }
        c = dlsym(h, t2);
        if ((error = dlerror())!=NULL){
            char *er = "error";
            printf("ERROR in finding function\n");
            write(sock_fd, er, sizeof(er));
            close(sock_fd); /* break connection */
            log_msg("SERVER: dispatcher_logic: Done. Worker thread terminating.", false);
            thread_available[index] = false; // space is available at index after end of thread execution
            pthread_exit(NULL); // Must be the last statement
            return;
        }
        int arg = atoi(t3);
        //printf("SERVER: Received from client: %f\n", (*c)(arg));
        char res[50];
        sprintf(res, "%f", (*c)(arg));
        write(sock_fd, res, sizeof(res)); /* echo as confirmation */
    }
    close(sock_fd); /* break connection */
    log_msg("SERVER: dispatcher_logic: Done. Worker thread terminating.", false);
    thread_available[index] = false; // space is available at index after end of thread execution
    pthread_exit(NULL); // Must be the last statement
}

/**
 * This function launches a new worker thread.
 * @param sock_fd
 * @return Return true if thread is successfully created, otherwise false.
 */
bool create_worker_thread(int sock_fd, int index) {
    log_msg("SERVER: Creating a worker thread.", false);
    pthread_t thr_id;
    struct arg_dispatcher *args = malloc(sizeof(struct arg_dispatcher));
    args->arg1 = sock_fd;
    args->arg2 = index;
    int rc = pthread_create(&thr_id,
            /* Attributes of the new thread, if any. */
                            NULL,
            /* Pointer to the function which will be
             * executed in new thread. */
                            dispatcher_logic,
            /* Argument to be passed to the above
             * thread function. */
                            (void *) args);
    if (rc) {
        log_msg("SERVER: Failed to create thread.", false);
        return false;
    }
    return true;
}

/**
 * Sends a message to the server socket.
 * @param msg Message to send
 * @param socket_file Path of the server socket on localhost.
 */
void send_message_to_socket(char *msg, char *socket_file) {
    int sockfd = make_named_socket(socket_file, true);

    /* Write some stuff and read the echoes. */
    log_msg("CLIENT: Connect to server, about to write some stuff...", false);
    if (write(sockfd, msg, strlen(msg)) > 0) {
        /* get confirmation echoed from server and print */
        char buffer[5000];
        memset(buffer, '\0', sizeof(buffer));
        if (read(sockfd, buffer, sizeof(buffer)) > 0) {
            printf("CLIENT: Received from server:: %s\n", buffer);
        }
    }
    log_msg("CLIENT: Processing done, about to exit...", false);
    close(sockfd); /* close the connection */
}


/**
 * This is the driver function you can use to test client-server
 * communication using sockets.
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s [server|client] [Local socket file path] [Message to send (needed only in case of client)]\n",
               argv[0]);
        exit(-1);
    }
    if (0 == strcmp("server", argv[1])) {
        struct rlimit lim_files;
        struct rlimit lim_memory;
        lim_files.rlim_cur = atoi(argv[3]);
        lim_files.rlim_max = 1024;
        
        if (setrlimit(RLIMIT_NOFILE, &lim_files) == -1){
            printf("Error in setting file limit\n");
            return 0;
        }
        threadN = atoi(argv[5]);
        lim_memory.rlim_cur = atoi(argv[4]);//1000*100000
        lim_memory.rlim_max = 1000*(long unsigned)3000000;
        //getrlimit(RLIMIT_AS, &lim_memory);
        //printf("%ld %ld\n", lim_memory.rlim_cur, lim_memory.rlim_max);
        if (setrlimit(RLIMIT_AS, &lim_memory) == -1){
            printf("Error in setting memory limit\n");
            return 0;
        }
        for(int i=0;i<MAXN;i++) thread_available[i] = false; // set active thread checks to false
        start_server_socket(argv[2], 10);
    } else {
        send_message_to_socket(argv[3], argv[2]);
    }
}