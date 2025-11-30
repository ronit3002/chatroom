#include <iostream>
#include <thread>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

const int PORT = 8080;

void receive_messages(int sock) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;
        std::cout << "\n" << buffer << std::flush;
    }
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    std::cout << "Connected to server!\n";

    std::string username;
    std::cout << "Enter your name: ";
    std::getline(std::cin, username);
    send(sock, username.c_str(), username.size(), 0);

    std::thread(receive_messages, sock).detach();

    while (true) {
        std::string msg;
        std::getline(std::cin, msg);
        if (msg.empty()) continue;
        msg += '\n';
        send(sock, msg.c_str(), msg.size(), 0);
    }

    close(sock);
    return 0;
}
