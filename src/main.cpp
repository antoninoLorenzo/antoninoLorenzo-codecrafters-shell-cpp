#include <iostream>
#include <string>
#include <sstream>
#include <regex>
#include <vector>

#include "shell.h"

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
#else __linux__
    // ...
    std::string __run_process(
        const char *path, 
        char *const argv[]
    )
    {
        return std::string("");
    }
#endif


std::string strip_quotes(const std::string &arg) {
    if (arg.size() >= 2 && arg.front() == '\'' && arg.back() == '\'') {
        return arg.substr(1, arg.size() - 2);
    }
    return arg;
}


int main()
{
    Shell s = Shell();
    while (true) 
    {
        std::string input;

        std::cout << "$ ";
        std::getline(std::cin, input);
        if (input.compare(0, 4, "exit") == 0)
            break;
        
        s.run(input);
    }
}