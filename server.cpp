#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <mutex>
#include <algorithm>

const int PORT = 8080;

std::vector<std::pair<int, std::string>> clients;
std::mutex clients_mutex;

void broadcast_message(const std::string &msg, int sender_socket) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto &client : clients) {
        if (client.first != sender_socket) {
            send(client.first, msg.c_str(), msg.size(), 0);
        }
    }
}

void handle_client(int client_socket, std::string username) {
    char buffer[1024];

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

        if (bytes_received <= 0) {
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients.erase(std::remove_if(
                    clients.begin(), clients.end(),
                    [&](auto &p){ return p.first == client_socket; }),
                    clients.end()
                );
            }
            close(client_socket);
            std::string leaveMsg = "⬅️ " + username + " left the chat!\n";
            broadcast_message(leaveMsg, client_socket);
            return;
        }

        std::string msg = "[" + username + "]: " + buffer;
        broadcast_message(msg, client_socket);
    }
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        return 1;
    }

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        char nameBuffer[32] = {0};
        recv(client_socket, nameBuffer, sizeof(nameBuffer), 0);
        std::string username = nameBuffer;

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.push_back({client_socket, username});
        }

        std::string joinMsg = "➡️ " + username + " joined the chat!\n";
        broadcast_message(joinMsg, client_socket);

        std::thread(handle_client, client_socket, username).detach();
    }

    close(server_socket);
    return 0;
}
