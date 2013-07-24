#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <stdio.h>
#include <queue>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <cstdlib>
#include <dirent.h>
#include <pthread.h>
#include <iostream>
#include <fcntl.h>

#define SOCKET_ERROR        -1
#define BUFFER_SIZE         100
#define MESSAGE             "This is the message I'm sending back and forth"
#define QUEUE_SIZE          1000
#define THREAD_COUNT		200
#define MAXBUFLEN			1000
#define ERROR_MESSAGE		"HTTP/1.1 404 Not Found\nContentType: text/html\n\n<HTML>404 Page Not Found</HTML>"

int pop();
void push(int);

