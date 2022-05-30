#include "JsonParser.hpp"
#include <iostream>
int main()
{
    std::string jsonStr = "{\"name\": \"sphere\", \"center\": [1.0, 2.0, 3.0], \"radius\": 0.5}";
    JsonParser parser;
    // parse
    JsonNode json1;
    try
    {
        json1 = parser.parse(jsonStr);
    }
    catch (const JsonParseExcept &e)
    {
        std::cerr << e.what() << std::endl;
    }

    // get value
    std::cout << json1["name"].getStr() << std::endl;
    std::cout << json1["center"][0].getNum() << ' '
              << json1["center"][1].getNum() << ' '
              << json1["center"][2].getNum() << std::endl;
    std::cout << json1["radius"].getNum() << std::endl;

    // set value
    json1["radius"] = 5;
    json1["center"] = {jsptr(4), jsptr(5), jsptr(6)};
    json1["center"][0] = 7.5;
    json1["center"][1] = json1.copy(); // deep copy
    json1["tag"] = "object1";          // new key
    std::cout << json1.toString() << std::endl;

    JsonNode json2 = json1; // shallow copy
    json2["tag"] = "object2";
    std::cout << json1.toString() << std::endl;

    // construct
    JsonNode json3 = {{"key", jsptr("value")},
                      {"false", jsptr(false)},
                      {"null", jsptr(JsonNull)},
                      {"arr", jsptr({jsptr(1), jsptr("2"), jsptr({jsptr(3)})})}};
    if (json3.find("arr"))
        std::cout << json3["arr"].toString() << std::endl;
    std::cout << json3.toString() << std::endl;

    // parse utf-8 encoded string
    jsonStr = reinterpret_cast<const char *>(+u8"\"z\u00df\u6c34\U0001f34c\"");
    JsonNode json4 = parser.parse(jsonStr);
    std::cout << json4.toString() << std::endl; // decode utf-8

    JsonNode json5 = parser.parse("[\"z\\u00df\\u6c34\\ud83c\\udf4c\"]");
    std::cout << json5.toString(false) << std::endl; // utf-8 encoded
}