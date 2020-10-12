#ifndef WEBCONNECTION_H
#define WEBCONNECTION_H
#include <iostream>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <winsock2.h>
#include <sys/time.h>
#include <windows.h>
#include <stdlib.h>
#include <string>
#include <tchar.h>
#include <tlhelp32.h>
#include <vector>
#include <sstream>
#include <shellapi.h>
#include <math.h>
#include <algorithm>
#include <fstream>
#include <ctime>
#include <unistd.h>
#include <sstream>
#include <conio.h>
#include <thread>
#include <dirent.h>
#include <bitset>
#include <mutex>
#include <random>
#include <chrono>
#include <future>
#include <list>
#include "../Shared code/Helpers.h"
#include "../json/single_include/nlohmann/json.hpp"
#include "ACCOUNTS.h"
#include "../Shared code/GLOBALS.h"
#include "MEMORY.h"

struct WEBSOCKET {
        int _socket;
        time_t BEGINTIME;
        string ip;
        WEBSOCKET(int p) {
                _socket = p;
                BEGINTIME = time(0);
        }
};

class WEBCONNECTION {
public:
        thread THREAD;
        bool TERMINATE = false;
        int PORT = 9002;
        int MASTER_SOCKET, NEW_SOCKET, addrlen, maxClients = 30, activity, sd, max_sd;
        vector<WEBSOCKET> WEB_SOCKETS;
        struct sockaddr_in address;
        char buffer[10240];
        fd_set readfds;

        WEBCONNECTION();

        void GetConfig();
        int init() ;
        void _listen() ;
        void ProcessMessage(WEBSOCKET *websocket, string msg, int bytes);
        bool Send(WEBSOCKET* to, string msg) ;
};
#endif // WEBCONNECTION_H
