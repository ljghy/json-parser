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