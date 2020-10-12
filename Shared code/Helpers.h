#ifndef HELPERS_H
#define HELPERS_H
#include <iostream>
#include <stdio.h>
#include <sys/time.h>
#include <windows.h>
#include <stdlib.h>
#include <string>
#include <tchar.h>
#include <tlhelp32.h>
#include <vector>
#include <sstream>
#include <math.h>
#include <algorithm>
#include <ctime>
#include <unistd.h>
#include <sstream>
#include <mutex>
#include <random>
#include <list>
#include <fstream>
#include <future>

#define printf sync_printf
#define WEBSERVER_KEY iLikeGirls528

#define CC_GRAY 7
#define CC_BLUE 9
#define CC_GREEN 10
#define CC_CYAN 11
#define CC_RED 12
#define CC_PINK 13
#define CC_YELLOW 14
#define CC_WHITE 15
#define CC_DARKGREEN 2
#define CC_DARKBLUE 1
#define CC_DARKAQUA 3
#define CC_DARKCYAN 4
#define CC_DARKPURPLE 5

using namespace std;

extern std::mutex mtx_printf;
extern HANDLE hConsole;
extern char APPLOCATION[256];

void sync_printf(const char *format, ...);
void printf(int color, const char *format, ...);
std::string LastError();
void inline PrintLastError() {
        printf("\t%s", LastError().c_str());
}
bool GetLineByKey(string &receiver, string key, vector<string> &searchIn, string defaultReturn = "");
vector<string> GetFileLines(string filepath);
void EditFileLine(string newValue, string key, string fileName);
void FilePutContents(vector<string>& contents, string fileName);
int Random(int min, int max);
std::string random_string(size_t length);
std::string GetTimestamp();

vector<UINT> GetProcessList(const char* exeName);

template<typename T>
bool IsThreadFinished(std::future<T>& t) {
        return t.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

namespace String {
std::vector<string> split(std::string fullstr, std::string delim);
void ltrim(std::string &s);
void rtrim(std::string &s);
void trim(std::string &s);
void trim(std::vector<std::string> &s);
std::string ws2s(const std::wstring& wide);
std::wstring s2ws(const std::string& str);
std::string FormatNumber(double number);
}

DWORD LaunchExe(const string path);
#endif // HELPERS_H
