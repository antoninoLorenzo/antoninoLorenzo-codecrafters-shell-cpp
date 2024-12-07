#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>

// a template is like a java generics, with the difference 
// that its compiled instead than determined at runtime, for example:
// some_function(T input) is compiled two times if used both as
// some_function("Hello") and some_function(1)

void __builtin_echo(std::string input) {
    std::cout << input << std::endl;    
}


int main() {
    // Flush after every std::cout / std:cerr
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // unordered map should be more efficient for direct access
    std::unordered_map<
        std::string, 
        std::function<void(std::string)>
    > builtin_functions;

    // map lambda functions
    // [captures](parameters) -> return_type { body }
    builtin_functions["echo"] = [](std::string input){
        __builtin_echo(input);
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
        
        if (builtin_functions.count(command))
            builtin_functions[command](command_args);
        else
            std::cout << input << ": command not found" << std::endl;
    }
}
