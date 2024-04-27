#include "includes/redis.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>


int main(int argc, char** argv) {

    redis_config_t* config = new redis_config_t();

    for(uint32_t i = 0; i < argc; i++) {
        if(strcmp("--port", argv[i]) == 0) {
            config->port = std::stoi(argv[i + 1]);
        }
        if(strcmp("--replicaof", argv[i]) == 0) {
            config->replic_addr = argv[i + 1];
            config->replic_port = std::stoi(argv[i + 2]);
            config->mode = redis_config_t::SLAVE;
        };
    }

    Redis* redis = new Redis(*config);

    getchar();

    delete redis;


    return 0;
}
