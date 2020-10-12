#include "WebServer.h"

/*
    Should this be a class or namespace? Difficult choices
    Should I return string and care not for handling post errors, or should I return int and use a receiver parameter? Difficult choices
    If mistakes show up in the future then git gud scrub
*/
namespace WebServer {
string address = "", publicIP = "";
string useragent = "";
int port = 80;

bool POST(string data, string* receiver, string target) {
        char buffer[10240];
        struct sockaddr_in serveraddr;
        int _socket;
        ///build request
        std::stringstream request;
        request<<"POST "<<target<<" HTTP/1.1\r\n";
        request<<"User-Agent: "<<useragent<<"\r\n";
        request<<"Host: "<<address<<"\r\n";
        request<<"Content-Type: application/x-www-form-urlencoded\r\n";
        request<<"Content-Encoding: binary\r\n";
        request<<"Content-Length: "<<data.length()<<"\r\n";
        request<<"Connection: close\r\n";
        request<<"\r\n";
        request<<data;
        ///socket
        if((_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) return false;
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port = htons(port);
        if(InetPton(AF_INET, address.c_str(), &serveraddr.sin_addr)<=0) return false;
        if(connect(_socket, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) < 0) return false;
        send(_socket, request.str().c_str(), request.str().length(), 0);
        recv(_socket, (char*)&buffer, sizeof(buffer), 0);
        closesocket(_socket);
        char* content = strstr(buffer, "\r\n\r\n");
        if(content != NULL) content += 4;
        if(receiver != NULL) *receiver = content;
        return true;
}

bool GET(string hostname, string* receiver) {
        char buffer[10240];
        struct sockaddr_in serveraddr;
        int _socket;
        struct hostent *host;

        host = gethostbyname(hostname.c_str());
        ///build request
        std::stringstream request;
        request<<"GET / HTTP/1.1\r\n";
        request<<"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/80.0.3987.163 Safari/537.36\r\n";
        request<<"Host: "<<hostname<<"\r\n";
        request<<"Connection: close\r\n";
        request<<"\r\n";
        ///socket
        if((_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) return false;
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port = htons(80);
        serveraddr.sin_addr.s_addr = *((unsigned long*)host->h_addr);
        if(connect(_socket, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) < 0) return false;
        send(_socket, request.str().c_str(), request.str().length(), 0);
        recv(_socket, (char*)&buffer, sizeof(buffer), 0);
        closesocket(_socket);
        char* content = strstr(buffer, "\r\n\r\n");
        if(content != NULL) content += 4;
        if(receiver != NULL) *receiver = content;
        return true;
}

bool GetPublicIP() {
        string result;
        if(!GET("whatismyip.host", &result)) {
                printf(CC_RED, "\t@Failed to GET from whatismyip.host\nThis machine's public address is unknown\n");
                publicIP = "";
                return false;
        } else {
                unsigned first = result.find("<p class=\"ipaddress\">")+string("<p class=\"ipaddress\">").length();
                unsigned last = result.find("</p>", first);
                publicIP = result.substr(first,last-first);
        }
        return true;
}
}
