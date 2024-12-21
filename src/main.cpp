#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <functional>
#include <filesystem>


namespace fs = std::filesystem;

#if __linux__
    #include <cstring>
    #include <stdlib.h>
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/wait.h>

    #define READ_END  0
    #define WRITE_END 1
    #define BUF_SIZE  256

    const char *PATH_DELIMITER = ":";
    const char *PATH_SEPARATOR = "/";
#elif _WIN32
    #include <Windows.h>
    const char *PATH_DELIMITER = ";";
    const char *PATH_SEPARATOR = "\\";
#else
    const char *PATH_DELIMITER = ":";
    const char *PATH_SEPARATOR = "/";
#endif

// define PATH at compile time, but populate at program start.
// Note: std::vector is a dynamic array
std::vector<std::string> PATH;

// current working directory
fs::path WORKING_DIR = fs::current_path();

// unordered map should be more efficient for direct access
// having a function that returns void is ok, pointers could eventually be used,
// but having a single param is a pain in the ass with string parsing in complex scenarios.
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
 * Replace every occurence of old_value to new_value in a string.
 * Similar to Python str.replace()
 */
void replace(
    std::string *str,
    const char  *old_value,
    const char  *new_value
)
{   
    std::size_t pos = 0; 
    std::size_t old_len = std::strlen(old_value); 

    while ((pos = str->find(old_value, pos)) != std::string::npos)
    {
        str->replace(pos, old_len, new_value);
        pos += std::strlen(new_value);
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
void __builtin_echo(std::string input) 
{
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
void __builtin_type(std::string input) 
{
    if (BUILTIN_FUNCTIONS.count(input) || input == "exit") 
    {
        std::cout << input << " is a shell builtin" << std::endl;
        return;
    }

    bool found = false;
    std::string executable_path;
    for (int i = 0; i < PATH.size(); i++) 
    {
        found = __search_file(PATH.at(i), input);
        if (found) 
        {
            executable_path = PATH.at(i) + PATH_SEPARATOR + input;
            break;
        }
    }
    
    if (found)
        std::cout << input << " is " << executable_path << std::endl;
    else
        std::cout << input << ": not found" << std::endl;
}


/**
 * Runs an external program located in PATH as a child process.
 * 
 * *Note: doesn't handle ./run.something*
 * 
 * @param input: full command (name + arguments)
 */
void __builtin_exec(std::string input) 
{
    if (input.size() == 0)
    {
        std::cout << "Error: empty input" << std::endl;
        return;
    }

    std::vector<std::string> split_args;
    const char *space = " ";
    // doesn't work properly for cases like `python -c "import os"`
    split(&split_args, input, space);

    bool found = false;
    std::string executable_path;
    for (int i = 0; i < PATH.size(); i++) 
    {
        found = __search_file(PATH.at(i), split_args.at(0));
        if (found) 
        {
            executable_path = PATH.at(i) + PATH_SEPARATOR + split_args.at(0) ;
            break;
        }
    }

    if(!found)
    {
        // shouldn't actually happen
        std::cout << "Error: how is possible that it didn't found now?" << std::endl;
        return;
    }

    // Actual subprocess
    #ifdef _WIN32
        // use CreateProcess
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        BOOL create_flag;

        SecureZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        SecureZeroMemory(&pi, sizeof(pi));

        // path must be quoted otherwise it won't find the file
        // when there is a space (ex. C:\Program Files\Git\usr\bin\ls)
        executable_path = "\"" + executable_path + "\"";
        
        // convert split_args to string with full path instead of the command.
        // ex. ls -la = "C:\Program Files\Git\usr\bin\ls" -la 
        split_args.at(0) = executable_path;
        std::string full_input = "";
        for (int i = 0; i < split_args.size(); i++)
        {
            if (i >= 1)
                full_input = full_input + " " + split_args.at(i);
            else
                full_input = full_input + split_args.at(i);
        }
        
        LPSTR lp_input = (LPSTR) full_input.c_str();

        // std::cout << lp_input << std::endl;
        create_flag = CreateProcessA(
            NULL,
            lp_input,
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            NULL,
            &si,
            &pi
        );

        if (create_flag == FALSE)
        {
            std::cout << "Error creating process: " << GetLastError() << std::endl;
            // see error codes at https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes
            return;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

    #elif __linux__
        // use fork
        pid_t pid;
        int stdin_pipe[2], stdout_pipe[2];

        if (pipe(stdin_pipe) == -1)
        {
            std::cout << "Error creating stdin pipe" << std::endl;
            return;
        }

        if (pipe(stdout_pipe) == -1)
        {
            std::cout << "Error creating stdout pipe" << std::endl;
            return;
        } 

        pid = fork();
        if (pid < 0)
        {
            std::cout << "Fork failed." << std::endl;
            return;
        } 

        if (pid > 0)
        {
            int status, n_bytes;
            char child_stdout_buffer[BUF_SIZE] = {0};

            close(stdin_pipe[READ_END]); // parent writes to stdin
            close(stdout_pipe[WRITE_END]); // parent reads from stdout

            // exec (in)
            while ((n_bytes = read(stdout_pipe[READ_END], child_stdout_buffer, BUF_SIZE)) > 0)
            {
                std::cout << std::flush;
                write(STDOUT_FILENO, child_stdout_buffer, n_bytes);
                std::cout << std::flush;
            }

            // std::string some;
            // std::getline(std::cin, some);
            // write(stdin_pipe[WRITE_END], some.c_str(), some.size() + 1);

            close(stdin_pipe[WRITE_END]); // done taking input 
            close(stdout_pipe[READ_END]); // done reading

            waitpid(pid, &status, 0);
        }
        else if (pid == 0)
        {
            close(stdin_pipe[WRITE_END]); //  son reads from stdin

            // handle standard output (son writes to stdout)
            if (dup2(stdout_pipe[WRITE_END], STDOUT_FILENO) == -1)
            {
                perror("dup2: stdout");
                exit(EXIT_FAILURE);
            }

            close(stdout_pipe[READ_END]); 
            close(stdout_pipe[WRITE_END]);

            // exec (out)
            
            std::vector<char *> c_args;
            
            c_args.push_back(const_cast<char*>(executable_path.c_str()));
            for (size_t i = 1; i < split_args.size(); ++i)
                c_args.push_back(const_cast<char*>(split_args[i].c_str()));
            c_args.push_back(nullptr);

            execv(executable_path.c_str(), c_args.data());

            perror("shell");
            close(STDOUT_FILENO);
            exit(EXIT_FAILURE);
        }
    #else 
        #error "Sub-processing unavailable for current OS"
    #endif
}

void __builtin_print_working_directory(std::string unused)
{
    std::cout << WORKING_DIR << std::endl;
}

void __builtin_change_directory(std::string input)
{
    // Should handle:
    // - absolute path 
    // - relative path
    // - dots

    // to differentiate between path types:
    // start with PATH_DELIMITER    -> absolute 
    // start with allowed character -> relative
    // start with .                 -> dots
    // !!!: std::filesystem has is_absolute and is_relative

    // case absoulute path
    // go to input directory
    //
    // edge case: don't exist -> error

    // case relative path
    // search input in WORKING_DIR and go to it
    // 
    // edge case: don't exist -> error

    // case dots: .., ../, ../.., ../../ etc.
    // possible approach:
    // split(&tmp, input, PATH_DELIMITER);
    // go parent directory for tmp.size() times
    //
    // edge case: too much ../ -> got to highest possible path
}


int main() 
{
    // parse PATH
    // Microsoft specific alternative: _dupenv_s()
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
    BUILTIN_FUNCTIONS["exec"] = [](std::string input){
        __builtin_exec(input);
    };
    BUILTIN_FUNCTIONS["pwd"] = [](std::string unused){
        __builtin_print_working_directory("");
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
        
        // Search in BUILTIN
        if (BUILTIN_FUNCTIONS.count(command)) 
        {
            BUILTIN_FUNCTIONS[command](command_args);
            continue;
        }
        
        // Search in PATH, otherwise not found
        bool found = false;
        std::string executable_path;
        for (int i = 0; i < PATH.size(); i++) 
        {
            found = __search_file(PATH.at(i), command);
            if (found) 
            {
                executable_path = PATH.at(i) + PATH_SEPARATOR + command;
                break;
            }
        }

        if (found)
            BUILTIN_FUNCTIONS["exec"](input); // should pass PATH and command_args
        else
            std::cout << command << ": command not found" << std::endl;
    }
}
