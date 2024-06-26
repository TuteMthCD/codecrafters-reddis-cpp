#include "includes/redis.h"
#include <arpa/inet.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <iterator>
#include <netinet/in.h>
#include <ostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define REDIS_ERROR (const char*)"-Error message\r\n"
#define REDIS_OK (const char*)"+OK\r\n"
#define REDIS_BULK_ERROR (const char*)"$-1\r\n"

Redis::Redis(redis_config_t _config) {

    config = _config;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if(server_fd < 0) {
        std::cerr << "Failed to create server socket\n";
    }

    // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        std::cerr << "setsockopt failed\n";
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(config.port);

    if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        std::cerr << "Failed to bind to port" << config.port << std::endl;
    }

    int connection_backlog = 5;
    if(listen(server_fd, connection_backlog) != 0) {
        std::cerr << "listen failed\n";
    }


    std::cout << "Waiting for a client to connect...\n";

    if(config.mode == redis_config_t::SLAVE)
        openThreads.push_back(std::thread(&Redis::ReplicaEntryPoint, this));

    openThreads.push_back(std::thread(&Redis::Listener, this, server_fd));
};

Redis::~Redis() {
    close(server_fd);
}

void Redis::Listener(uint32_t server_fd) {
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    while(int32_t client_fd = accept(server_fd, (struct sockaddr*)&client_addr, (socklen_t*)&client_addr_len)) {
        // create thread and push to vector
        openThreads.push_back(std::thread(&Redis::HandleClients, this, client_fd));
    }
}

void Redis::HandleClients(int32_t client_fd) {
    if(!client_fd)
        std::cerr << "Client error\n";
    // else
    //     std::cout << "Client connected\n";

    std::vector<uint8_t> buff(1024);

    while(recv(client_fd, buff.data(), buff.size(), 0) > 0) {
        std::vector<std::string> cmd;
        cmd = ParseArray(ParseMsg(buff));

        for(char& c : cmd[0]) c = std::tolower(c);

        if(cmd.size() > 0) {
            if(cmd[0] == "echo") {
                std::string msg = RedisStr(cmd[1]);
                send(client_fd, msg.c_str(), msg.size(), 0);
            }

            if(cmd[0] == "set") { // cuando escribi esto entendia que era este codigo , ahora solo dios sabe
                size_t arg_size = cmd.size();
                if(arg_size >= 3) {
                    set(cmd[1], cmd[2]);
                    send(client_fd, REDIS_OK, std::strlen(REDIS_OK), 0);
                    if(arg_size > 4) {
                        if(cmd[3] == "px")
                            openThreads.push_back(std::thread(&Redis::RemoveKey, this, cmd[1].c_str(), std::atoi(cmd[4].data())));
                    }
                } else
                    send(client_fd, REDIS_ERROR, std::strlen(REDIS_ERROR), 0);
            }

            if(cmd[0] == "get") {
                if(cmd.size() >= 2) {
                    std::string value = get(cmd[1]);

                    if(value == "\0")
                        value = REDIS_BULK_ERROR;
                    else
                        value = RedisStr(value);

                    send(client_fd, value.c_str(), value.size(), 0);

                } else
                    send(client_fd, REDIS_ERROR, std::strlen(REDIS_ERROR), 0);
            }

            if(cmd[0] == "ping") {
                std::string PingResponse = "+PONG\r\n";
                send(client_fd, PingResponse.c_str(), PingResponse.size(), 0);
            }

            if(cmd[0] == "info") {
                std::string InfoResponse;

                InfoResponse += (config.mode == redis_config_t::MASTER) ? "role:master\n" : "role:slave\n";
                InfoResponse += "master_replid:8371b4fb1155b71f4a04d3e1bc3e18c4a990aeeb\n";
                InfoResponse += "master_repl_offset:0\n";

                InfoResponse = RedisStr(InfoResponse);
                send(client_fd, InfoResponse.c_str(), InfoResponse.size(), 0);
            }

            if(cmd[0] == "replconf") {
                send(client_fd, REDIS_OK, std::strlen(REDIS_OK), 0);
            }
        }
    }
    close(client_fd);
}

std::string Redis::RedisStr(std::string str) {
    std::string msg = "$" + std::to_string(str.size()) + "\r\n" + str + "\r\n"; // solo hace falta devolver el primero
    return msg;
}

void Redis::set(std::string key, std::string value) {
    store.insert(std::pair<std::string, std::string>(key, value));
}

std::string Redis::get(std::string key) {
    auto value = store.find(key);

    if(value == store.end())
        return "\0";

    return value->second;
}

void Redis::RemoveKey(std::string key, int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    auto ite = store.find(key);
    if(ite != store.end()) {
        store.erase(ite);
    }
}

std::vector<std::string> Redis::ParseMsg(std::vector<uint8_t> buff) {
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

std::vector<std::string> Redis::ParseArray(std::vector<std::string> msg) {
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


        case '+':
            parsed.push_back(msg[i].substr(1, msg[i].size()));
            i++;
            break;

        default: std::cerr << "Character not recognized -> " << msg[i] << std::endl; break;
        }
        i++;
    }

    return parsed;
}

void Redis::ReplicaEntryPoint() {
    int replica_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(replica_fd < 0) {
        std::cerr << "Failed to create replic" << std::endl;
        return;
    }

    struct sockaddr_in master_addr = { .sin_family = AF_INET, .sin_port = htons(config.replic_port) };

    if(std::strcmp(config.replic_addr, "localhost") == 0)
        config.replic_addr = (char*)"127.0.0.1";

    if(inet_pton(AF_INET, config.replic_addr, &master_addr.sin_addr) <= 0) {
        std::cerr << "Address of replic is not valid" << std::endl;
        return;
    }

    if(connect(replica_fd, (struct sockaddr*)&master_addr, sizeof(master_addr)) == -1) {
        std ::cerr << "Connection to master failed" << std::endl;
        return;
    } else
        std::cout << "Connection to master successfully" << std::endl;

    std::vector<uint8_t> buff(256);
    std::string ping = "*1\r\n$4\r\nping\r\n";
    send(replica_fd, ping.c_str(), ping.length(), 0);

    while(recv(replica_fd, buff.data(), buff.size(), 0)) {
        // leo lo que llega
        std::vector<std::string> response = ParseArray(ParseMsg(buff));

        for(std::string cmd : response) {
            if(cmd == "PONG") {
                std::string rta = "*3\r\n$8\r\nREPLCONF\r\n$14\r\nlistening-port\r\n$4\r\n6380\r\n";
                send(replica_fd, rta.data(), rta.size(), 0);
                rta = "*3\r\n$8\r\nREPLCONF\r\n$4\r\ncapa\r\n$6\r\npsync2\r\n";
                send(replica_fd, rta.data(), rta.size(), 0);
            }
        }

        // for(char c : buff) { printf("%c", c); }
        // printf("\n");
    }
}
