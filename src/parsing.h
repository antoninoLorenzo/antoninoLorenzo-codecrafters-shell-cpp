#ifndef PARSING_H
#define PARSING_H

#include <string>
#include <vector>

namespace Parsing 
{
    void split_arguments(std::vector<std::string> *argv, std::string command);

    std::string eval(std::string command_args);
}

#endif