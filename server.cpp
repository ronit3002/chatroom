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
int waitingPlayer = -1;
int playerX = -1;
int playerO = -1;
int turn = 0;
int gameNumber = 0;
char board[3][3];
bool gameActive = false;
std::mutex clients_mutex;

void broadcast_message(const std::string &msg, int sender_socket) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto &client : clients) {
        int sock = client.first;
        if (sock != sender_socket) {
            send(sock, msg.c_str(), msg.size(), 0);
        }
    }
}

void reset_board(){
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            board[r][c] = ' ';
}

std::string print_board() {
    std::string t = "";
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            t += board[r][c];
            if (c < 2) t += " | ";
        }
        t += "\n";
        if (r < 2) t += "--+---+--\n";
    }
    t += "\n";
    return t;
}

void send_to_X(const std::string &msg) {
    send(playerX, msg.c_str(), msg.size(), 0);
}

void send_to_O(const std::string &msg) {
    send(playerO, msg.c_str(), msg.size(), 0);
}

int check_win() {
    for (int i = 0; i < 3; i++) {
        if (board[i][0] != ' ' &&
            board[i][0] == board[i][1] &&
            board[i][1] == board[i][2]) 
            return (board[i][0] == 'X') ? 1 : 2;

        if (board[0][i] != ' ' &&
            board[0][i] == board[1][i] &&
            board[1][i] == board[2][i])
            return (board[0][i] == 'X') ? 1 : 2;
    }

    if (board[1][1] != ' ' &&
        ((board[0][0] == board[1][1] && board[1][1] == board[2][2]) ||
         (board[0][2] == board[1][1] && board[1][1] == board[2][0])))
        return (board[1][1] == 'X') ? 1 : 2;

    bool full = true;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            if (board[r][c] == ' ')
                full = false;
    if (full) return 0;

    return -1;
}

void handle_move(int client_socket, const std::string& msg) {
    if (!gameActive) {
        send(client_socket, "[Game] No active game. Type /play to start.\n", 46, 0);
        return;
    }

    std::stringstream ss(msg);
    std::string command;
    int row, col;
    ss >> command >> row >> col;

    if (row < 0 || row > 2 || col < 0 || col > 2) {
        send(client_socket, "[Game] Invalid cell!\n", 21, 0);
        return;
    }

    if ((client_socket == playerX && turn != 0) ||
        (client_socket == playerO && turn != 1)) {
        send(client_socket, "[Game] Not your turn!\n", 23, 0);
        return;
    }

    if (board[row][col] != ' ') {
        send(client_socket, "[Game] Cell already taken!\n", 28, 0);
        return;
    }

    if (client_socket == playerX) board[row][col] = 'X';
    else board[row][col] = 'O';

    std::string b = print_board();
    send_to_X(b);
    send_to_O(b);

    int result = check_win();
    if (result != -1) {
        if (result == 1) {
            send_to_X("[Game] You WON!\n");
            send_to_O("[Game] You LOST!\n");
        }
        else if (result == 2) {
            send_to_O("[Game] You WON!\n");
            send_to_X("[Game] You LOST!\n");
        }
        else {
            send_to_X("[Game] It's a DRAW!\n");
            send_to_O("[Game] It's a DRAW!\n");
        }
        
        gameActive = false;
        playerX = -1;
        playerO = -1;
        waitingPlayer = -1;
        gameNumber++;
        send_to_X("[Game] Type /play to start a new match.\n");
        send_to_O("[Game] Type /play to start a new match.\n");

        return;
    }

    turn = 1 - turn;
    if (turn == 0) {
        send_to_X("[Game] Your turn!\n");
        send_to_O("[Game] Waiting for X...\n");
    } else {
        send_to_O("[Game] Your turn!\n");
        send_to_X("[Game] Waiting for O...\n");
    }
}


void play(int client_socket) {
    if (waitingPlayer == -1) {
        waitingPlayer = client_socket;
        std::string msg = "[Game] Waiting for opponent...\n"; 
        send(client_socket, msg.c_str(), msg.size(), 0);
        return;
    }

    if (waitingPlayer != -1 && waitingPlayer != client_socket) {        
        if (gameNumber % 2 == 0) {
            playerX = waitingPlayer;
            playerO = client_socket;
            turn = 0;
        } else {
            playerO = waitingPlayer;
            playerX = client_socket;
            turn = 1;
        }
        waitingPlayer = -1;
        gameActive = true;
        reset_board();
        send_to_X("[Game] You are X!\n");
        send_to_O("[Game] You are O!\n");
        if (turn == 0) {
            send_to_X("[Game] Your turn! Enter: move row col\n");
            send_to_O("[Game] Waiting for X...\n");
        } else {
            send_to_O("[Game] Your turn! Enter: move row col\n");
            send_to_X("[Game] Waiting for O...\n");
        }
        gameNumber++;
    }

    else {
        std::string msg = "Game in Progrss"; 
        send(client_socket, msg.c_str(), msg.size(), 0);
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

            std::string leaveMsg = username + " left the chat!\n";
            std::cout << leaveMsg;
            broadcast_message(leaveMsg, client_socket);
            return;
        }

        std::string msg = buffer;
        if (msg.rfind("/play", 0) == 0) {
            play(client_socket);
            continue;
        }

        if (msg.rfind("move", 0) == 0) {
            handle_move(client_socket, msg);
            continue;
        }

        msg = "[" + username + "]: " + buffer;
        std::cout << msg;
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

    std::cout << "Server running on port " << PORT << "...\n";

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

        std::string joinMsg = username + " joined the chat!\n";
        std::cout << joinMsg;
        broadcast_message(joinMsg, client_socket);

        std::thread(handle_client, client_socket, username).detach();
    }

    close(server_socket);
    return 0;
}
