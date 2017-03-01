/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#define BUFSIZE 1024
//#define PORT "3490" // the port client will be connecting to 

#define MAXDATASIZE 8111 // max number of bytes we can get at once 
int clientSocket;

static ssize_t loop_write(int fd, const void*data, size_t size, int sockfd) {
    ssize_t ret = 0;
    //FILE* buf = fdopen(fd, "r");
    while (size > 0) {
        // char * clop=(char*)data;
        // printf("%s", clop);
        ssize_t r;
        // if ((r = write(fd, data, size)) < 0){
        //     perror("send22");
        // }
        if ((r=write(sockfd, data, size)) < 0){
            close(sockfd);
            printf("%d", errno);
            //printf("%s", data);
            //perror("send11");
        }
        if (r == 0)
            break;
        ret += r;
        data = (const uint8_t*) data + r;
        size -= (size_t) r;
    }
    return ret;
}



void my_handler_for_sigint(int signumber)
{
  char ans[2];
  if (signumber == SIGINT)
  {
    printf("received SIGINT\n");
    printf("Program received a CTRL-C\n");
    printf("Terminate Y/N : "); 
    scanf("%s", ans);
    if ((strcmp(ans,"Y") == 0)||(strcmp(ans,"y") == 0))
    {
       printf("Exiting ....\n");
       if (send(clientSocket, "End of Connection", MAXDATASIZE-1, 0) == -1){
            close(clientSocket);
            perror("send");
        }
       close(clientSocket);
       exit(0); 
    }
    else
    {
       printf("Continung ..\n");
    }
  }
}



// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 2
    };

    pa_simple *s2 = NULL;
    int ret = 1;
    int error;

    if (!(s2 = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        goto finish;
    }

    int sockfd;  
    //char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    FILE*input=fopen("input.wav", "a+");


//Signal handler for sigint
    if (signal(SIGINT, my_handler_for_sigint) == SIG_ERR)
      printf("\ncan't catch SIGINT\n");



    if (argc != 3) {
        fprintf(stderr,"usage: ./client <hostname> <portnumber>\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }
    

    //error//
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);

    printf("client: connecting to %s\n", s);
    //printf("ZSx");
    freeaddrinfo(servinfo); // all done with this structure
    clientSocket = sockfd;///////////////////////////////signal handling////////////////////////
    int innum = fileno(input);
    
    while(1){
        // scanf("%s", buf);
        // if (send(sockfd, buf, MAXDATASIZE-1, 0) == -1){
        //     close(sockfd);
        //     perror("send");
        
        uint8_t buf2[BUFSIZE];
        /* Record some data ... */
        
        if (pa_simple_read(s2, buf2, sizeof(buf2), &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            goto finish;
        }
        /* And write it to STDOUT */

        if (loop_write(innum, buf2, sizeof(buf2), sockfd) != sizeof(buf2)) {
            fprintf(stderr, __FILE__": write() failed: %s\n", strerror(errno));
            goto finish;
        }
    }
    ret = 0;
    finish:

    if (s2)
        pa_simple_free(s2);
    return ret;

    close(sockfd);

    return 0;
}

