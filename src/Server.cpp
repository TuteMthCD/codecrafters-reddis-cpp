#include <arpa/inet.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

std::string ping_response = "+PONG\r\n";

std::vector<std::string> parseMsg(std::vector<uint8_t> buff) {
    std::vector<std::string> parsed;

    const std::string separator = "\r\n";
    std::string s(buff.begin(), buff.end());

    int start = 0, end = 0;

    do {
        end = s.find(separator, start);                     // obtener la posicion del separator
        if(end != std::string::npos) {                      // que end no sea el final asi no pusha un str vacio
            parsed.push_back(s.substr(start, end - start)); // obtenemos el substr hasta el separator
            start = end + separator.size();                 // pasamos al siguiente paso
        }
    } while(end != std::string::npos);

    return parsed;
}

std::vector<std::string> parseArray(std::vector<std::string> msg) {
    std::vector<std::string> parsed;

    uint32_t i = 0;

    while(i < msg.size()) {
        switch(msg[i][0]) { // miro el primer caracter de la array de strings
        case '$':           // guardo el strings de la array
            parsed.push_back(msg[i + 1]);
            i++;
            break;

        case '*': // reserve de la array
            parsed.reserve(std::atoi(msg[i].data() + 1));
            break;

        default: std::cerr << "Character not recognized" << std::endl; break;
        }
        i++;
    }

    return parsed;
}

void handle_clients(int32_t client_fd) {
    if(!client_fd)
        std::cerr << "Client error\n";
    else
        std::cout << "Client connected\n";

    std::vector<uint8_t> buff(1024);

    while(recv(client_fd, buff.data(), buff.size(), 0) > 0) {
        std::vector<std::string> parsed;
        parsed = parseArray(parseMsg(buff));

        if(parsed.size() > 0) {
            if(parsed[0] == "echo") {
                parsed.erase(parsed.begin());

                std::string s_echo;

                for(std::string s : parsed) {
                    s_echo += "$";
                    s_echo += std::to_string(s.size());
                    s_echo += "\r\n";
                    s_echo += s;
                    s_echo += "\r\n";
                }

                send(client_fd, s_echo.c_str(), s_echo.size(), 0);
            } else
                send(client_fd, ping_response.c_str(), ping_response.size(), 0);
        }
    }
    close(client_fd);
}

int main(int argc, char** argv) {
    // You can use print statements as follows for debugging, they'll be visible when running tests.
    std::cout << "Logs from your program will appear here!\n";

    // Uncomment this block to pass the first stage

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(6379);

    if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port 6379\n";
        return 1;
    }

    int connection_backlog = 5;
    if(listen(server_fd, connection_backlog) != 0) {
        std::cerr << "listen failed\n";
        return 1;
    }

    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    std::cout << "Waiting for a client to connect...\n";

    std::vector<std::thread> openThreads;

    while(int32_t client_fd = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&client_addr_len)) {
        // create thread and push to vector
        openThreads.push_back(std::thread(handle_clients, client_fd));
    }


    close(server_fd);

    return 0;
}
