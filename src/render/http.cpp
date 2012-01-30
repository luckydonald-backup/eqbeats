#include "http.h"

std::string statusMsg(int stat){
    switch(stat){
        case 200: return "200 OK";
        case 303: return "303 See Other";
        case 403: return "403 Forbidden";
        case 404: return "404 Not Found";
        case 500: return "500 Server Error";
    }
    return "";
}

std::string Http::header(int status){
    std::string msg = statusMsg(status);
    return msg.empty() ? "\n\n" : "Status: " + msg + "\n\n";
}

std::string Http::header(const std::string &type, int status){
    return
        "Content-Type: " + type + "; charset=UTF-8\n"
        + header(status);
}

std::string Http::notFound(){
    return header(404);
}

std::string Http::redirect(const std::string &location){
    return "Location: " + location + "\n" + header(303);
}