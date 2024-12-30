#include "parsing.h"


void Parsing::split_arguments(
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

std::string Parsing::eval(std::string command_args)
{
    size_t pos_s, pos_e, start = 0;
    std::string s = command_args;
    const char *metachars[] = {"$", "\\", "\""};

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
        //replacement = __remove_spaces(replacement);

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
