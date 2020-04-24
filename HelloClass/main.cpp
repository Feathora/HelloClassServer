#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string>
#include <vector>

static const std::string WELCOME_MESSAGE = "Annie says Hello!";

std::vector<int> client_sockets;
pthread_mutex_t client_sockets_mutex;

void error(const char* msg)
{
    perror(msg);
    exit(1);
}

void broadcast_message(int sender, char* message, ssize_t length)
{
    pthread_mutex_lock(&client_sockets_mutex);
    for (auto& client : client_sockets)
    {
        if (sender == client) continue;

        send(client, message, length, 0);
    }
    pthread_mutex_unlock(&client_sockets_mutex);
}

void* handle_client(void* data)
{
    int socket = (long)data;

    pthread_mutex_lock(&client_sockets_mutex);
    client_sockets.push_back(socket);
    pthread_mutex_unlock(&client_sockets_mutex);

    send(socket, WELCOME_MESSAGE.c_str(), WELCOME_MESSAGE.size() + 1, 0);

    char buffer[1024];
    while (true)
    {
        bzero(buffer, 1024);
        ssize_t status = recv(socket, buffer, 1023, 0);

        if (status <= 0)
        {
            std::cout << "SERVER >> CLIENT DISCONNECTED" << std::endl;

            shutdown(socket, SHUT_RDWR);
            close(socket);

            pthread_mutex_lock(&client_sockets_mutex);
            for (size_t i = 0; i < client_sockets.size(); ++i)
            {
                if (client_sockets[i] != socket) continue;

                client_sockets.erase(client_sockets.begin() + i);
                break;
            }
            pthread_mutex_unlock(&client_sockets_mutex);

            break;
        }

        std::cout << buffer << std::endl;
        broadcast_message(socket, buffer, status);
    }

    pthread_exit(nullptr);
}

void handle_connections(int socket)
{
    while (true)
    {
        sockaddr_storage their_addr;
        socklen_t sin_size = sizeof(their_addr);

        int accepted_socket = accept(socket, (sockaddr*)&their_addr, &sin_size);

        pthread_t client_thread;
        pthread_create(&client_thread, nullptr, handle_client, (void*)accepted_socket);

        std::cout << "SERVER >> CLIENT CONNECTED" << std::endl;
    }

    close(socket);

    pthread_exit(nullptr);
}

int main()
{
    int sockfd, newsockfd, portno = 7331;

    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) error("ERROR opening socket");

    bzero((char*)&serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr*) & serv_addr, sizeof(serv_addr)) < 0)
    {
        error("ERROR on binding");
    }

    listen(sockfd, 5);

    pthread_mutex_init(&client_sockets_mutex, nullptr);

    handle_connections(sockfd);

    pthread_mutex_destroy(&client_sockets_mutex);

    return 0;
}
