#ifndef PATH_H
#define PATH_H

#include <string>

extern std::string path; // defined in path.cpp

// Return 0 on error.
int route(const std::string &t, std::string path, std::string &sub);

std::string stripSlash(const std::string &path);

std::string eqbeatsDir();

#endif // PATH_H
