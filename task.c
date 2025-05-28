#include "common.h"
#include <sys/wait.h>
#include <sys/mman.h>
#include <ctype.h>



#define MAX_MESSAGE_SIZE 255
#define MAX_EVENTS 16


void child(char *buf)
{
    int fd[2];
    if (pipe(fd) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Pamięć współdzielona
    char *shared = mmap(NULL, MAX_MESSAGE_SIZE + 1, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED)
    {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    pid_t ret = fork();

    if (ret == 0)
    {
        // --- Proces dziecka ---
        close(fd[1]); // zamknij write
        char buff[MAX_MESSAGE_SIZE + 1] = {0};
        read(fd[0], buff, MAX_MESSAGE_SIZE);
        close(fd[0]);

        printf("Child %d: read: %s\n", getpid(), buff);

        // Zapisz do mmap
        strncpy(shared, buff, MAX_MESSAGE_SIZE);

        munmap(shared, MAX_MESSAGE_SIZE + 1);
        exit(EXIT_SUCCESS);
    }
    else
    {
        // --- Proces rodzica ---
        close(fd[0]); // zamknij read
        write(fd[1], buf, MAX_MESSAGE_SIZE);
        close(fd[1]);

        waitpid(ret, NULL, 0);

        // Odczytaj z mmap
        printf("Rodzic odczytał z mmap: %s\n", shared);

        // Liczenie słów
        int word_count = 0;
        int in_word = 0;
        for (int i = 0; shared[i] != '\0'; i++)
        {
            if (isspace(shared[i]))
            {
                if (in_word)
                {
                    word_count++;
                    in_word = 0;
                }
            }
            else
            {
                in_word = 1;
            }
        }
        if (in_word)
            word_count++;

        printf("Liczba słów: %d\n", word_count);

        munmap(shared, MAX_MESSAGE_SIZE + 1);
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



/*#include "common.h"

#define MAX_MESSAGE_SIZE 255
#define MAX_EVENTS 16

typedef struct clients_info
{
    int clients_list[10];
    int client_count;
} clients_info_t;

void add_to_epoll(int epoll_descriptor, int fd)
{
    struct epoll_event event;
    event.events = EPOLLIN;

    event.data.fd = fd;
    if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, fd, &event) == -1)
    {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }
}

void add_client(int server_fd, int epoll_descriptor, clients_info_t* clients)
{
    // new incoming connection
    int client_fd = add_new_client(server_fd, NULL, 0);
    if (client_fd < 0)
    {
        ERR("add_new_client");
    }

    printf("[%d] connected\n", client_fd);

    int new_flags = fcntl(client_fd, F_GETFL) | O_NONBLOCK;
    fcntl(client_fd, F_SETFL, new_flags);

    // register client to epoll
    add_to_epoll(epoll_descriptor, client_fd);

    clients->clients_list[clients->client_count++] = client_fd;
}

void handle_client(int client_fd, int server_fd)
{
    char buf[3];
    int bytes_already_read = 0;

    ssize_t bytes_read = read(client_fd, buf + bytes_already_read, 3 - bytes_already_read);
    if (bytes_read == 0)
    {
        printf("No, the ritual...\n");
        if (TEMP_FAILURE_RETRY(close(client_fd)) < 0)
        {
            ERR("close");
        }
    }
    else if (bytes_read < 0)
    {
        if (errno != EAGAIN)
        {
            perror("read");
            if (TEMP_FAILURE_RETRY(close(client_fd)) < 0)
            {
                ERR("close");
            }
        }
        ERR("read");
    }
    else
    {
        bytes_already_read += bytes_read;
        if (bytes_already_read >= 3)
        {
            printf("The ritual has started\n");
            if (TEMP_FAILURE_RETRY(close(client_fd)) < 0)
            {
                ERR("close");
            }
            if (TEMP_FAILURE_RETRY(close(server_fd)) < 0)
            {
                ERR("close");
            }
            return;
        }
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
    if (argc != 2)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    uint16_t port = atoi(argv[1]);

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGPIPE);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    // setting up server socket
    int server_fd = bind_tcp_socket(port, 1);
    if (server_fd < 0)
    {
        ERR("bind_tcp_socket");
    }

    printf("Server listening on port %d...\n", port);

    int epoll_descriptor;
    struct epoll_event events[MAX_EVENTS];
    // struct epoll_event events[MAX_EVENTS];

    if ((epoll_descriptor = epoll_create1(0)) < 0)
    {
        ERR("epoll_create:");
    }

    add_to_epoll(epoll_descriptor, server_fd);

    clients_info_t clients;
    clients.client_count = 0;

    int client_fd = -1;
    unsigned char header;
    unsigned char body[2];
    ssize_t have = 0;

    struct sockaddr_storage maiden_addr;
    socklen_t maiden_addr_len = sizeof(maiden_addr);

    while (1)
    {
        int nfds = epoll_wait(epoll_descriptor, events, MAX_EVENTS, -1);
        if (nfds < 0)
        {
            if (errno == EINTR)
                continue;
            ERR("epoll_wait");
        }

        for (int i = 0; i < nfds; i++)
        {
            int current_fd = events[i].data.fd;

            if (current_fd == server_fd && client_fd < 0)
            {
                // accept maiden witch
                int new_fd = add_new_client(server_fd, (struct sockaddr*)&maiden_addr, maiden_addr_len);
                if (new_fd < 0)
                {
                    continue;
                }

                client_fd = new_fd;
                int new_flags = fcntl(client_fd, F_GETFL) | O_NONBLOCK;
                fcntl(client_fd, F_SETFL, new_flags);

                add_to_epoll(epoll_descriptor, client_fd);

                clients.clients_list[clients.client_count++] = client_fd;

                // print fd
                printf("[%d] connected\n", client_fd);
                fflush(stdout);
            }
            else if (current_fd == client_fd)
            {
                // read header then body
                if (have < 1)
                {
                    ssize_t bytes_read = read(client_fd, &header, 1);
                    if (bytes_read == 0)
                    {
                        printf("No, the ritual...\n");
                        if (TEMP_FAILURE_RETRY(close(client_fd)) < 0)
                        {
                            ERR("close");
                        }
                        if (TEMP_FAILURE_RETRY(close(server_fd)) < 0)
                        {
                            ERR("close");
                        }
                        return EXIT_FAILURE;
                    }
                    else if (bytes_read < 0)
                    {
                        if (errno != EAGAIN)
                        {
                            perror("read");
                            if (TEMP_FAILURE_RETRY(close(client_fd)) < 0)
                            {
                                ERR("close");
                            }
                            if (TEMP_FAILURE_RETRY(close(server_fd)) < 0)
                            {
                                ERR("close");
                            }
                        }
                        ERR("read");
                    }

                    have = 1;
                }
                else
                {
                    ssize_t bytes_read = read(client_fd, body + (have - 1), (ssize_t)header - (have - 1));
                    if (bytes_read == 0)
                    {
                        printf("No, the ritual...\n");
                        if (TEMP_FAILURE_RETRY(close(client_fd)) < 0)
                        {
                            ERR("close");
                        }
                        if (TEMP_FAILURE_RETRY(close(server_fd)) < 0)
                        {
                            ERR("close");
                        }
                        return EXIT_FAILURE;
                    }
                    else if (bytes_read < 0)
                    {
                        if (errno != EAGAIN)
                        {
                            perror("read");
                            if (TEMP_FAILURE_RETRY(close(client_fd)) < 0)
                            {
                                ERR("close");
                            }
                            if (TEMP_FAILURE_RETRY(close(server_fd)) < 0)
                            {
                                ERR("close");
                            }
                        }
                        ERR("read");
                    }

                    have += bytes_read;
                }
            }
        }
    }

    if (have == (ssize_t)(1 + header))
    {
        // reading header and body complete
        printf("The ritual has started\n");

        uint16_t new_port;
        memcpy(&new_port, body, 2);
        new_port = ntohs(new_port);

        // connecting back
        struct sockaddr_in mother_addr;
        memset(&mother_addr, 0, sizeof(mother_addr));
        mother_addr.sin_family = AF_INET;
        memcpy(&mother_addr.sin_addr, &((struct sockaddr_in*)&maiden_addr)->sin_addr, sizeof(mother_addr.sin_addr));
        mother_addr.sin_port = htons(new_port);

        int mother_fd = make_tcp_socket();
        if (connect(mother_fd, (struct sockaddr*)&mother_addr, sizeof(mother_addr)) < 0)
        {
            ERR("connect mother");
        }

        int new_flags = fcntl(mother_fd, F_GETFL) | O_NONBLOCK;
        fcntl(mother_fd, F_SETFL, new_flags);

        add_to_epoll(epoll_descriptor, mother_fd);
    }

    if (TEMP_FAILURE_RETRY(close(client_fd)) < 0)
    {
        ERR("close");
    }
    if (TEMP_FAILURE_RETRY(close(server_fd)) < 0)
    {
        ERR("close");
    }
    for (int j = 0; j < clients.client_count; ++j)
    {
        if (TEMP_FAILURE_RETRY(close(clients.clients_list[j])) < 0)
        {
            ERR("close");
        }
    }
    return EXIT_SUCCESS;
}
*/