#include <iostream>

#include "shell.h"
#include "parsing.h"
#include "strutils.h"


#if _WIN32
    #include <Windows.h>
    const char *PATH_DELIMITER = ";";
    const char *PATH_SEPARATOR = "\\";
#else
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
#endif


bool __search_file(
    std::string absolute_path,
    std::string file_name
) {
    std::filesystem::path abs(absolute_path);
    // the absolute_path should refer to a directory, if that's not
    // the case, this means the PATH is not set correctly.
    if (std::filesystem::is_directory(absolute_path) == false) 
        return false;

    for (auto const& dir_entry : std::filesystem::directory_iterator{abs}) 
    {
        if (dir_entry.is_directory())
            continue;
        if (dir_entry.path().stem().string() == file_name)
            return true;
    }

    return false;
}


std::string __get_path(
    std::string executable_name,
    std::vector<std::string> path_vec
)
{
    std::string executable_path("");
    for (int i = 0; i < path_vec.size(); i++) 
    {
        if (__search_file(path_vec.at(i), executable_name)) 
        {
            executable_path = path_vec.at(i) + PATH_SEPARATOR + executable_name;
            break;
        }
    }
    return executable_path;
}


/**
 * Implements `cd` command.
 * 
 * Handles:
 * - absolute paths
 * - relative paths
 * - user home (~)
 */
void Shell::__builtin_change_directory(std::string input)
{
    if (input == "~")
    {
        std::filesystem::current_path(_HOME_DIR);
        _WORKING_DIR = _HOME_DIR;
        return;
    }

    try
    {
        if (new_path.is_absolute() == false)
            new_path = std::filesystem::canonical(absolute(new_path));
    } 
    catch(const std::exception& ex)
    {
        std::cout << "cd: " << new_path.string() << ": No such file or directory" << std::endl;
        return;
    }

    if (std::filesystem::is_directory(new_path) == false) 
    {
        std::cout << "cd: " << new_path.string() << ": No such file or directory" << std::endl;
        return;
    }

    std::filesystem::current_path(new_path);
    _WORKING_DIR = new_path;
}


/**
 * Implements `echo` command.
 */
void Shell::__builtin_echo(std::string input) 
{
    std::string text, first_char = input.substr(0, 1);
    
    if (first_char != "'" && first_char != "\"")
    {
        text = remove_spaces(input);

        // handle `\` (just replace with next character)
        for (int i = 0; i < text.size(); i++)
        {
            if (text.at(i) == '\\')
            {
                text = text.replace(
                    i, 
                    2, 
                    std::string(1, text.at(i+1))
                );
            }
        }

        std::cout << text << std::endl;
        return;
    }

    std::vector<std::string> split_args;
    Parsing::split_arguments(&split_args, input);
    for (auto &arg : split_args)
    {
        first_char = arg.substr(0, 1);
        
        if (first_char == "'")
            // just removes single quotes '' from input
            text = arg.substr(1, arg.size()-2);
        else if (first_char == "\"")
            text = Parsing::eval(arg);
        else 
            text = arg;

        std::cout << text << " ";
    }
    std::cout << std::endl;
}


/**
 * Runs an external program located in PATH as a child process.
 * 
 * *Note: doesn't handle ./run.something*
 * 
 * @param input: full command (name + arguments)
 */
void Shell::__builtin_exec(std::string input) 
{
    if (input.size() == 0)
    {
        std::cout << "Error: empty input" << std::endl;
        return;
    }

    std::vector<std::string> split_args;
    Parsing::split_arguments(&split_args, input);

    std::string executable_path = __get_path(split_args.at(0), _PATH);
    if(executable_path.empty())
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

        // CreateProcessA takes the command in the format "<executable_path>" [args...]
        // where the executable_path is quoted to ensure the executable is found.
        // ex. ls -la -> "C:\Program Files\Git\usr\bin\ls" -la 
        executable_path = "\"" + executable_path + "\"";
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
            // see error codes at https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes
            std::cout << "Error creating process: " << GetLastError() << std::endl;
            return;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    #elif __linux__
        // use fork
        pid_t pid;
        int stdin_pipe[2], stdout_pipe[2];

        // if (pipe(stdin_pipe) == -1)
        // {
        //     std::cout << "Error creating stdin pipe" << std::endl;
        //     return;
        // }

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

            // close(stdin_pipe[READ_END]); // parent writes to stdin
            close(stdout_pipe[WRITE_END]); // parent reads from stdout

            // exec (in)
            while ((n_bytes = read(stdout_pipe[READ_END], child_stdout_buffer, BUF_SIZE)) > 0)
            {
                std::cout << std::flush;
                write(STDOUT_FILENO, child_stdout_buffer, n_bytes);
                std::cout << std::flush;
            }

            // close(stdin_pipe[WRITE_END]); // done taking input 
            close(stdout_pipe[READ_END]); // done reading

            waitpid(pid, &status, 0);
        }
        else if (pid == 0)
        {
            // close(stdin_pipe[WRITE_END]); //  son reads from stdin

            // handle standard output (son writes to stdout)
            if (dup2(stdout_pipe[WRITE_END], STDOUT_FILENO) == -1)
            {
                perror("dup2: stdout");
                exit(EXIT_FAILURE);
            }

            close(stdout_pipe[READ_END]); 
            close(stdout_pipe[WRITE_END]);

            // exec (out)
            // make arguments array for execv
            // note: execv doesn't handle quotes
            std::vector<char *> c_args;

            c_args.push_back(const_cast<char*>(executable_path.c_str()));
            for (size_t i = 1; i < split_args.size(); ++i)
            {
                if (split_args.at(i).front() == '\'' && split_args.at(i).back() == '\'')
                    split_args.at(i) = split_args.at(i).substr(1, split_args.at(i).size() - 2);
                if (split_args.at(i).front() == '\"' && split_args.at(i).back() == '\"')
                    split_args.at(i) = split_args.at(i).substr(1, split_args.at(i).size() - 2);
                
                c_args.push_back(const_cast<char*>(split_args.at(i).c_str()));
            }
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
void Shell::__builtin_type(std::string input) 
{
    if (_BUILTIN_FUNCTIONS.count(input) || input == "exit") 
    {
        std::cout << input << " is a shell builtin" << std::endl;
        return;
    }

    std::string executable_path = __get_path(input, _PATH);
    if (!executable_path.empty())
        std::cout << input << " is " << executable_path << std::endl;
    else
        std::cout << input << ": not found" << std::endl;
}


Shell::Shell()
{
    const char *path_raw = getenv("PATH");
    const char *home_dir = getenv("HOME");

    if (path_raw != nullptr) {
        std::string path_str = path_raw;
        split(&_PATH, path_str, PATH_DELIMITER);
    }

    if (home_dir != nullptr)
        _HOME_DIR = std::filesystem::path(home_dir);

    _WORKING_DIR = std::filesystem::current_path();


    // map command names to functions
    _BUILTIN_FUNCTIONS["cd"] = [this](std::string input){
        Shell::__builtin_change_directory(input);
    };
    _BUILTIN_FUNCTIONS["echo"] = [this](std::string input){
        Shell::__builtin_echo(input);
    };
    _BUILTIN_FUNCTIONS["exec"] = [this](std::string input){
        Shell::__builtin_exec(input);
    };
    _BUILTIN_FUNCTIONS["pwd"] = [this](std::string unused){
        std::cout << Shell::_WORKING_DIR.string() << std::endl;
    };
    _BUILTIN_FUNCTIONS["type"] = [this](std::string input){
        Shell::__builtin_type(input);
    };
}


void Shell::run(std::string command)
{
    std::string command_name, command_args;
    
    command_name = command.substr(0, command.find(" "));
    command_args = command.substr(command.find(" ")+1);

    // Search in BUILTIN
    if (_BUILTIN_FUNCTIONS.count(command_name)) 
        _BUILTIN_FUNCTIONS[command_name](command_args);
    else
    // Search in PATH, otherwise not found
    {
        std::string executable_path = __get_path(command_name, _PATH);
        if (!executable_path.empty())
            _BUILTIN_FUNCTIONS["exec"](command);
        else
            std::cout << command << ": command not found" << std::endl;
    }
}