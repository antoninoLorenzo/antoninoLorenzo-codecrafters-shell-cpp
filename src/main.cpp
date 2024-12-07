#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>


// unordered map should be more efficient for direct access
std::unordered_map<
    std::string, 
    std::function<void(std::string)>
> BUILTIN_FUNCTIONS;

// a template is like a java generics, with the difference 
// that its compiled instead than determined at runtime, for example:
// some_function(T input) is compiled two times if used both as
// some_function("Hello") and some_function(1)

void __builtin_echo(std::string input) {
    std::cout << input << std::endl;    
}

/**
 * Implements `type` command, used to determine how a command would
 * be interpreted.
 * 
 * Examples:
 * 
 * $ type echo
 * echo is a shell builtin
 * 
 * $ type something
 * invalid command: not found
 * 
 */
void __builtin_type(std::string input) {
    if (BUILTIN_FUNCTIONS.count(input) || input == "exit")
        std::cout << input << " is a shell builtin" << std::endl;
    else
        std::cout << input << ": not found" << std::endl;
}


int main() {
    // Flush after every std::cout / std:cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // map lambda functions
    // [captures](parameters) -> return_type { body }
    BUILTIN_FUNCTIONS["echo"] = [](std::string input){
        __builtin_echo(input);
    };
    BUILTIN_FUNCTIONS["type"] = [](std::string input){
        __builtin_type(input);
    };

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
