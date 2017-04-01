/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <sys/time.h> //for setitimer
#include "codec.c"
#define PORT "3490"  // the port users will be connecting to
#define BUFSIZE 1024
#define BACKLOG 10     // how many pending connections queue will hold
#define INTERVAL 1 //nanoseconds
#define MAXDATASIZE 8111
/*Define Global Variables*/
pa_simple *playStream;
int inpuno, new_fd, error;
uint8_t buf[BUFSIZE];
int ret;

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

void readFromSocket(int filefd, uint8_t*buf, int sockfd)
{
    ssize_t r2;
    int i=0;
    r2 = read(sockfd, buf, sizeof(buf));
    for(i=0;i++;i<BUFSIZE)
    {
        buf[i] = (uint8_t)linear2ulaw((int)buf[i]); //decodeing
    }
    //Write what you recieve to a file
    write(filefd, buf, sizeof(buf));
    //Create a play stream
    if (pa_simple_write(playStream, buf, (size_t) r2, &error) < 0) {
        //fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
        //goto finish;
        close(sockfd);
        printf("Ending connection\n");
        exit(0);     
        if (playStream)
            pa_simple_free(playStream);
            //ret = 9;
    }
    //End connection
    if(strcmp((char*)buf, "End of Connection")==0){ 
        close(sockfd);
        printf("Ending connection");
        if (playStream)
            pa_simple_free(playStream);
            ret = 9;
        exit(0);
    }
}

/*
Signal handler for the client.
*/
void my_handler_for_sigalarm(int signumber)
{
  if ((signumber == SIGALRM)&&(ret!=9))
  {
    readFromSocket(inpuno, buf, new_fd);
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
    it_val.it_value.tv_sec =     INTERVAL/10000;
    it_val.it_value.tv_usec =    (INTERVAL) % 1000000;   
    it_val.it_interval = it_val.it_value;

    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    int sockfd;
    if (signal(SIGALRM, my_handler_for_sigalarm) == SIG_ERR) //BIND SIGALARM
      printf("\ncan't catch SIGINT\n");
    if (argc != 2) {
        fprintf(stderr,"usage: ./server <portnumber>\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) { //bind port
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) { //start listening to start chats.
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        FILE*inpu = fopen("output.dat", "w+");
        inpuno = fileno(inpu);
        ret = 1;
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        /*Whenever the server accepts any incoming connection, it forks a new child process*/
        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            
            //Data structure for pulseAudio
            static const pa_sample_spec ss = {
                .format = PA_SAMPLE_S16LE,
                .rate = 44100,
                .channels = 2
            };   
            
            //pa_simple *playStream = NULL;
            int error;
            if (!(playStream = pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
                fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
                goto finish;
            }
            if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
                perror("error calling setitimer()");
                exit(1);
            }
            while(1)
                pause();

            fclose(inpu);
            //Finish to close playStream
            finish:
                if (playStream)
                    pa_simple_free(playStream);
                return ret;
        }
        close(new_fd);  // parent doesn't need this
    }
   
    return 0;

}

