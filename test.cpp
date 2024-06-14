#include "JsonParser.hpp"
#include <filesystem>
#include <iostream>

int main() {
  JsonNode json;
  std::string dir = "tests/JSONTestSuite/test_parsing/";

  int totalTests = 0;
  int passedTests = 0;
  for (const auto &path : std::filesystem::directory_iterator(dir)) {
    ++totalTests;
    std::stringstream ss;
    bool success = true;
    try {
      json = parseJsonFile(path);
    } catch (const std::exception &e) {
      success = false;
    }
    std::string filename = path.path().filename().string();
    if ((success && filename[0] == 'n') || (!success && filename[0] == 'y')) {
      std::cerr << "Unexpected parsing result: " << filename << '\n';
    } else {
      ++passedTests;
    }
  }
  std::cerr << "Passed " << passedTests << " out of " << totalTests << '\n';
}