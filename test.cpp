#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include "JsonParser.hpp"

int main()
{
    JsonParser parser;
    JsonNode json;
    std::string dir = "tests/JSONTestSuite/test_parsing/";
    for (const auto &dirEntry : std::filesystem::directory_iterator(dir))
    {
        std::stringstream ss;
        ss << dirEntry;
        bool success = true;
        std::string filename = dir + ss.str().substr(dir.size() + 1, ss.str().size() - dir.size() - 2);
        try
        {
            std::ifstream fin(filename, std::ios::binary);
            json = parser.parse(fin);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Fail: " << filename << ": " << e.what() << '\n';
            success = false;
        }
        if (success)
        {
            std::cerr << "Success: " << filename << "\n";
            std::cerr << json.toString() << '\n';
        }
    }
}