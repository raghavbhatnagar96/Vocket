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
#include <sys/time.h> // for setitimer
#include "codec.c"
#define BUFSIZE 1024
//#define PORT "3490" // the port client will be connecting to 

#define MAXDATASIZE 8111 // max number of bytes we can get at once 


#define INTERVAL 2 //in miliseconds

/*defining global variables */
pa_simple *recordStream; //recording stream
uint8_t buf2[BUFSIZE]; //datastructure for buffer
int innum; //file discriptor for file in which we store things
int error; //error number
int clientSocket; //socket that is used by the client.
/*end of global variables*/

/*
loop_write function is used to read in a loop and send the data over to the
server it is connected to using write() function. 
fd: File parameter to record conversation if you want to.
data: buffer where the audio is stored.
size: size of buffer.
sockfd: socket with the server connection.
*/
static ssize_t loop_write(int fd, const void*data, size_t size, int sockfd) {
    ssize_t ret = 0;
    //FILE* buf = fdopen(fd, "r");
    while (size > 0) {

        ssize_t r, r2;
        if ((r = write(fd, data, size)) < 0){
            perror("send22");
        }
        
        if ((r2 = write(sockfd, data, size)) < 0){
            close(sockfd);
            printf("%d", errno);
        }

        if (r == 0)
            break;
        ret += r;
        data = (const uint8_t*) data + r;
        size -= (size_t) r;
    }
    return ret;
}


void readFromStream(int fd, const void*data, size_t size, int sockfd)
{

        int i=0;
        /* Record data ... */
        if (pa_simple_read(recordStream, buf2, sizeof(buf2), &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            //goto finish;
        }
        for(i=0;i++;i<BUFSIZE)
        {
        	buf2[i] = (uint8_t)linear2ulaw((int)buf2[i]); //ulaw encoding for voip
        }
        /* And write it to socket and file */
        if (loop_write(fd, buf2, sizeof(buf2), sockfd) != sizeof(buf2)) {
            fprintf(stderr, __FILE__": write() failed: %s\n", strerror(errno));
            //goto finish;
        }
}


/*
Signal handler for the client.
*/
void my_handler_for_sigint_sigalarm(int signumber)
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
  else if (signumber == SIGALRM)
  {
    readFromStream(innum, buf2, sizeof(buf2), clientSocket);
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
    //For function setitimer
    struct itimerval it_val; 
    it_val.it_value.tv_sec =     INTERVAL/1000;
    it_val.it_value.tv_usec =    (INTERVAL*1000) % 1000000;   
    it_val.it_interval = it_val.it_value;
    //Data structure for pulseaudio
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 2
    };

    int ret = 1;
    int error;
    //Creating recordStream using pa_simple_new
    if (!(recordStream = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        goto finish;
    }

    int sockfd;  
    //char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    FILE*input=fopen("input.dat", "w+");


    //Signal handler for sigint
    if (signal(SIGINT, my_handler_for_sigint_sigalarm) == SIG_ERR)
      printf("\ncan't catch SIGINT\n");
    if (signal(SIGALRM, my_handler_for_sigint_sigalarm) == SIG_ERR) //BIND SIGALARM
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
    //clientsocket for signal handling
    clientSocket = sockfd;
    innum = fileno(input);
    //Settimer for periodic function calling by sending SIGALARM at INTERVAL
    if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
        perror("error calling setitimer()");
        exit(1);
    }
    while(1)
        pause();

    ret = 0;
    //Close recordStream
    finish:
        if (recordStream)
            pa_simple_free(recordStream);
        return ret;
    //close socket
    close(sockfd);

    return 0;
}

