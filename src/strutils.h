#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <string>
#include <vector>
#include <sstream>


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


std::string remove_spaces(std::string &s)
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

#endif