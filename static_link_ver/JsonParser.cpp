#include "JsonParser.h"

#include <sstream>

JsonParseExcept::JsonParseExcept(const JsonParseErrCode &err,
                                 const std::string::const_iterator &pos,
                                 const std::string &buf)
    : m_code(err), m_pos(pos - buf.begin())
{
    m_whatStr = JsonParseErrMsg[static_cast<size_t>(err)];
    size_t p = 0, lineCount = 0;
    std::istringstream iss(buf);
    std::string line;
    while (p + buf.begin() < pos)
    {
        std::getline(iss, line);
        p += line.size() + 1;
        ++lineCount;
    }
    m_whatStr += " at line " + std::to_string(lineCount) + ':' + std::to_string((pos + line.size() + 2 - (p + buf.begin())));
}

std::string JsonNode::_toHex4(uint64_t u)
{
    char h[4];
    for (int8_t i = 3; i >= 0; --i)
    {
        h[i] = u & 0xF;
        h[i] = h[i] < 10 ? (h[i] + '0') : (h[i] - 10 + 'a');
        u >>= 4;
    }
    return "\\u" + std::string(h, 4);
}

std::string JsonNode::_decodeUTF8(std::string::const_iterator &ite,
                                  const std::string &src)
{
    uint64_t u = 0;
    if (!(*ite & 0x20))
    {
        u = ((*ite & 0x1F) << 6) | (*(ite + 1) & 0x3F);
        ++ite;
    }
    else if (!(*ite & 0x10))
    {
        u = ((*ite & 0xF) << 12) | ((*(ite + 1) & 0x3F) << 6) | (*(ite + 2) & 0x3F);
        ite += 2;
    }
    else
    {
        u = ((*ite & 0x7) << 18) | ((*(ite + 1) & 0x3F) << 12) | ((*(ite + 2) & 0x3F) << 6) | (*(ite + 3) & 0x3F);
        ite += 3;
    }

    if (u >= 0x10000)
    {
        uint64_t l = 0xDC00 + (u & 0x3FF);
        uint64_t u0 = ((u - 0x10000) >> 10) + 0xD800;
        if (u0 <= 0xDBFF)
        {
            return _toHex4(u0) + _toHex4(l);
        }
    }
    return _toHex4(u);
}

std::string JsonNode::_toJsonString(const std::string &src, bool decodeUTF8)
{
    std::stringstream ss;
    for (auto ite = src.begin(); ite != src.end(); ++ite)
    {
        switch (*ite)
        {
        case '"':
        case '\\':
        case '/':
            ss << '\\' << *ite;
            break;
        case '\b':
            ss << "\\b";
            break;
        case '\f':
            ss << "\\f";
            break;
        case '\n':
            ss << "\\n";
            break;
        case '\r':
            ss << "\\r";
            break;
        case '\t':
            ss << "\\t";
            break;
        default:
        {
            if (decodeUTF8 && *ite & 0x80)
                ss << _decodeUTF8(ite, src);
            else
                ss << *ite;
        }
        break;
        }
    }
    return ss.str();
}

std::string JsonNode::_toString(uint16_t depth, bool decodeUTF8) const
{
    if (depth >= JSON_MAX_DEPTH)
    {
        throw std::runtime_error("Json string conversion failure: json too deep");
        return "...";
    }
    switch (m_value.index())
    {
    case JsonEmptyType:
        return "";
    case JsonNullType:
        return "null";
    case JsonBoolType:
        return getBool() ? "true" : "false";
    case JsonNumType:
    {
        std::stringstream ss;
        ss.precision(JSON_DECIMAL_TO_STRING_PRECISION);
        ss << getNum();
        return ss.str();
    }
    case JsonStrType:
        return '"' + _toJsonString(getStr(), decodeUTF8) + '"';
    case JsonArrType:
    {
        JsonArr_t arr = getArr();
        if (arr.size() == 0)
            return "[]";
        std::stringstream ss;
        ss << '[';
        for (size_t i = 0; i != arr.size() - 1; ++i)
            ss << arr[i]->_toString(depth + 1, decodeUTF8) << ", ";
        ss << arr.back()->_toString(depth + 1, decodeUTF8) << ']';
        return ss.str();
    }
    default: // JsonObjType
    {
        JsonObj_t obj = getObj();
        if (obj.size() == 0)
            return "{}";
        size_t i = 0;
        std::stringstream ss;
        ss << '{';
        for (auto &each : obj)
        {
            ss << '"' << _toJsonString(each.first, decodeUTF8) << "\": " << each.second->_toString(depth + 1, decodeUTF8);
            ++i;
            if (i < obj.size())
                ss << ", ";
        }
        ss << '}';
        return ss.str();
    }
    }
}

JsonBool_t JsonNode::getBool() const
{
    if (!isBool())
        throw std::runtime_error("Bad json access: expect bool but is " + JsonTypeStr[m_value.index()]);
    return std::get<JsonBoolType>(m_value);
}
JsonBool_t &JsonNode::getBool()
{
    if (!isBool())
        throw std::runtime_error("Bad json access: expect bool but is " + JsonTypeStr[m_value.index()]);
    return std::get<JsonBoolType>(m_value);
}

JsonNum_t JsonNode::getNum() const
{
    if (!isNum())
        throw std::runtime_error("Bad json access: expect number but is " + JsonTypeStr[m_value.index()]);
    return std::get<JsonNumType>(m_value);
}
JsonNum_t &JsonNode::getNum()
{
    if (!isNum())
        throw std::runtime_error("Bad json access: expect number but is " + JsonTypeStr[m_value.index()]);
    return std::get<JsonNumType>(m_value);
}

JsonStr_t JsonNode::getStr() const
{
    if (!isStr())
        throw std::runtime_error("Bad json access: expect string but is " + JsonTypeStr[m_value.index()]);
    return std::get<JsonStrType>(m_value);
}
JsonStr_t &JsonNode::getStr()
{
    if (!isStr())
        throw std::runtime_error("Bad json access: expect string but is " + JsonTypeStr[m_value.index()]);
    return std::get<JsonStrType>(m_value);
}

JsonArr_t JsonNode::getArr() const
{
    if (!isArr())
        throw std::runtime_error("Bad json access: expect array but is " + JsonTypeStr[m_value.index()]);
    return std::get<JsonArrType>(m_value);
}
JsonArr_t &JsonNode::getArr()
{
    if (!isArr())
        throw std::runtime_error("Bad json access: expect array but is " + JsonTypeStr[m_value.index()]);
    return std::get<JsonArrType>(m_value);
}

JsonObj_t JsonNode::getObj() const
{
    if (!isObj())
        throw std::runtime_error("Bad json access: expect object but is " + JsonTypeStr[m_value.index()]);
    return std::get<JsonObjType>(m_value);
}
JsonObj_t &JsonNode::getObj()
{
    if (!isObj())
        throw std::runtime_error("Bad json access: expect object but is " + JsonTypeStr[m_value.index()]);
    return std::get<JsonObjType>(m_value);
}

const JsonNode &JsonNode::operator[](size_t idx) const
{
    const JsonArr_t &arr = getArr();
    if (idx >= arr.size())
        throw std::runtime_error("Bad json access: index out of range");
    return *(arr[idx]);
}
JsonNode &JsonNode::operator[](size_t idx)
{
    JsonArr_t &arr = getArr();
    if (idx >= arr.size())
        throw std::runtime_error("Bad json access: index out of range");
    return *(arr[idx]);
}

const JsonNode &JsonNode::operator[](const JsonStr_t &key) const
{
    const JsonObj_t &obj = getObj();
    auto ite = obj.find(key);
    if (ite == obj.end())
        throw std::runtime_error("Bad json access: key not found");
    return *(ite->second);
}
JsonNode &JsonNode::operator[](const JsonStr_t &key)
{
    JsonObj_t &obj = getObj();
    auto ite = obj.find(key);
    if (ite != obj.end())
        return *(ite->second);
    return *(obj.insert(std::make_pair(key, std::make_shared<JsonNode>(JsonNull)))->second);
}
bool JsonNode::find(const JsonStr_t &key) const
{
    const JsonObj_t &obj = getObj();
    return (obj.find(key) != obj.end());
}

JsonNode JsonNode::copy() const
{
    switch (m_value.index())
    {
    case JsonArrType:
    {
        JsonArr_t arr = getArr(), ret;
        ret.reserve(arr.size());
        for (auto &elem : arr)
            ret.push_back(std::make_shared<JsonNode>(elem->copy()));
        return ret;
    }
    case JsonObjType:
    {
        JsonObj_t obj = getObj(), ret;
        ret.reserve(obj.size());
        for (auto &p : obj)
            ret.insert({p.first, std::make_shared<JsonNode>(p.second->copy())});
        return ret;
    }
    default:
        return *this;
    }
}

JsonNode JsonParser::parse(const std::string &buffer)
{
    auto pos = buffer.begin();
    JsonNode ret = _parseValue(pos, buffer, 0);
    if (ret.isEmpty())
    {
        throw JsonParseExcept(JSON_EMPTY_BUFFER, pos, buffer);
    }
    if (pos != buffer.end())
    {
        throw JsonParseExcept(JSON_ROOT_NOT_SINGLE_VALUE, pos, buffer);
    }
    return ret;
}

inline JsonNode JsonParser::_parseValue(std::string::const_iterator &pos,
                                        const std::string &buf,
                                        uint16_t depth)
{
    if (depth >= JSON_MAX_DEPTH)
        throw JsonParseExcept(JSON_TOO_DEEP, pos, buf);
    _skipSpace(pos, buf);
    JsonNode ret = JsonEmpty_t();
    if (pos == buf.end())
        return ret;

    switch (*pos)
    {
    case 'n':
    case 't':
    case 'f':
        ret = _parseLiteral(pos, buf);
        break;
    case '"':
        ret = _parseString(pos, buf);
        break;
    case '[':
        ret = _parseArray(pos, buf, depth);
        break;
    case '{':
        ret = _parseObject(pos, buf, depth);
        break;
    default:
        if (std::isdigit(*pos) || (*pos == '-'))
            ret = _parseNumber(pos, buf);
        else
            throw JsonParseExcept(JSON_INVALID_VALUE, pos, buf);
        break;
    }
    _skipSpace(pos, buf);
    return ret;
}

inline std::pair<JsonStr_t, std::shared_ptr<JsonNode>>
JsonParser::_parseKeyValue(std::string::const_iterator &pos,
                           const std::string &buf,
                           uint16_t depth)
{
    _skipSpace(pos, buf);
    if (pos == buf.end() || *pos != '"')
    {
        if (*pos == '}')
            return {"", std::make_shared<JsonNode>(JsonEmpty_t())};
        throw JsonParseExcept(JSON_INVALID_KEY_VALUE_PAIR, pos, buf);
    }

    JsonStr_t key = _parseString(pos, buf);
    _skipSpace(pos, buf);
    if (pos == buf.end() || *pos != ':')
    {
        throw JsonParseExcept(JSON_INVALID_KEY_VALUE_PAIR, pos, buf);
    }
    ++pos;
    JsonNode node = _parseValue(pos, buf, depth + 1);
    return {key, std::make_shared<JsonNode>(node)};
}

inline JsonNode JsonParser::_parseObject(std::string::const_iterator &pos,
                                         const std::string &buf,
                                         uint16_t depth)
{
    assert((pos != buf.end()) && (*pos == '{'));
    ++pos;
    JsonObj_t obj;

    while (pos != buf.end())
    {
        auto keyValue = _parseKeyValue(pos, buf, depth);
        if (pos == buf.end())
        {
            throw JsonParseExcept(JSON_MISS_COMMA_OR_CURLY_BRACKET, pos, buf);
        }
        if (keyValue.second->isEmpty())
        {
            if (*pos == '}' && obj.empty())
            {
                ++pos;
                return obj;
            }
            if (*pos == '}')
                throw JsonParseExcept(JSON_TRAILING_COMMA, pos, buf);
            throw JsonParseExcept(JSON_MISS_VALUE, pos, buf);
        }
        obj.insert(keyValue);
        if (*pos == '}')
        {
            ++pos;
            return obj;
        }
        if (*pos == ',')
            ++pos;
        else
        {
            throw JsonParseExcept(JSON_MISS_COMMA_OR_CURLY_BRACKET, pos, buf);
        }
    }
    throw JsonParseExcept(JSON_MISS_COMMA_OR_CURLY_BRACKET, pos, buf);
}

inline JsonNode JsonParser::_parseArray(std::string::const_iterator &pos,
                                        const std::string &buf,
                                        uint16_t depth)
{
    assert((pos != buf.end()) && (*pos == '['));
    ++pos;
    JsonArr_t arr;

    while (pos != buf.end())
    {
        _skipSpace(pos, buf);
        if (pos == buf.end())
            throw JsonParseExcept(JSON_MISS_COMMA_OR_SQUARE_BRACKET, pos, buf);
        if (*pos == ']')
        {
            ++pos;
            if (!arr.empty())
                throw JsonParseExcept(JSON_TRAILING_COMMA, pos, buf);
            return arr;
        }
        JsonNode node = _parseValue(pos, buf, depth + 1);
        if (pos == buf.end())
            throw JsonParseExcept(JSON_MISS_COMMA_OR_SQUARE_BRACKET, pos, buf);

        arr.push_back(std::make_shared<JsonNode>(node));
        if (*pos == ']')
        {
            ++pos;
            return arr;
        }
        if (*pos == ',')
            ++pos;
        else
        {
            throw JsonParseExcept(JSON_MISS_COMMA_OR_SQUARE_BRACKET, pos, buf);
        }
    }
    throw JsonParseExcept(JSON_MISS_COMMA_OR_SQUARE_BRACKET, pos, buf);
}

inline void JsonParser::_encodeUTF8(std::string &dest, uint64_t u)
{
    if (u <= 0x7F)
        dest.push_back(u & 0xFF);
    else if (u <= 0x7FF)
    {
        dest.push_back(0xC0 | ((u >> 6) & 0xFF));
        dest.push_back(0x80 | (u & 0x3F));
    }
    else if (u <= 0xFFFF)
    {
        dest.push_back(0xE0 | ((u >> 12) & 0xFF));
        dest.push_back(0x80 | ((u >> 6) & 0x3F));
        dest.push_back(0x80 | (u & 0x3F));
    }
    else
    {
        dest.push_back(0xF0 | ((u >> 18) & 0xFF));
        dest.push_back(0x80 | ((u >> 12) & 0x3F));
        dest.push_back(0x80 | ((u >> 6) & 0x3F));
        dest.push_back(0x80 | (u & 0x3F));
    }
}

inline uint64_t JsonParser::_parseHex4(
    std::string::const_iterator &pos,
    const std::string &buf)
{
    assert(pos != buf.end());
    uint64_t u = 0;
    auto p = pos;
    for (; pos != buf.end() && pos < p + 4; ++pos)
    {
        if (pos == buf.end())
        {
            throw JsonParseExcept(JSON_UNEXPECTED_END_OF_STRING, pos, buf);
        }
        u <<= 4;
        if (*pos >= '0' && *pos <= '9')
            u |= *pos - '0';
        else if (*pos >= 'A' && *pos <= 'F')
            u |= *pos - 'A' + 10;
        else if (*pos >= 'a' && *pos <= 'f')
            u |= *pos - 'a' + 10;
        else
        {
            throw JsonParseExcept(JSON_INVALID_UNICODE_HEX, pos, buf);
        }
    }
    return u;
}

inline void JsonParser::_parseEscapeSequence(std::string &dest,
                                             std::string::const_iterator &pos,
                                             const std::string &buf)
{
    assert((pos != buf.end()) && (*pos == '\\'));
    ++pos;
    if (pos == buf.end())
    {
        throw JsonParseExcept(JSON_UNEXPECTED_END_OF_STRING, pos, buf);
    }
    switch (*pos)
    {
    case '"':
    case '\\':
    case '/':
        dest.push_back(*pos);
        ++pos;
        break;
    case 'b':
        dest.push_back('\b');
        ++pos;
        break;
    case 'f':
        dest.push_back('\f');
        ++pos;
        break;
    case 'n':
        dest.push_back('\n');
        ++pos;
        break;
    case 'r':
        dest.push_back('\r');
        ++pos;
        break;
    case 't':
        dest.push_back('\t');
        ++pos;
        break;
    case 'u':
    {
        ++pos;
        uint64_t u = _parseHex4(pos, buf);
        if (u >= 0xD800 && u <= 0xDBFF)
        {
            if (*pos != '\\' ||
                *(pos + 1) != 'u')
            {
                throw JsonParseExcept(JSON_INVALID_UNICODE_SURROGATE, pos, buf);
            }
            pos += 2;
            uint64_t l = _parseHex4(pos, buf);
            if (l < 0xDC00 || l > 0xDFFF)
            {
                throw JsonParseExcept(JSON_INVALID_UNICODE_SURROGATE, pos, buf);
            }
            u = (((u - 0xD800) << 10) | (l - 0xDC00)) +
                0x10000;
        }
        else if (u > 0xDBFF && u <= 0xDFFF)
        {
            throw JsonParseExcept(JSON_INVALID_UNICODE_SURROGATE, pos, buf);
        }
        _encodeUTF8(dest, u);
    }
    break;
    default:
        throw JsonParseExcept(JSON_INVALID_STRING_ESCAPE_SEQUENCE, pos, buf);
        ;
    }
}

inline void JsonParser::_parseUTF8(std::string &dest,
                                   std::string::const_iterator &pos,
                                   const std::string &buf)
{
    assert(pos != buf.end());
    uint8_t u = static_cast<uint8_t>(*pos), byteCount = 0;
    if ((u <= 0x7F))
        byteCount = 1;
    else if ((u >= 0xC0) && (u <= 0xDF))
        byteCount = 2;
    else if ((u >= 0xE0) && (u <= 0xEF))
        byteCount = 3;
    else if ((u >= 0xF0) && (u <= 0xF7))
        byteCount = 4;
    else
        throw JsonParseExcept(JSON_INVALID_UTF8_CHAR, pos, buf);

    dest.push_back(*pos);
    ++pos;
    for (auto p0 = pos; pos != buf.end() && pos < p0 + byteCount - 1; ++pos)
    {
        u = static_cast<uint8_t>(*pos);
        if ((u >= 0x80) && (u <= 0xBF))
            dest.push_back(*pos);
        else
            throw JsonParseExcept(JSON_INVALID_UTF8_CHAR, pos, buf);
    }
}

inline JsonStr_t JsonParser::_parseString(std::string::const_iterator &pos,
                                          const std::string &buf)
{
    assert((pos != buf.end()) && (*pos == '"'));
    ++pos;
    JsonStr_t str;
    while (pos != buf.end() && (*pos != '"'))
    {
        switch (*pos)
        {
        case '\\':
            _parseEscapeSequence(str, pos, buf);
            break;
        default:
            if (static_cast<uint8_t>(*pos) < 0x20)
            {
                throw JsonParseExcept(JSON_INVALID_STRING_CHAR, pos, buf);
            }
            _parseUTF8(str, pos, buf);
        }
    }
    ++pos;
    return str;
}

inline JsonNode JsonParser::_parseNumber(std::string::const_iterator &pos,
                                         const std::string &buf)
{
    assert(pos != buf.end());

    auto p = pos;
    p += (*p == '-');
    if (p == buf.end())
        throw JsonParseExcept{JSON_INVALID_NUMBER, pos, buf};
    if (*p == '0')
    {
        ++p;
    }
    else
    {
        if (!std::isdigit(*p) || *p == '0')
            throw JsonParseExcept{JSON_INVALID_NUMBER, pos, buf};
        for (; p != buf.end() && std::isdigit(*p); ++p)
            ;
    }
    if (p != buf.end() && *p == '.')
    {
        ++p;
        if (p == buf.end() || !std::isdigit(*p))
            throw JsonParseExcept{JSON_INVALID_NUMBER, pos, buf};
        for (; p != buf.end() && std::isdigit(*p); ++p)
            ;
    }
    if (p != buf.end() && (*p == 'e' || *p == 'E'))
    {
        ++p;
        if (p == buf.end())
            throw JsonParseExcept{JSON_INVALID_NUMBER, pos, buf};
        if (*p == '+' || *p == '-')
            ++p;
        if (p == buf.end())
            throw JsonParseExcept{JSON_INVALID_NUMBER, pos, buf};
        for (; p != buf.end() && std::isdigit(*p); ++p)
            ;
    }

    JsonNum_t ret;
    size_t idx = 0;
    const std::string &numStr = buf.substr(pos - buf.begin(), p - pos);
    try
    {
#ifndef JSON_PARSER_USE_FLOAT
        ret = std::stod(numStr, &idx);
#else
        ret = std::stof(numStr, &idx);
#endif
    }
    catch (const std::invalid_argument &)
    {
        throw JsonParseExcept(JSON_INVALID_NUMBER, pos, buf);
    }
    catch (const std::out_of_range &)
    {
        throw JsonParseExcept(JSON_NUMBER_OUT_OF_RANGE, pos, buf);
    }
    pos += idx;
    return ret;
}

inline JsonNode JsonParser::_parseLiteral(std::string::const_iterator &pos,
                                          const std::string &buf)
{
    assert((pos != buf.end()) && (*pos == 'n' || *pos == 't' || *pos == 'f'));
    if (buf.end() - pos < 4)
    {
        throw JsonParseExcept(JSON_INVALID_LITERAL, pos, buf);
    }
    if (*pos == 'n' && *(pos + 1) == 'u' && *(pos + 2) == 'l' && *(pos + 3) == 'l')
    {
        pos += 4;
        return JsonNull;
    }
    if (*pos == 't' && *(pos + 1) == 'r' && *(pos + 2) == 'u' && *(pos + 3) == 'e')
    {
        pos += 4;
        return JsonBool_t(true);
    }
    if (buf.end() - pos < 5)
    {
        throw JsonParseExcept(JSON_INVALID_LITERAL, pos, buf);
    }
    if (*pos == 'f' && *(pos + 1) == 'a' && *(pos + 2) == 'l' && *(pos + 3) == 's' && *(pos + 4) == 'e')
    {
        pos += 5;
        return JsonBool_t(false);
    }
    throw JsonParseExcept(JSON_INVALID_LITERAL, pos, buf);
}

inline void JsonParser::_skipSpace(std::string::const_iterator &pos,
                                   const std::string &buf)
{
    while (pos != buf.end() && ((*pos == ' ') ||
                                (*pos == '\t') ||
                                (*pos == '\n') ||
                                (*pos == '\r')))
        ++pos;
}