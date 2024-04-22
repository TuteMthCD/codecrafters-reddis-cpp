#include "includes/redis.h"
#include <cctype>
#include <cstring>
#include <map>
#include <string>


// funciones privadas
std::vector<std::string> parseMsg(std::vector<uint8_t> buff);
std::vector<std::string> parseArray(std::vector<std::string> msg);
std::string echoer(std::vector<std::string> msg);
std::string redis_str(std::string str);

// funciones y variables database
std::map<std::string, std::string> store;
void set(std::string key, std::string value);
std::string get(std::string key);

const char* redis_error = "-Error message\r\n";
const char* redis_ok = "+OK\r\n";


void handle_clients(int32_t client_fd) {
    if(!client_fd)
        std::cerr << "Client error\n";
    // else
    //     std::cout << "Client connected\n";

    std::vector<uint8_t> buff(1024);

    while(recv(client_fd, buff.data(), buff.size(), 0) > 0) {
        std::vector<std::string> cmd;
        cmd = parseArray(parseMsg(buff));

        for(char& c : cmd[0]) c = std::tolower(c);

        if(cmd.size() > 0) {
            if(cmd[0] == "echo") {
                std::string msg = redis_str(cmd[1]);
                send(client_fd, msg.c_str(), msg.size(), 0);
            }

            if(cmd[0] == "set") {
                if(cmd.size() >= 3) {
                    set(cmd[1], cmd[2]);
                    send(client_fd, redis_ok, std::strlen(redis_ok), 0);
                } else
                    send(client_fd, redis_error, std::strlen(redis_error), 0);
            }

            if(cmd[0] == "get") {
                if(cmd.size() >= 2) {
                    std::string value = get(cmd[1]);
                    value = redis_str(value);
                    send(client_fd, value.c_str(), value.size(), 0);
                } else
                    send(client_fd, redis_error, std::strlen(redis_error), 0);
            }

            if(cmd[0] == "ping") {
                std::string ping_response = "+PONG\r\n";
                send(client_fd, ping_response.c_str(), ping_response.size(), 0);
            }
        }
    }
    close(client_fd);
}

std::string redis_str(std::string str) {
    std::string msg = "$" + std::to_string(str.size()) + "\r\n" + str + "\r\n"; // solo hace falta devolver el primero
    return msg;
}

void set(std::string key, std::string value) {
    store.insert(std::pair<std::string, std::string>(key, value));
}

std::string get(std::string key) {
    auto value = store.find(key);

    if(value == store.end())
        return "\0";

    return value->second;
}

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
