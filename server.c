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
#define PORT "3490"  // the port users will be connecting to
#define BUFSIZE 1024
#define BACKLOG 10     // how many pending connections queue will hold

#define MAXDATASIZE 8111

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


static ssize_t loop_read(int fd, void*data, size_t size) {
    ssize_t ret = 0;
    while (size > 0) {
        ssize_t r;
        if ((r = read(fd, data, size)) < 0)
            return r;
        if (r == 0)
            break;
        ret += r;
        data = (uint8_t*) data + r;
        size -= (size_t) r;
    }
    return ret;
}



int main(int argc, char *argv[])
{
    
    int sockfd, new_fd, numbytes;  // listen on sock_fd, new connection on new_fd
    //uint8_t *buf;
    uint8_t buf[BUFSIZE];
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    
    if (argc != 2) {
        fprintf(stderr,"usage: ./server <portnumber>\n");
        exit(1);
    }
    FILE*output=fopen("output.wav", "w+");
    int outnum = fileno(output);


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
        FILE*inpu = fopen("output.dat", "w+");
        int inpuno = fileno(inpu);
        /*Whenever the server accepts any incoming connection, it forks a new child process*/
        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            ///////////play////////////
            static const pa_sample_spec ss = {
                .format = PA_SAMPLE_S16LE,
                .rate = 44100,
                .channels = 2
            };
            
            pa_simple *playStream = NULL;
            int ret = 1;
            int error;
            if (!(playStream = pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
                fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
                goto finish;
            }
            /////////end//////////////
            for(;;){    
                ssize_t r, r2;
                r2 = read(new_fd, buf, sizeof(buf));

                ///////play///////////
                write(inpuno, buf, sizeof(buf));
                if (pa_simple_write(playStream, buf, (size_t) r2, &error) < 0) {
                    fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
                    goto finish;
                }
                ////////end//////////

                if(strcmp((char*)buf, "End of Connection")==0){ 
                    close(new_fd);
                    printf("Ending connection");
                    exit(0);
                }

            }
            fclose(inpu);
            finish:
                if (playStream)
                    pa_simple_free(playStream);
                return ret;
        }
        close(new_fd);  // parent doesn't need this
    }
   
    return 0;

}

