# Json Parser

## Run test

```
g++ test.cpp -o test -std=c++17
./test
```

## Example

```c++
#include "JsonParser.hpp"
#include <iostream>
int main() {
  std::string jsonStr =
      "{\"name\": \"sphere\", \"center\": [1.0, 2.0, 3.0], \"radius\": 0.5}";
  // parse
  JsonNode json1;
  try {
    json1 = parseJsonString(jsonStr);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }

  // get value
  std::cout << json1["name"].str() << std::endl;
  std::cout << json1["center"][0].get<float>() << ' '
            << json1["center"][1].get<int>() << ' '
            << json1["center"][2].get<size_t>() << std::endl;
  std::cout << json1["radius"].get<double>() << std::endl;

  // set value
  json1["radius"] = 5;
  json1["center"] = {4, 5, 6};
  json1["center"][0] = 7.5;
  json1["center"][1] = json1;
  json1["tag"] = "object1"; // new key
  std::cout << json1 << std::endl;

  JsonNode json2 = json1;
  json2["tag"] = "object2";
  std::cout << json1.serializer().indent(2) << std::endl;

  // construct
  JsonNode json3 = {{"key"_key, "value"},
                    {"false"_key, false},
                    {"null"_key, JsonNull},
                    {"arr"_key, {1, "2", {3}}}};
  if (json3.contains("arr"))
    std::cout << json3["arr"] << std::endl;
  std::cout << json3 << std::endl;

  // parse utf-8 encoded string
  jsonStr = reinterpret_cast<const char *>(+u8"\"z\u00df\u6c34\U0001f34c\"");
  JsonNode json4 = parseJsonString(jsonStr);
  std::cout << json4 << std::endl; // decode utf-8

  JsonNode json5 = parseJsonString("[\"z\\u00df\\u6c34\\ud83c\\udf4c\"]");
  std::cout << json5.serializer().indent(4).ascii(false)
            << std::endl; // utf-8 encoded

  std::cout << "Empty: " << JsonNode{} << std::endl;
  std::cout << "Null: " << JsonNode(JsonNull) << std::endl;
  std::cout << "Empty array: " << JsonNode(JsonArr_t{}) << std::endl;
  std::cout << "Empty object: " << JsonNode(JsonObj_t{}) << std::endl;

  JsonNode json6{3.14159, 2.71828, json5};
  std::cout << json6.serializer().indent(2).ascii(true).precision(3)
            << std::endl;

  std::string_view multipleJsonStr = R"(
    null
    true false
    123 456.789
    "abc"
    [1, 2, 3]
    {"a": 1, "b": 2}
  )";
  size_t pos = 0;
  JsonNode json7;
  while (pos < multipleJsonStr.size()) {
    auto j = parseJsonString(multipleJsonStr, &pos);
    json7.push_back(std::move(j));
  }
  std::cout << json7.serializer().indent(2) << std::endl;
}
```

```c++
#include "JsonParser.hpp"
#include <Eigen/Core>
#include <glm/glm.hpp>
#include <iostream>
#include <unordered_map>

int main() {
  try {
    auto jvec = parseJsonString("[1.2, 2.3, 3.4, 4.5]");
    auto jmat =
        parseJsonString("[[1.2, 2.3, 3.4], [4.5, 5.6, 6.7], [7.8, 8.9, 9.0]]");

    Eigen::VectorXd vec_eigen = jvec.get<Eigen::VectorXd>();
    glm::vec4 vec_glm = jvec.get<glm::vec4>();
    auto mat = jmat.get<std::vector<std::array<double, 3>>>();

    std::cout << "Eigen Vector: " << vec_eigen.transpose() << std::endl;
    std::cout << "GLM Vector: " << vec_glm.x << ", " << vec_glm.y << ", "
              << vec_glm.z << ", " << vec_glm.w << std::endl;

    for (const auto &row : mat) {
      for (const auto &elem : row) {
        std::cout << elem << " ";
      }
      std::cout << std::endl;
    }

    JsonNode jvec2 = vec_eigen;
    JsonNode jmat2 = mat;

    std::cout << jvec2 << std::endl;
    std::cout << jmat2 << std::endl;

    JsonNode jmap{
        {"one"_key, {1, 1}}, {"two"_key, {2, 2}}, {"three"_key, {3, 3}}};

    auto map = jmap.get<std::unordered_map<std::string, glm::vec2>>();
    for (const auto &p : map) {
      std::cout << p.first << ": " << p.second[0] << ", " << p.second[1]
                << std::endl;
    }

    JsonNode jmap2 = map;
    std::cout << jmap2 << std::endl;

  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
}
```
