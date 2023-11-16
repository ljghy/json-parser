#include "JsonParser.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

int main() {
  JsonParser parser;
  JsonNode json;
  std::string dir = "tests/JSONTestSuite/test_parsing/";

  int totalTests = 0;
  int passedTests = 0;
  for (const auto &dirEntry : std::filesystem::directory_iterator(dir)) {
    ++totalTests;
    std::stringstream ss;
    ss << dirEntry;
    bool success = true;
    std::string filename =
        ss.str().substr(dir.size() + 1, ss.str().size() - dir.size() - 2);
    std::string path = dir + filename;
    try {
      std::ifstream fin(path, std::ios::binary);
      json = parser.parse(fin);
    } catch (const std::exception &e) {
      success = false;
    }
    if ((success && filename[0] == 'n') || (!success && filename[0] == 'y')) {
      std::cerr << "Unexpected parsing result: " << filename << '\n';
    } else {
      ++passedTests;
    }
  }
  std::cerr << "Passed " << passedTests << " out of " << totalTests << '\n';
}