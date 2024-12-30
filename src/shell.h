#ifndef SHELL_H
#define SHELL_H

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <functional>


#if _WIN32
    #include <Windows.h>
    extern const char *PATH_DELIMITER;
    extern const char *PATH_SEPARATOR;
#else
    #include <cstring>
    #include <stdlib.h>
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/wait.h>

    #define READ_END  0
    #define WRITE_END 1
    #define BUF_SIZE  256

    extern const char *PATH_DELIMITER;
    extern const char *PATH_SEPARATOR;
#endif


class Shell
{
public:
    Shell();
    void run(std::string command);

private:
    std::vector<std::string> _PATH;
    std::filesystem::path _HOME_DIR;
    std::filesystem::path _WORKING_DIR;
    std::unordered_map<
        std::string, 
        std::function<void(std::string)>
    > _BUILTIN_FUNCTIONS;

    void __builtin_change_directory(std::string input);
    void __builtin_echo(std::string input);
    void __builtin_exec(std::string input);
    void __builtin_type(std::string input); 
};

#endif