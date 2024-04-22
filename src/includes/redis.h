#include <arpa/inet.h>
#include <cstdlib>
#include <iostream>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>


void handle_clients(int32_t client_fd);
