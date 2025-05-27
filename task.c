#include "common.h"
#include <sys/wait.h>


#define MAX_MESSAGE_SIZE 255
#define MAX_EVENTS 16


void child(char *buf)
{
    int fd[2];
    if (pipe(fd) == -1)
        ERR("pipe");

    pid_t ret = fork();
    if (ret == -1)
        ERR("fork");

    if (ret == 0) // dziecko
    {
        close(fd[1]); // zamyka zapis
        char buff[MAX_MESSAGE_SIZE + 1] = {0};
        ssize_t read_bytes = read(fd[0], buff, MAX_MESSAGE_SIZE);
        if (read_bytes > 0)
        {
            buff[read_bytes] = 0;
            printf("Child %d: read: %s\n", getpid(), buff);
        }
        close(fd[0]);
        exit(EXIT_SUCCESS); // dziecko MUSI zakończyć się
    }
    else // rodzic
    {
        close(fd[0]); // zamyka odczyt
        write(fd[1], buf, strlen(buf)); // nie trzeba MAX_MESSAGE_SIZE
        close(fd[1]); // po write trzeba zamknąć!
        waitpid(ret, NULL, 0); // czekaj tylko na tego konkretnego potomka
    }
}


void usage(char* program_name)
{
    fprintf(stderr, "Usage: \n");

    fprintf(stderr, "\t%s my_port\n", program_name);
    fprintf(stderr, "\t  my_port - the port on which the server will listen\n");

    exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    if (argc != 2)
    {
        usage(argv[0]);
    }
    uint16_t port;
    port = atoi(argv[1]);

    if (port <= 1023 || port >= 65535)
    {
        usage(argv[0]);
    }

    int server_socket = bind_tcp_socket(port, 16);

    int new_flags = fcntl(server_socket, F_GETFL) | O_NONBLOCK;
    fcntl(server_socket, F_SETFL, new_flags);

    int epoll_ds;

    if ((epoll_ds = epoll_create1(0)) < 0)
    {
        ERR("epoll_create:");
    }

   // user_context client_list[MAX_CLIENTS];
   

    struct epoll_event event, events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = server_socket;
    if (epoll_ctl(epoll_ds, EPOLL_CTL_ADD, server_socket, &event) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_storage addr;
socklen_t addr_len = sizeof(struct sockaddr_in);

    int current_connections_number = 0;

        while (1)
    {
        int ready_fds = epoll_wait(epoll_ds, events, MAX_EVENTS, -1);

        for (int i = 0; i < ready_fds; i++)
        {
            if (events[i].data.fd == server_socket)
            {
                // new user
                int client_socket = add_new_client(events[i].data.fd, (struct sockaddr*)&addr, addr_len);
                current_connections_number++;
                if(current_connections_number > 3)
                {
                    if (write(client_socket, "Server is full\n", 16) < 0)
                    {
                        if (errno != EPIPE)
                        {
                            ERR("write");
                        }
                    }
                        if (TEMP_FAILURE_RETRY(close(client_socket)) < 0)
                            ERR("close");
                    
                }
                else
                {

                    printf("[%d] connected\n", client_socket);
 

                    if (write(client_socket, "Please enter your username\n", 28) < 0)
                    {
                        if (errno != EPIPE)
                        {
                            ERR("write");
                        }
                    }

                    struct epoll_event user_event;
                    user_event.events = EPOLLIN;
                    user_event.data.fd = client_socket;
                    if (epoll_ctl(epoll_ds, EPOLL_CTL_ADD, client_socket, &user_event) == -1)
                    {
                        perror("epoll_ctl: listen_sock");
                        exit(EXIT_FAILURE);
                    }
                }
            }
            else
            {
                // message from already connected user

                char buf[MAX_MESSAGE_SIZE + 1] = {0};
                int read_chars;
                memset(buf, 0, sizeof(buf));
                read_chars = read(events[i].data.fd, buf, MAX_MESSAGE_SIZE);
                if (read_chars == 0)
                {
                    printf("xdddd\n");
                    if (TEMP_FAILURE_RETRY(close(events[i].data.fd)) < 0)
                        ERR("close");
                }

                buf[strcspn(buf, "\r\n")] = 0;

                if (strncmp(buf, "quit", 4) == 0)
                {

                    printf("Client is quting\n");
                          if (TEMP_FAILURE_RETRY(close(events[i].data.fd)) < 0)
                            ERR("close");
                    current_connections_number--;
                    continue;;
                }


                if (read_chars < 0)
                {
                    ERR("read");
                }

                child(buf);
                //printf("%s\n", buf);
            }
        }
    }

    if (TEMP_FAILURE_RETRY(close(server_socket)) < 0)
        ERR("close");

    return EXIT_SUCCESS;
}