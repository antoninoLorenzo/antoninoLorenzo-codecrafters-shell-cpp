#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <functional>
#include <filesystem>


namespace fs = std::filesystem;

#if __linux__
    const char *PATH_DELIMITER = ":";
    const char *PATH_SEPARATOR = "/";
#elif _WIN32
    const char *PATH_DELIMITER = ";";
    const char *PATH_SEPARATOR = "\\";
#else
    const char *PATH_DELIMITER = ":";
    const char *PATH_SEPARATOR = "/";
#endif

// define PATH at compile time, but populate at program start.
// Note: std::vector is a dynamic array
std::vector<std::string> PATH;

// unordered map should be more efficient for direct access
std::unordered_map<
    std::string, 
    std::function<void(std::string)>
> BUILTIN_FUNCTIONS;


/**
 * Divides a string in a vector of substrings based on a delimiter.
 */
void split(
    std::vector<std::string>  *vec_ptr,
    std::string                str,
    const char                *delimiter
){
    std::stringstream stream(str);
    std::string chunk;
    while(std::getline(stream, chunk, *delimiter)) {
        vec_ptr->push_back(chunk);
    }
}


/**
 * Search a file in a directory.
 * @return true if found, otherwise false
 */
bool __search_file(
    std::string absolute_path,
    std::string file_name
) {
    fs::path abs(absolute_path);
    // the absolute_path should refer to a directory, if that's not
    // the case, this means the PATH is not set correctly.
    if (fs::is_directory(absolute_path) == false) {
        return false;
    }

    for (auto const& dir_entry : fs::directory_iterator{abs}) {
        if (dir_entry.is_directory())
            continue;
        if (dir_entry.path().stem().string() == file_name)
            return true;
    }

    return false;
}

/**
 * Implements `echo` command.
 */
void __builtin_echo(std::string input) {
    std::cout << input << std::endl;    
}

/**
 * Implements `type` command, used to determine if a command is available.
 * It searches in BUILTIN_COMMANDS and in PATH.
 * 
 * Examples:
 * 
 * $ type echo
 * echo is a shell builtin
 * 
 * $ type python.exe
 * python is /usr/local/bin/python
 * 
 * $ type something
 * something: not found
 * 
 */
void __builtin_type(std::string input) {
    if (BUILTIN_FUNCTIONS.count(input) || input == "exit") {
        std::cout << input << " is a shell builtin" << std::endl;
        return;
    }

    bool found = false;
    std::string executable_path;
    for (int i = 0; i < PATH.size(); i++) {
        found = __search_file(PATH.at(i), input);
        if (found) {
            executable_path = PATH.at(i) + PATH_SEPARATOR + input;
            break;
        }
    }
    
    if (found)
        std::cout << input << " is " << executable_path << std::endl;
    else
        std::cout << input << ": not found" << std::endl;
}


int main() {
    // parse PATH
    const char* path_raw = getenv("PATH");
    if (path_raw != nullptr) {
        std::string path_str = path_raw;
        split(&PATH, path_str, PATH_DELIMITER);
    }

    // map command names to functions
    BUILTIN_FUNCTIONS["echo"] = [](std::string input){
        __builtin_echo(input);
    };
    BUILTIN_FUNCTIONS["type"] = [](std::string input){
        __builtin_type(input);
    };

    // Flush after every std::cout / std:cerr
    // Note: I tried removing also this
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // REPL
    while (true) {
        std::cout << "$ ";

        std::string input, command, command_args;
        std::getline(std::cin, input);
        if (input.compare(0, 4, "exit") == 0)
            break;
        
        command = input.substr(0, input.find(" "));
        command_args = input.substr(input.find(" ")+1); 
        
        if (BUILTIN_FUNCTIONS.count(command)) 
            BUILTIN_FUNCTIONS[command](command_args);
        else
            std::cout << input << ": command not found" << std::endl;
    }
}
