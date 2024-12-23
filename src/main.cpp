#include <iostream>
#include <string>
#include <sstream>
#include <regex>
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
fs::path HOME_DIR;

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

std::string __get_path(
    std::string executable_name
)
{
    std::string executable_path("");
    for (int i = 0; i < PATH.size(); i++) 
    {
        if (__search_file(PATH.at(i), executable_name)) 
        {
            executable_path = PATH.at(i) + PATH_SEPARATOR + executable_name;
            break;
        }
    }
    return executable_path;
}

/**
 * Extracts commands enclosed in $() from a given string and stores them in a vector.
 */
void __extract_commands(
    std::vector<std::string> *commands,
    std::string               s
)
{
    std::regex command_regex("\\$\\((.*?)\\)");
    std::smatch command_match;
    
    auto search_start = s.cbegin();
    while (std::regex_search(search_start, s.cend(), command_match, command_regex)) {
        commands->push_back(command_match[1]);
        search_start = command_match.suffix().first;
    }
}


std::string __remove_spaces(std::string s)
{
    std::string out("");
    std::vector<std::string> words;
    split(&words, s, " ");
    for (int i = 0; i < words.size(); i++)
    {
        if (!words.at(i).empty() && words.at(i) != " ")
        {
            out = out + words.at(i) + " ";
        }
    } 

    return out;
}

/**
 * Parses a string to perform variable expansion.
 * Variable expansion is triggered when the input is enclosed in "..." or not enclosed at all (todo).
 */
std::string __eval(std::string command_args)
{
    size_t pos_s, pos_e, start = 0;
    std::string s = command_args;

    // process "..."
    while (true)
    {
        pos_s = s.find("\"", start);
        if (pos_s == std::string::npos)
            break;
            
        pos_e = s.find("\"", pos_s + 1);
        if (pos_e == std::string::npos) 
            break;
        
        // evalute substr and replace with the result
        std::string replacement = s.substr(pos_s+1, pos_e - pos_s - 1);
        replacement = __remove_spaces(replacement);

        // evaluate content
        // + (DONE) step 1: extract commands 
        // std::vector<std::string> commands;
        // __extract_commands(&commands, replacement);
        //
        // + step 2: execute commands and store outputs
        // Note: 
        //  __builtin_exec prints output, a version that just returns output should be done.
        //  - for Windows: pipes should be introduced and STARTUPINFO.hStdOutput/Error should be
        //  set to the pipe.
        //  - for UNIX: the line `write(STDOUT_FILENO, child_stdout_buffer, n_bytes);` should be
        //  replaced to write in a buffer that is then returned.
        //  > Consider writing "reusable code"
        //
        // if (commands.size())
        //    for (auto cmd : commands)
        // 
        // + step 3 : replace sections of commands with outputs
        // ... 

        s.replace(pos_s, pos_e - pos_s + 1, replacement);
        
        start = pos_e - 1;
    }

    return s;
}


/**
 * Implements `echo` command.
 */
void __builtin_echo(std::string input) 
{
    std::string text;
    std::string first_char = input.substr(0, 1);
    if (first_char != "'" && first_char != "\"")
        text = __remove_spaces(input);
    else if (first_char == "'")
        // just removes single quotes '' from input
        text = input.substr(1, input.size()-2);
    else
        text = __eval(input);

    std::cout << text << std::endl;
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

    std::string executable_path = __get_path(input);
    if (!executable_path.empty())
        std::cout << input << " is " << executable_path << std::endl;
    else
        std::cout << input << ": not found" << std::endl;
}

// Note: to implement reusable __run_process there is the need to use iterators, this way:
// - need to print directly (__builtin_exec) : just print while the output comes
// - need string output     (__eval)         : store the output as it comes
#ifdef _WIN32
    /**
     * 
     */
    std::string __run_process(
        LPSTR lp_input
    )
    {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        HANDLE stdout_pipe_r, stdout_pipe_w;
        HANDLE stderr_pipe_r, stderr_pipe_w;
        BOOL create_flag;

        create_flag = CreatePipe(&stdout_pipe_r, &stdout_pipe_w, NULL, 0);
        if (create_flag == FALSE)
        {
            std::cout << "Error creating stdout pipe: " << GetLastError() << std::endl;
            return std::string("");
        }

        create_flag = CreatePipe(&stderr_pipe_r, &stderr_pipe_w, NULL, 0);
        if (create_flag == FALSE)
        {
            std::cout << "Error creating stderr pipe: " << GetLastError() << std::endl;
            return std::string("");
        }

        SecureZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = stdout_pipe_w;
        si.hStdError = stderr_pipe_w;
        SecureZeroMemory(&pi, sizeof(pi));

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
            return std::string("");
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        
        CloseHandle(stdout_pipe_r);
        CloseHandle(stdout_pipe_w);
        CloseHandle(stderr_pipe_r);
        CloseHandle(stderr_pipe_w);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
#elif __linux__
    // ...
    std::string __run_process(
        const char *path, 
        char *const argv[]
    )
    {
        return std::string("");
    }
#endif


void split_arguments(
    std::vector<std::string> *argv,
    std::string command
)
{
    std::string buffer("");
    // 0: in single quotes, 1: in double quotes
    int state[2] = {0}, SINGLE = 0, DOUBLE = 1;

    for (int i = 0; i < command.size(); i++)
    {
        char c = command[i];

        if (c == '\'' && !state[DOUBLE]) 
        {
            state[SINGLE] = !state[SINGLE];
            buffer += c;
        }
        else if (c == '"' && !state[SINGLE])
        {
            state[DOUBLE] = !state[DOUBLE];
            buffer += c; 
        }
        else if (state[SINGLE] || state[DOUBLE])
        {
            buffer += c;
        }
        else if (c == ' ') 
        {
            if (!buffer.empty())
            {
                argv->push_back(buffer);
                buffer.clear();
            }
        }
        else
            buffer += c;
    }

    if (!buffer.empty())
        argv->push_back(buffer);
}


std::string strip_quotes(const std::string &arg) {
    if (arg.size() >= 2 && arg.front() == '\'' && arg.back() == '\'') {
        return arg.substr(1, arg.size() - 2);
    }
    return arg;
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
    split_arguments(&split_args, input);

    std::string executable_path = __get_path(split_args.at(0));
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
                
                c_args.push_back(const_cast<char*>(split_args.at(i).c_str()));
            }
            c_args.push_back(nullptr);

            /*
            std::string test("+ command: ");
            for (auto &arg : c_args)
            {
                if (arg == nullptr)
                    break;
                
                test = test + std::string(arg) + " ";
                
            }
            test = test + "+\n";
            std::cout << test;
            */
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
 * Implements `cd` command.
 * 
 * Handles:
 * - absolute paths
 * - relative paths
 * - user home (~)
 */
void __builtin_change_directory(std::string input)
{
    if (input == "~")
    {
        fs::current_path(HOME_DIR);
        WORKING_DIR = HOME_DIR;
        return;
    }

    fs::path new_path(input);
    if (new_path.is_absolute() == false)
        new_path = fs::canonical(absolute(new_path));

    if (fs::is_directory(new_path) == false) 
    {
        std::cout << "cd: " << new_path.string() << ": No such file or directory" << std::endl;
        return;
    }

    fs::current_path(new_path);
    WORKING_DIR = new_path;
}


int main() 
{
    // parse PATH
    // Microsoft specific alternative: _dupenv_s()
    const char *path_raw = getenv("PATH");
    if (path_raw != nullptr) {
        std::string path_str = path_raw;
        split(&PATH, path_str, PATH_DELIMITER);
    }

    // get HOME directory
    const char *home_dir = getenv("HOME");
    if (home_dir != nullptr)
        HOME_DIR = fs::path(home_dir);

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
        std::cout << WORKING_DIR.string() << std::endl;
    };
    BUILTIN_FUNCTIONS["cd"] = [](std::string input){
        __builtin_change_directory(input);
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

        // evaluate command args
        // echo "some" = echo 'some' = some
        // echo "$(var)" = var value
        // echo '$(var)' = $(var)
        
        // Search in BUILTIN
        if (BUILTIN_FUNCTIONS.count(command)) 
        {
            BUILTIN_FUNCTIONS[command](command_args);
            continue;
        }
        
        // Search in PATH, otherwise not found
        std::string executable_path = __get_path(command);
        if (!executable_path.empty())
            BUILTIN_FUNCTIONS["exec"](__eval(input));
        else
            std::cout << command << ": command not found" << std::endl;
    }
}
