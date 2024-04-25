#include <arpa/inet.h>
#include <cstdlib>
#include <iostream>
#include <map>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

struct redis_config_t {
    int port = 6379;
    int replic_port;
    char* replic_addr;
    enum {
        MASTER,
        SLAVE,
    } mode;
};

class Redis {
    public:
    Redis(redis_config_t);
    Redis(Redis&&) = default;
    Redis(const Redis&) = default;
    Redis& operator=(Redis&&) = default;
    Redis& operator=(const Redis&) = default;
    ~Redis();


    private:
    // funciones privadas
    std::vector<std::string> ParseMsg(std::vector<uint8_t> buff);
    std::vector<std::string> ParseArray(std::vector<std::string> msg);
    std::string echoer(std::vector<std::string> msg);
    std::string RedisStr(std::string str);

    // funciones y variables database
    std::map<std::string, std::string> store;
    void set(std::string key, std::string value);
    std::string get(std::string key);
    void RemoveKey(std::string key, int ms);
    void HandleClients(int32_t client_fd);

    int server_fd; // server
    redis_config_t config;

    std::vector<std::thread> openThreads;
};
