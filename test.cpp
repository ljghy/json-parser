#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include "JsonParser.hpp"

std::string loadFromFile(const std::string &filename)
{
    std::ifstream ifs(filename, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>{ifs}, {});
}

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
            json = parser.parse(loadFromFile(filename));
        }
        catch (const JsonParseExcept &e)
        {
            std::cerr << "Fail: " << filename << ": " << e.what() << '\n';
            success = false;
        }
        if (success)
        {
            std::cerr << "Success: " << filename << "\n";
            std::cerr << json.toString(false) << '\n';
        }
    }
}