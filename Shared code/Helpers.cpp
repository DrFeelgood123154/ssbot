#include "Helpers.h"

HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
char APPLOCATION[256];

std::mutex mtx_printf;
void sync_printf(const char *format, ...) {
        va_list args;
        va_start(args, format);

        mtx_printf.lock();
        vprintf(format, args);
        mtx_printf.unlock();

        va_end(args);
}
void printf(int color, const char *format, ...) {
        va_list args;
        va_start(args, format);
        mtx_printf.lock();
        SetConsoleTextAttribute(hConsole, color);

        vprintf(format, args);

        SetConsoleTextAttribute(hConsole, CC_GRAY);
        mtx_printf.unlock();
        va_end(args);
}
std::string LastError() {
        DWORD errorMessageID = ::GetLastError();
        if(errorMessageID == 0) return std::string();

        LPSTR messageBuffer = nullptr;
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                     NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
        std::string message(messageBuffer, size);
        LocalFree(messageBuffer);

        return message;
}
vector<string> GetFileLines(string filepath) {
        vector<string> ret;
        string line;
        ifstream file(filepath.c_str());
        if(!file.good()) {
                printf("Error opening file (%s)\n", filepath.c_str());
                printf("\t%s\n", strerror(errno));
                return ret;
        }
        while(getline(file, line)) ret.push_back(line);
        return ret;
}
bool GetLineByKey(string &receiver, string key, vector<string> &searchIn, string defaultReturn) {
        for(string &line : searchIn) {
                if(line.find(key+"=") != string::npos) {
                        receiver = line.substr(line.find_first_of("=")+1, string::npos);
                        return true;
                }
        }
        receiver = defaultReturn;
        return false;
}
void EditFileLine(string newValue, string key, string fileName) {
        vector<string> lines = GetFileLines(fileName);
        for(string &line : lines) {
                if(line.find(key+"=") != string::npos) {
                        line = key+"="+newValue;
                        break;
                }
        }
        FilePutContents(lines, fileName);
}
void FilePutContents(vector<string>& contents, string fileName) {
        ofstream outfile(fileName);
        for(const string& line : contents) outfile<<line.c_str()<<endl;
        outfile.close();
}
/*
int intRand(const int & min, const int & max) {
    static thread_local mt19937* generator = nullptr;
    if(!generator) generator = new mt19937(time(0));
    uniform_int_distribution<int> distribution(min, max);
    return distribution(*generator);
}*/
int Random(int min, int max) {
        int n = max - min + 1;
        int remainder = RAND_MAX % n;
        int x;
        do {
                x = rand();
        } while (x >= RAND_MAX - remainder);
        return min + x % n;
}

std::string random_string(size_t length) {
        auto randchar = []() -> char {
                const char charset[] =
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
                ///"abcdefghijklmnopqrstuvwxyz";
                const size_t max_index = (sizeof(charset) - 1);
                return charset[rand() % max_index];
        };
        std::string str(length,0);
        std::generate_n(str.begin(), length, randchar);
        return str;
}
std::string GetTimestamp(){
        stringstream ss;
        time_t now = time(0);
        tm *ltm = localtime(&now);
        if(ltm->tm_hour <= 9) ss<<"0";
        ss<<ltm->tm_hour<<":";
        if(ltm->tm_min <= 9) ss<<"0";
        ss<<ltm->tm_min<<":";
        if(ltm->tm_sec <= 9) ss<<"0";
        ss<<ltm->tm_sec;
        return ss.str();
}

vector<UINT> GetProcessList(const char* exeName) {
        vector<UINT> aprocessList;
        aprocessList.clear();
        PROCESSENTRY32 entry;
        ZeroMemory(&entry, sizeof(entry));
        entry.dwSize = sizeof(PROCESSENTRY32);

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

        if(snapshot == NULL) {
                printf("@Process snapshot returned null, press to continue\n");
                getchar();
                CloseHandle(snapshot);
                return aprocessList;
        }
        if(snapshot == INVALID_HANDLE_VALUE) {
                printf("@Process snapshot returned INVALID_HANDLE_VALUE (Error %lu)\n", GetLastError());
                aprocessList.clear();
                CloseHandle(snapshot);
                return aprocessList;
        }

        if(Process32First(snapshot, &entry) == TRUE) {
                while (Process32Next(snapshot, &entry) == TRUE) {
                        if (_stricmp(entry.szExeFile, exeName) == 0) {
                                aprocessList.push_back(entry.th32ProcessID);
                        }
                }
        } else {
                printf("Error ");
                cout<<GetLastError()<<endl;
                CloseHandle(snapshot);
                getchar();
        }

        CloseHandle(snapshot);
        return aprocessList;
}

std::vector<string> String::split(std::string fullstr, std::string delim) {
        std::vector<string> ret;
        if(fullstr == "") return ret;
        size_t pos = 0;
        std::string token;
        while((pos = fullstr.find(delim)) != std::string::npos) {
                token = fullstr.substr(0, pos);
                ret.push_back(token);
                fullstr.erase(0, pos + delim.length());
        }
        ret.push_back(fullstr);
        return ret;
}
void String::ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
                return !std::isspace(ch);
        }));
}
void String::rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
                return !std::isspace(ch);
        }).base(), s.end());
}
void String::trim(std::string &s) {
        ltrim(s);
        rtrim(s);
}
void String::trim(std::vector<std::string> &s) {
        for(std::string &index : s) {
                ltrim(index);
                rtrim(index);
        }
}
std::string String::ws2s(const std::wstring& wide) {
        std::string out;
        std::copy(wide.begin(), wide.end(), std::back_inserter(out));
        return out;
        /*using convert_typeX = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_typeX, wchar_t> converterX;
        return converterX.to_bytes(wide);*/
}
std::wstring String::s2ws(const std::string& str) {
        if(str == "") return L"";
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo( size_needed, 0 );
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
}
std::string String::FormatNumber(double number){
        string ret = to_string(number);
        size_t dotPos = ret.find(".");
        if(dotPos != string::npos) ret = ret.substr(0, dotPos);
        int insertAt = ret.length()-3;
        while(insertAt > 0){
                ret.insert(insertAt, ",");
                insertAt -= 3;
        }
        return ret;
}

DWORD LaunchExe(const string path){
        STARTUPINFO si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        CreateProcess(path.c_str(), NULL, NULL, NULL, FALSE, DETACHED_PROCESS, NULL, NULL, &si, &pi);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return pi.dwProcessId;
}
