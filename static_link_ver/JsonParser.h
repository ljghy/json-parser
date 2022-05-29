#ifndef JSON_PARSER_H_
#define JSON_PARSER_H_

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <memory>
#include <stdexcept>
#include <cassert>
#include <cstdint>
#include <cmath>

enum JsonParseErrCode
{
    JSON_EMPTY_BUFFER = 0,
    JSON_ROOT_NOT_SINGLE_VALUE,
    JSON_TOO_DEEP,

    JSON_INVALID_VALUE,

    JSON_INVALID_LITERAL,

    JSON_INVALID_NUMBER,
    JSON_NUMBER_OUT_OF_RANGE,

    JSON_INVALID_STRING_CHAR,
    JSON_INVALID_STRING_ESCAPE_SEQUENCE,
    JSON_INVALID_UNICODE_SURROGATE,
    JSON_UNEXPECTED_END_OF_STRING,
    JSON_INVALID_UNICODE_HEX,
    JSON_INVALID_UTF8_CHAR,

    JSON_MISS_COMMA_OR_SQUARE_BRACKET,
    JSON_TRAILING_COMMA,
    JSON_MISS_COMMA_OR_CURLY_BRACKET,
    JSON_INVALID_KEY_VALUE_PAIR,
    JSON_MISS_VALUE,
};

inline const std::string JsonParseErrMsg[]{
    "Empty buffer",
    "Root not single value",
    "Too deep",

    "Invalid value",

    "Invalid literal",

    "Invalid number",
    "Number out of range",

    "Invalid string char",
    "Invalid string escape sequence",
    "Invalid unicode surrogate",
    "Unexpected end of string",
    "Invalid unicode hex",
    "Invalid utf-8 char",

    "Miss comma or square bracket",
    "Trailing comma",
    "Miss comma or curly bracket",
    "Invalid key-value pair",
    "Miss value",
};

class JsonParseExcept
{
private:
    std::string m_whatStr;
    JsonParseErrCode m_code;
    size_t m_pos;

public:
    JsonParseExcept(const JsonParseErrCode &err,
                    const std::string::const_iterator &pos,
                    const std::string &buf);

    const char *what() const throw() { return m_whatStr.c_str(); }
    JsonParseErrCode code() const throw() { return m_code; }
    size_t pos() const throw() { return m_pos; }
};

class JsonNode;

struct JsonEmpty_t
{
};
constexpr struct JsonNull_t
{
} JsonNull;

using JsonBool_t = bool;

#ifndef JSON_PARSER_USE_FLOAT
using JsonNum_t = double;
#else
using JsonNum_t = float;
#endif

using JsonStr_t = std::string;
using JsonArr_t = std::vector<std::shared_ptr<JsonNode>>;
using JsonObj_t = std::unordered_multimap<JsonStr_t, std::shared_ptr<JsonNode>>;

inline const std::string JsonTypeStr[]{
    "empty",
    "null",
    "bool",
    "number",
    "string",
    "array",
    "object"};

inline constexpr uint16_t JSON_MAX_DEPTH = 20;
inline constexpr uint16_t JSON_DECIMAL_TO_STRING_PRECISION = 12;

class JsonNode
{
private:
    std::variant<JsonEmpty_t, JsonNull_t, JsonBool_t, JsonNum_t, JsonStr_t, JsonArr_t, JsonObj_t> m_value;
    enum
    {
        JsonEmptyType = 0,
        JsonNullType,
        JsonBoolType,
        JsonNumType,
        JsonStrType,
        JsonArrType,
        JsonObjType
    };

    static std::string _toHex4(uint64_t u);

    static std::string _decodeUTF8(std::string::const_iterator &ite,
                                   const std::string &src);

    static std::string _toJsonString(const std::string &src, bool decodeUTF8);

    std::string _toString(uint16_t depth, bool decodeUTF8) const;

public:
    JsonNode() : m_value(JsonEmpty_t()) {}

    template <typename ValType>
    JsonNode(const ValType &val) : m_value(val) {}

    JsonNode(const char *val) : m_value(JsonStr_t(val)) {}

    JsonNode(int8_t val) : m_value(static_cast<JsonNum_t>(val)) {}
    JsonNode(int16_t val) : m_value(static_cast<JsonNum_t>(val)) {}
    JsonNode(int32_t val) : m_value(static_cast<JsonNum_t>(val)) {}
    JsonNode(int64_t val) : m_value(static_cast<JsonNum_t>(val)) {}
    JsonNode(uint8_t val) : m_value(static_cast<JsonNum_t>(val)) {}
    JsonNode(uint16_t val) : m_value(static_cast<JsonNum_t>(val)) {}
    JsonNode(uint32_t val) : m_value(static_cast<JsonNum_t>(val)) {}
    JsonNode(uint64_t val) : m_value(static_cast<JsonNum_t>(val)) {}

    JsonNode(const std::initializer_list<std::shared_ptr<JsonNode>> &val) : m_value(JsonArr_t(val)) {}
    JsonNode(const std::initializer_list<std::pair<const JsonStr_t, std::shared_ptr<JsonNode>>> &val) : m_value(JsonObj_t(val)) {}

    JsonNode(const JsonNode &) = default;
    JsonNode(JsonNode &&other) : m_value(other.m_value) {}

    JsonNode &operator=(const JsonNode &) = default;
    JsonNode &operator=(JsonNode &&) = default;

    bool isEmpty() const { return m_value.index() == JsonEmptyType; }
    bool isNull() const { return m_value.index() == JsonNullType; }
    bool isBool() const { return m_value.index() == JsonBoolType; }
    bool isNum() const { return m_value.index() == JsonNumType; }
    bool isStr() const { return m_value.index() == JsonStrType; }
    bool isArr() const { return m_value.index() == JsonArrType; }
    bool isObj() const { return m_value.index() == JsonObjType; }

    bool operator==(bool b) const { return getBool() == b; }
    bool operator!=(bool b) const { return getBool() != b; }

    bool operator==(JsonNum_t n) const { return getNum() == n; }
    bool operator!=(JsonNum_t n) const { return getNum() != n; }
    bool operator>(JsonNum_t n) const { return getNum() > n; }
    bool operator<(JsonNum_t n) const { return getNum() < n; }
    bool operator>=(JsonNum_t n) const { return getNum() >= n; }
    bool operator<=(JsonNum_t n) const { return getNum() <= n; }

    bool operator==(const JsonStr_t &s) const { return getStr() == s; }
    bool operator!=(const JsonStr_t &s) const { return getStr() != s; }
    bool operator==(const char *s) const { return getStr() == JsonStr_t(s); }
    bool operator!=(const char *s) const { return getStr() != JsonStr_t(s); }

    JsonBool_t getBool() const;
    JsonBool_t &getBool();

    JsonNum_t getNum() const;
    JsonNum_t &getNum();
    template <typename IntType = int>
    IntType getInt() const
    {
        if (!isNum())
            throw std::runtime_error("Bad json access: expect number but is " + JsonTypeStr[m_value.index()]);
        return static_cast<IntType>(std::round(std::get<JsonNumType>(m_value)));
    }

    JsonStr_t getStr() const;
    JsonStr_t &getStr();

    JsonArr_t getArr() const;
    JsonArr_t &getArr();

    JsonObj_t getObj() const;
    JsonObj_t &getObj();

    const JsonNode &operator[](size_t idx) const;
    JsonNode &operator[](size_t idx);

    const JsonNode &operator[](const JsonStr_t &key) const;
    JsonNode &operator[](const JsonStr_t &key);
    bool find(const JsonStr_t &key) const;

    std::string toString(bool decodeUTF8 = true) const { return _toString(0, decodeUTF8); }

    JsonNode copy() const;
};

template <typename ValType>
static inline std::shared_ptr<JsonNode> jsptr(const ValType &val)
{
    return std::make_shared<JsonNode>(val);
}
static inline std::shared_ptr<JsonNode> jsptr(const char *val)
{
    return std::make_shared<JsonNode>(JsonStr_t(val));
}
static inline std::shared_ptr<JsonNode> jsptr(const std::initializer_list<std::shared_ptr<JsonNode>> &val)
{
    return std::make_shared<JsonNode>(JsonArr_t(val));
}
static inline std::shared_ptr<JsonNode> jsptr(const std::initializer_list<std::pair<const JsonStr_t, std::shared_ptr<JsonNode>>> &val)
{
    return std::make_shared<JsonNode>(JsonObj_t(val));
}

class JsonParser
{
private:
    JsonParser() = default;

    static JsonNode _parseValue(std::string::const_iterator &,
                                const std::string &,
                                uint16_t);

    static JsonNode _parseLiteral(std::string::const_iterator &,
                                  const std::string &);

    static JsonStr_t _parseString(std::string::const_iterator &,
                                  const std::string &);

    static JsonNode _parseNumber(std::string::const_iterator &,
                                 const std::string &);

    static JsonNode _parseArray(std::string::const_iterator &,
                                const std::string &,
                                uint16_t);

    static JsonNode _parseObject(std::string::const_iterator &,
                                 const std::string &,
                                 uint16_t);

    static void _skipSpace(std::string::const_iterator &,
                           const std::string &);

    static void _parseEscapeSequence(std::string &,
                                     std::string::const_iterator &,
                                     const std::string &);
    static void _parseUTF8(std::string &,
                           std::string::const_iterator &,
                           const std::string &);
    static uint64_t _parseHex4(std::string::const_iterator &,
                               const std::string &);
    static void _encodeUTF8(std::string &, uint64_t u);

    static std::pair<JsonStr_t, std::shared_ptr<JsonNode>>
    _parseKeyValue(std::string::const_iterator &,
                   const std::string &,
                   uint16_t);

public:
    static JsonNode parse(const std::string &);
};
#endif