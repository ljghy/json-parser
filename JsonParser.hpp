#ifndef JSON_PARSER_HPP_
#define JSON_PARSER_HPP_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

struct JsonNull_t {
  friend constexpr JsonNull_t JsonNull();

private:
  JsonNull_t() = default;
};

inline constexpr JsonNull_t JsonNull() { return JsonNull_t{}; }

class JsonNode;

using JsonNum_t = double;
using JsonStr_t = std::string;
using JsonArr_t = std::vector<JsonNode>;
using JsonObj_t = std::map<std::string, JsonNode>;

struct JsonKeyLiteral_t {
  JsonStr_t key;
};

inline JsonKeyLiteral_t operator""_key(const char *key, size_t len) {
  return JsonKeyLiteral_t{JsonStr_t(key, len)};
}

enum class JsonType : uint8_t {
  Empty = 0,
  Null,
  Bool,
  Num,
  Str,
  Arr,
  Obj,
};

enum class JsonErrorCode : uint8_t {
  InvalidJson = 0,
  UnexpectedEndOfInput,
  InvalidLiteral,
  InvalidNumber,
  InvalidString,
  InvalidCharacter,
  InvalidUnicode,
  InvalidEscapeSequence,
  InvalidKeyValuePair,
  InvalidArrayOrObject,
};

inline const char *JsonErrorMsg[]{
    "invalid json",           "unexpected end of input",
    "invalid literal",        "invalid number",
    "invalid string",         "invalid character",
    "invalid unicode",        "invalid escape sequence",
    "invalid key-value pair", "invalid array or object",
};

inline const char *getJsonErrorMsg(JsonErrorCode code) {
  return JsonErrorMsg[static_cast<uint8_t>(code)];
}

template <typename T> struct isJsonType {
  static constexpr bool value =
      std::is_same_v<T, JsonNull_t> || std::is_same_v<T, bool> ||
      std::is_same_v<T, JsonNum_t> || std::is_same_v<T, JsonStr_t> ||
      std::is_same_v<T, JsonArr_t> || std::is_same_v<T, JsonObj_t>;
};

template <typename T>
inline T strToJsonNum_t(const std::string &, size_t *idx) {
  return T{};
}
template <>
inline double strToJsonNum_t<double>(const std::string &str, size_t *idx) {
  return std::stod(str, idx);
}
template <>
inline float strToJsonNum_t<float>(const std::string &str, size_t *idx) {
  return std::stof(str, idx);
}
template <>
inline long double strToJsonNum_t<long double>(const std::string &str,
                                               size_t *idx) {
  return std::stold(str, idx);
}
class JsonNode {
  friend class JsonParser;

private:
  struct TraverseState {
    JsonNode *node;
    union {
      JsonArr_t::iterator arrIt;
      JsonObj_t::iterator objIt;
    };

    TraverseState(JsonNode *node) : node(node) {
      if (node->isArr())
        arrIt = std::get<JsonArr_t>(node->m_value).begin();
      else if (node->isObj())
        objIt = std::get<JsonObj_t>(node->m_value).begin();
    }
  };

  struct ConstTraverseState {
    const JsonNode *node;
    union {
      JsonArr_t::const_iterator arrIt;
      JsonObj_t::const_iterator objIt;
    };

    ConstTraverseState(const JsonNode *node) : node(node) {
      if (node->isArr())
        arrIt = std::get<JsonArr_t>(node->m_value).cbegin();
      else if (node->isObj())
        objIt = std::get<JsonObj_t>(node->m_value).cbegin();
    }
  };

public:
  // Constructors

  JsonNode() = default;

  template <typename T,
            typename = std::enable_if_t<isJsonType<T>::value, JsonNode &>>
  JsonNode(const T &value) : m_value(value) {}

  template <typename T,
            typename = std::enable_if_t<isJsonType<T>::value, JsonNode &>>
  JsonNode(T &&value) : m_value(value) {}

  template <typename T, typename = std::enable_if_t<!isJsonType<T>::value &&
                                                    std::is_arithmetic_v<T>>>
  JsonNode(T num) : m_value(static_cast<JsonNum_t>(num)) {}

  JsonNode(const char *str) : m_value(JsonStr_t(str)) {}

  JsonNode(const std::initializer_list<JsonNode> &arr)
      : m_value(JsonArr_t(arr)) {}
  JsonNode(
      const std::initializer_list<std::pair<const JsonKeyLiteral_t, JsonNode>>
          &obj) {
    m_value = JsonObj_t{};
    for (const auto &p : obj) {
      std::get<JsonObj_t>(m_value).emplace(p.first.key, p.second);
    }
  }

  JsonNode(const JsonNode &other) {
    std::stack<ConstTraverseState> stateStack;
    std::stack<JsonNode *> nodeStack;
    stateStack.emplace(&other);
    nodeStack.emplace(this);
    while (!stateStack.empty()) {
      auto otherNode = stateStack.top().node;
      auto node = nodeStack.top();
      switch (otherNode->type()) {
      case JsonType::Arr: {
        auto &otherArr = std::get<JsonArr_t>(otherNode->m_value);
        auto &it = stateStack.top().arrIt;
        if (it == otherArr.cbegin()) {
          node->m_value.emplace<JsonArr_t>().reserve(otherArr.size());
        }
        if (it != otherArr.cend()) {
          const auto otherChild = &(*it);
          ++it;
          stateStack.emplace(otherChild);
          nodeStack.emplace(
              &(std::get<JsonArr_t>(node->m_value).emplace_back()));
        } else {
          stateStack.pop();
          nodeStack.pop();
        }
      } break;
      case JsonType::Obj: {
        auto &otherObj = std::get<JsonObj_t>(otherNode->m_value);
        auto &it = stateStack.top().objIt;
        if (it == otherObj.cbegin()) {
          node->m_value.emplace<JsonObj_t>();
        }
        if (it != otherObj.cend()) {
          const auto otherChild = &(it->second);
          stateStack.emplace(otherChild);
          nodeStack.emplace(&(std::get<JsonObj_t>(node->m_value)
                                  .emplace(it->first, JsonNode{})
                                  .first->second));
          ++it;
        } else {
          stateStack.pop();
          nodeStack.pop();
        }
      } break;
      case JsonType::Null:
        nodeStack.top()->m_value.emplace<JsonNull_t>(JsonNull());
        stateStack.pop();
        nodeStack.pop();
        break;
      case JsonType::Bool:
        nodeStack.top()->m_value.emplace<bool>(otherNode->get<bool>());
        stateStack.pop();
        nodeStack.pop();
        break;
      case JsonType::Num:
        nodeStack.top()->m_value.emplace<JsonNum_t>(
            otherNode->get<JsonNum_t>());
        stateStack.pop();
        nodeStack.pop();
        break;
      case JsonType::Str:
        nodeStack.top()->m_value.emplace<JsonStr_t>(
            otherNode->get<JsonStr_t>());
        stateStack.pop();
        nodeStack.pop();
        break;
      default:
        stateStack.pop();
        nodeStack.pop();
      }
    }
  }

  JsonNode(JsonNode &&other) : m_value(std::move(other.m_value)) {}

  // Destructor

  ~JsonNode() {
    std::stack<TraverseState> stateStack;
    stateStack.emplace(this);
    while (!stateStack.empty()) {
      auto node = stateStack.top().node;
      switch (node->type()) {
      case JsonType::Arr: {
        auto &arr = std::get<JsonArr_t>(node->m_value);
        auto &it = stateStack.top().arrIt;
        if (it != arr.cend()) {
          const auto child = &(*it);
          ++it;
          stateStack.emplace(child);
        } else {
          arr.clear();
          stateStack.pop();
        }
      } break;
      case JsonType::Obj: {
        auto &obj = std::get<JsonObj_t>(node->m_value);
        auto &it = stateStack.top().objIt;
        if (it != obj.cend()) {
          const auto child = &(it->second);
          ++it;
          stateStack.emplace(child);
        } else {
          obj.clear();
          stateStack.pop();
        }
      } break;
      default:
        stateStack.pop();
      }
    }
  }

  // Assignment operators

  JsonNode &operator=(const JsonNode &other) {
    m_value = other.m_value;
    return *this;
  }

  JsonNode &operator=(JsonNode &&other) {
    m_value = std::move(other.m_value);
    return *this;
  }

  template <typename T>
  typename std::enable_if_t<isJsonType<T>::value, JsonNode &>
  operator=(const T &value) {
    m_value = value;
    return *this;
  }

  template <typename T>
  typename std::enable_if_t<isJsonType<T>::value, JsonNode &>
  operator=(T &&value) {
    m_value = std::move(value);
    return *this;
  }

  template <typename T>
  typename std::enable_if_t<!(isJsonType<T>::value) && std::is_arithmetic_v<T>,
                            JsonNode &>
  operator=(T value) {
    m_value = static_cast<JsonNum_t>(value);
    return *this;
  }

  JsonNode &operator=(const char *str) {
    m_value = JsonStr_t(str);
    return *this;
  }

  // Index and key

  JsonNode &operator[](size_t idx) { return std::get<JsonArr_t>(m_value)[idx]; }
  JsonNode &at(size_t idx) { return std::get<JsonArr_t>(m_value).at(idx); }
  const JsonNode &operator[](size_t idx) const {
    return std::get<JsonArr_t>(m_value)[idx];
  }
  const JsonNode &at(size_t idx) const {
    return std::get<JsonArr_t>(m_value).at(idx);
  }

  JsonNode &operator[](const std::string &key) {
    return std::get<JsonObj_t>(m_value)[key];
  }
  JsonNode &at(const std::string &key) {
    return std::get<JsonObj_t>(m_value).at(key);
  }
  const JsonNode &operator[](const std::string &key) const {
    return std::get<JsonObj_t>(m_value).at(key);
  }
  const JsonNode &at(const std::string &key) const {
    return std::get<JsonObj_t>(m_value).at(key);
  }

  bool hasKey(const JsonStr_t &key) const {
    if (!isObj())
      return false;
    const auto &obj = std::get<JsonObj_t>(m_value);
    return obj.find(key) != obj.end();
  }

  JsonObj_t::iterator find(const JsonStr_t &key) {
    return std::get<JsonObj_t>(m_value).find(key);
  }

  JsonObj_t::const_iterator find(const JsonStr_t &key) const {
    return std::get<JsonObj_t>(m_value).find(key);
  }

  const size_t size() const {
    if (isStr()) {
      return std::get<JsonStr_t>(m_value).size();
    } else if (isArr()) {
      return std::get<JsonArr_t>(m_value).size();
    } else if (isObj()) {
      return std::get<JsonObj_t>(m_value).size();
    } else {
      return 0;
    }
  }

  // Type and getter

  JsonType type() const { return static_cast<JsonType>(m_value.index()); }
  std::string typeStr() const {
    const char *types[]{"empty", "null", "bool", "num", "str", "arr", "obj"};
    return types[m_value.index()];
  }

  bool isEmpty() const { return type() == JsonType::Empty; }
  bool isNull() const { return type() == JsonType::Null; }
  bool isBool() const { return type() == JsonType::Bool; }
  bool isNum() const { return type() == JsonType::Num; }
  bool isStr() const { return type() == JsonType::Str; }
  bool isArr() const { return type() == JsonType::Arr; }
  bool isObj() const { return type() == JsonType::Obj; }

  template <typename T>
  typename std::enable_if_t<isJsonType<T>::value, T &> get() {
    return std::get<T>(m_value);
  }

  template <typename T>
  typename std::enable_if_t<isJsonType<T>::value, const T> get() const {
    return std::get<T>(m_value);
  }

  template <typename T>
  typename std::enable_if_t<
      (!(isJsonType<T>::value) && std::is_arithmetic_v<T>), const T>
  get() const {
    return static_cast<T>(std::get<JsonNum_t>(m_value));
  }

  template <typename T>
  typename std::enable_if_t<std::is_same_v<T, const char *>, const char *>
  get() const {
    if (isStr())
      return std::get<JsonStr_t>(m_value).c_str();
    else
      return nullptr;
  }

private:
  template <typename ArrIter, typename ObjIter> class Iterator {
  public:
    Iterator(ArrIter iter) { m_iter.template emplace<0>(iter); }
    Iterator(ObjIter iter) { m_iter.template emplace<1>(iter); }

    Iterator &operator++() {
      if (m_iter.index() == 0)
        ++std::get<0>(m_iter);
      else
        ++std::get<1>(m_iter);
      return *this;
    }

    bool operator==(const Iterator &rhs) const { return m_iter == rhs.m_iter; }
    bool operator!=(const Iterator &rhs) const { return m_iter != rhs.m_iter; }

    auto &operator*() const {
      if (m_iter.index() == 0)
        return *std::get<0>(m_iter);
      else
        return std::get<1>(m_iter)->second;
    }
    auto *operator->() const {
      if (m_iter.index() == 0)
        return *std::get<0>(m_iter);
      else
        return std::get<1>(m_iter)->second;
    }

  private:
    std::variant<ArrIter, ObjIter> m_iter;
  };

public:
  using iterator = Iterator<JsonArr_t::iterator, JsonObj_t::iterator>;
  using const_iterator =
      Iterator<JsonArr_t::const_iterator, JsonObj_t::const_iterator>;
  using reverse_iterator =
      Iterator<JsonArr_t::reverse_iterator, JsonObj_t::reverse_iterator>;
  using const_reverse_iterator = Iterator<JsonArr_t::const_reverse_iterator,
                                          JsonObj_t::const_reverse_iterator>;

  iterator begin() {
    if (isArr()) {
      return iterator(std::get<JsonArr_t>(m_value).begin());
    } else
      return iterator(std::get<JsonObj_t>(m_value).begin());
  }
  iterator end() {
    if (isArr()) {
      return iterator(std::get<JsonArr_t>(m_value).end());
    } else
      return iterator(std::get<JsonObj_t>(m_value).end());
  }
  const_iterator begin() const {
    if (isArr()) {
      return const_iterator(std::get<JsonArr_t>(m_value).begin());
    } else
      return const_iterator(std::get<JsonObj_t>(m_value).begin());
  }
  const_iterator end() const {
    if (isArr()) {
      return const_iterator(std::get<JsonArr_t>(m_value).end());
    } else
      return const_iterator(std::get<JsonObj_t>(m_value).end());
  }
  const_iterator cbegin() const {
    if (isArr()) {
      return const_iterator(std::get<JsonArr_t>(m_value).cbegin());
    } else
      return const_iterator(std::get<JsonObj_t>(m_value).cbegin());
  }
  const_iterator cend() const {
    if (isArr()) {
      return const_iterator(std::get<JsonArr_t>(m_value).cend());
    } else
      return const_iterator(std::get<JsonObj_t>(m_value).cend());
  }
  reverse_iterator rbegin() {
    if (isArr()) {
      return reverse_iterator(std::get<JsonArr_t>(m_value).rbegin());
    } else
      return reverse_iterator(std::get<JsonObj_t>(m_value).rbegin());
  }
  reverse_iterator rend() {
    if (isArr()) {
      return reverse_iterator(std::get<JsonArr_t>(m_value).rend());
    } else
      return reverse_iterator(std::get<JsonObj_t>(m_value).rend());
  }
  const_reverse_iterator rbegin() const {
    if (isArr()) {
      return const_reverse_iterator(std::get<JsonArr_t>(m_value).rbegin());
    } else
      return const_reverse_iterator(std::get<JsonObj_t>(m_value).rbegin());
  }
  const_reverse_iterator rend() const {
    if (isArr()) {
      return const_reverse_iterator(std::get<JsonArr_t>(m_value).rend());
    } else
      return const_reverse_iterator(std::get<JsonObj_t>(m_value).rend());
  }

public:
  std::string toString(size_t indent = -1, bool ascii = true) const {

    bool formatted = (indent != -1);

    std::stack<ConstTraverseState> stateStack;
    stateStack.emplace(this);

    std::stringstream ss;

    while (!stateStack.empty()) {
      auto node = stateStack.top().node;
      switch (node->type()) {
      case JsonType::Null:
        ss << "null";
        stateStack.pop();
        break;
      case JsonType::Bool:
        ss << (node->get<bool>() ? "true" : "false");
        stateStack.pop();
        break;
      case JsonType::Num:
        ss << node->get<JsonNum_t>();
        stateStack.pop();
        break;
      case JsonType::Str:
        ss << "\"" << toJsonString(std::get<JsonStr_t>(node->m_value), ascii)
           << "\"";
        stateStack.pop();
        break;
      case JsonType::Arr: {
        const auto &arr = std::get<JsonArr_t>(node->m_value);
        auto &it = stateStack.top().arrIt;
        if (it == arr.cbegin()) {
          if (formatted)
            ss << "[\n" << std::string(indent * stateStack.size(), ' ');
          else
            ss << '[';
        } else if (it != arr.cend()) {
          if (formatted)
            ss << ",\n" << std::string(indent * stateStack.size(), ' ');
          else
            ss << ',';
        }
        if (it != arr.cend()) {
          const auto child = &(*it);
          ++it;
          stateStack.emplace(child);
        } else {
          if (formatted)
            ss << '\n'
               << std::string(indent * (stateStack.size() - 1), ' ') << ']';
          else
            ss << ']';
          stateStack.pop();
        }
      } break;
      case JsonType::Obj: {
        const auto &obj = std::get<JsonObj_t>(node->m_value);
        auto &it = stateStack.top().objIt;
        if (it == obj.cbegin()) {
          if (formatted)
            ss << "{\n" << std::string(indent * stateStack.size(), ' ');
          else
            ss << '{';
        } else if (it != obj.cend()) {
          if (formatted)
            ss << ",\n" << std::string(indent * stateStack.size(), ' ');
          else
            ss << ',';
        }

        if (it != obj.cend()) {
          ss << "\"" << toJsonString(it->first, ascii) << "\":";
          const auto child = &(it->second);
          ++it;
          stateStack.emplace(child);
        } else {
          if (formatted)
            ss << '\n'
               << std::string(indent * (stateStack.size() - 1), ' ') << '}';
          else
            ss << '}';
          stateStack.pop();
        }
      } break;
      default:
        stateStack.pop();
        break;
      }
    }
    return ss.str();
  }

private:
  static std::string toJsonString(const JsonStr_t &src, bool ascii) {
    std::stringstream ss;
    for (auto ite = src.begin(); ite != src.end(); ++ite) {
      switch (*ite) {
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
      default: {
        if (ascii && *ite & 0x80)
          ss << decodeUtf8(ite, src);
        else
          ss << *ite;
      } break;
      }
    }
    return ss.str();
  }

  static std::string decodeUtf8(std::string::const_iterator &ite,
                                const JsonStr_t &src) {
    uint32_t u = 0;
    if (!(*ite & 0x20)) {
      u = ((*ite & 0x1F) << 6) | (*(ite + 1) & 0x3F);
      ++ite;
    } else if (!(*ite & 0x10)) {
      u = ((*ite & 0xF) << 12) | ((*(ite + 1) & 0x3F) << 6) |
          (*(ite + 2) & 0x3F);
      ite += 2;
    } else {
      u = ((*ite & 0x7) << 18) | ((*(ite + 1) & 0x3F) << 12) |
          ((*(ite + 2) & 0x3F) << 6) | (*(ite + 3) & 0x3F);
      ite += 3;
    }

    if (u >= 0x10000) {
      uint64_t l = 0xDC00 + (u & 0x3FF);
      uint64_t u0 = ((u - 0x10000) >> 10) + 0xD800;
      if (u0 <= 0xDBFF) {
        return toHex4(u0) + toHex4(l);
      }
    }
    return toHex4(u);
  }

  static std::string toHex4(uint32_t u) {
    char h[4];
    for (int i = 3; i >= 0; --i) {
      h[i] = u & 0xF;
      h[i] = h[i] < 10 ? (h[i] + '0') : (h[i] - 10 + 'a');
      u >>= 4;
    }
    return "\\u" + std::string(h, 4);
  }

private:
  std::variant<std::monostate, JsonNull_t, bool, JsonNum_t, JsonStr_t,
               JsonArr_t, JsonObj_t>
      m_value;
};

class JsonParser {
public:
  JsonParser() = default;
  JsonParser(const JsonParser &) = delete;
  JsonParser &operator=(const JsonParser &) = delete;

  JsonNode parse(const std::string &buffer);

private:
  void skipSpace();

  void parseLiteral(JsonNode *);

  JsonStr_t parseString();
  void parseEscape(JsonStr_t &);
  void parseUtf8(JsonStr_t &);
  uint32_t parseHex4();
  void encodeUtf8(JsonStr_t &, uint32_t);

  void beginArray();
  void beginObject();
  JsonStr_t parseKeyWithColon();
  void handleComma();

  void parseNumber(JsonNode *);

  char ch() const { return m_input[m_pos]; }
  void next() { ++m_pos; }
  bool eoi() const { return m_pos == m_end; }
  void popStack() {
    if (!m_nodeStack.empty())
      m_nodeStack.pop();
    else
      throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidJson));
  }

private:
  std::string m_input;
  size_t m_pos;
  size_t m_end;
  std::stack<JsonNode *> m_nodeStack;
};

inline JsonNode JsonParser::parse(const std::string &input) {
  m_input = input;
  m_pos = 0;
  m_end = m_input.size();
  m_nodeStack = {};

  JsonNode ret;

  m_nodeStack.push(&ret);

  while (!m_nodeStack.empty()) {
    skipSpace();
    if (eoi()) {
      throw std::runtime_error(
          getJsonErrorMsg(JsonErrorCode::UnexpectedEndOfInput));
      break;
    }

    auto *node = m_nodeStack.top();

    switch (ch()) {
    case 'n':
    case 't':
    case 'f':
      parseLiteral(node);
      popStack();
      break;
    case '"':
      node->m_value.emplace<JsonStr_t>(parseString());
      popStack();
      break;
    case '[':
      beginArray();
      break;
    case ']':
      next(); // ']'
      if (!node->isArr())
        throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidJson));
      popStack();
      break;
    case '{':
      beginObject();
      break;
    case '}':
      next(); // '}'
      if (!node->isObj())
        throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidJson));
      popStack();
      break;
    case ',':
      handleComma();
      break;
    default:
      if (std::isdigit(ch()) || ch() == '-') {
        parseNumber(node);
        popStack();
      } else {
        throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidJson));
      }
    }
  }

  skipSpace();
  if (!eoi()) {
    throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidJson));
  }

  return ret;
}

inline void JsonParser::skipSpace() {
  while (!eoi() && std::isspace(ch())) {
    next();
  }
}

inline void JsonParser::parseLiteral(JsonNode *node) {
  size_t rem = m_end - m_pos;
  if (rem >= 4 && ch() == 'n' && m_input[m_pos + 1] == 'u' &&
      m_input[m_pos + 2] == 'l' && m_input[m_pos + 3] == 'l') {
    m_pos += 4;
    node->m_value.emplace<JsonNull_t>(JsonNull());
  } else if (rem >= 4 && ch() == 't' && m_input[m_pos + 1] == 'r' &&
             m_input[m_pos + 2] == 'u' && m_input[m_pos + 3] == 'e') {
    m_pos += 4;
    node->m_value.emplace<bool>(true);
  } else if (rem >= 5 && ch() == 'f' && m_input[m_pos + 1] == 'a' &&
             m_input[m_pos + 2] == 'l' && m_input[m_pos + 3] == 's' &&
             m_input[m_pos + 4] == 'e') {
    m_pos += 5;
    node->m_value.emplace<bool>(false);
  } else
    throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidLiteral));
}

inline JsonStr_t JsonParser::parseString() {
  next(); // '"'
  JsonStr_t ret;

  while (!eoi() && ch() != '"') {
    switch (ch()) {
    case '\\':
      parseEscape(ret);
      break;
    default:
      if (static_cast<uint8_t>(ch()) < 0x20u) {
        throw std::runtime_error(
            getJsonErrorMsg(JsonErrorCode::InvalidCharacter));
      }
      parseUtf8(ret);
    }
  }

  next(); // '"'
  return ret;
}

inline void JsonParser::parseEscape(JsonStr_t &ret) {
  next(); // '\\'
  if (eoi()) {
    throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidString));
  }
  switch (ch()) {
  case '"':
  case '\\':
  case '/':
    ret.push_back(ch());
    next();
    break;
  case 'b':
    ret.push_back('\b');
    next();
    break;
  case 'f':
    ret.push_back('\f');
    next();
    break;
  case 'n':
    ret.push_back('\n');
    next();
    break;
  case 'r':
    ret.push_back('\r');
    next();
    break;
  case 't':
    ret.push_back('\t');
    next();
    break;
  case 'u': {
    next(); // 'u'
    uint32_t u = parseHex4();
    if (u >= 0xD800 && u <= 0xDBFF) {
      if (eoi() || ch() != '\\') {
        throw std::runtime_error(
            getJsonErrorMsg(JsonErrorCode::InvalidUnicode));
      }
      next(); // '\\'
      if (eoi() || ch() != 'u') {
        throw std::runtime_error(
            getJsonErrorMsg(JsonErrorCode::InvalidUnicode));
      }
      next(); // 'u'
      uint32_t u2 = parseHex4();
      if (u2 < 0xDC00 || u2 > 0xDFFF) {
        throw std::runtime_error(
            getJsonErrorMsg(JsonErrorCode::InvalidUnicode));
      }
      u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
    }
    encodeUtf8(ret, u);
  } break;
  default:
    throw std::runtime_error(
        getJsonErrorMsg(JsonErrorCode::InvalidEscapeSequence));
  }
}

inline uint32_t JsonParser::parseHex4() {
  uint64_t u = 0;
  for (int i = 0; i < 4; ++i) {
    if (eoi()) {
      throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidString));
    }
    char c = ch();
    next();
    u <<= 4;
    if (c >= '0' && c <= '9') {
      u |= c - '0';
    } else if (c >= 'a' && c <= 'f') {
      u |= c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      u |= c - 'A' + 10;
    } else {
      throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidUnicode));
    }
  }
  return u;
}

inline void JsonParser::parseUtf8(JsonStr_t &ret) {
  uint8_t u = static_cast<uint8_t>(ch()), byteCount = 0;
  if ((u <= 0x7F))
    byteCount = 1;
  else if ((u >= 0xC0) && (u <= 0xDF))
    byteCount = 2;
  else if ((u >= 0xE0) && (u <= 0xEF))
    byteCount = 3;
  else if ((u >= 0xF0) && (u <= 0xF7))
    byteCount = 4;
  else
    throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidCharacter));

  ret.push_back(ch());
  next();
  for (auto p = m_pos; !eoi() && m_pos + 1 < p + byteCount; next()) {
    u = static_cast<uint8_t>(ch());
    if ((u >= 0x80) && (u <= 0xBF))
      ret.push_back(ch());
    else
      throw std::runtime_error(
          getJsonErrorMsg(JsonErrorCode::InvalidCharacter));
  }
}

inline void JsonParser::encodeUtf8(JsonStr_t &ret, uint32_t u) {
  if (u <= 0x7F)
    ret.push_back(u & 0xFF);
  else if (u <= 0x7FF) {
    ret.push_back(0xC0 | ((u >> 6) & 0xFF));
    ret.push_back(0x80 | (u & 0x3F));
  } else if (u <= 0xFFFF) {
    ret.push_back(0xE0 | ((u >> 12) & 0xFF));
    ret.push_back(0x80 | ((u >> 6) & 0x3F));
    ret.push_back(0x80 | (u & 0x3F));
  } else {
    ret.push_back(0xF0 | ((u >> 18) & 0xFF));
    ret.push_back(0x80 | ((u >> 12) & 0x3F));
    ret.push_back(0x80 | ((u >> 6) & 0x3F));
    ret.push_back(0x80 | (u & 0x3F));
  }
}

inline JsonStr_t JsonParser::parseKeyWithColon() {
  auto key = parseString();
  skipSpace();
  if (eoi() || ch() != ':') {
    throw std::runtime_error(
        getJsonErrorMsg(JsonErrorCode::InvalidKeyValuePair));
  }
  next(); // ':'
  return key;
}

inline void JsonParser::beginArray() {
  auto *node = m_nodeStack.top();
  next(); // '['
  skipSpace();
  if (eoi()) {
    throw std::runtime_error(
        getJsonErrorMsg(JsonErrorCode::UnexpectedEndOfInput));
  }
  node->m_value.emplace<JsonArr_t>();
  if (ch() == ']') {
    next(); // ']'
    popStack();
  } else {
    m_nodeStack.push(&(std::get<JsonArr_t>(node->m_value).emplace_back()));
  }
}

inline void JsonParser::beginObject() {
  auto *node = m_nodeStack.top();
  next(); // '{'
  skipSpace();
  if (eoi()) {
    throw std::runtime_error(
        getJsonErrorMsg(JsonErrorCode::UnexpectedEndOfInput));
  }
  node->m_value.emplace<JsonObj_t>();
  if (ch() == '}') {
    next(); // '}'
    popStack();
  } else {
    if (ch() != '"') {
      throw std::runtime_error(
          getJsonErrorMsg(JsonErrorCode::InvalidKeyValuePair));
    }
    auto key = parseKeyWithColon();
    m_nodeStack.push(&(std::get<JsonObj_t>(node->m_value)
                           .emplace(std::move(key), JsonNode{})
                           .first->second));
  }
}

inline void JsonParser::handleComma() {
  auto *node = m_nodeStack.top();
  next(); // ','
  if (node->isArr()) {
    auto &arr = std::get<JsonArr_t>(node->m_value);
    m_nodeStack.push(&arr.emplace_back());
  } else if (node->isObj()) {
    skipSpace();
    auto &obj = std::get<JsonObj_t>(node->m_value);
    auto key = parseKeyWithColon();
    m_nodeStack.push(&(obj.emplace(std::move(key), JsonNode{}).first->second));
  } else {
    throw std::runtime_error(
        getJsonErrorMsg(JsonErrorCode::InvalidArrayOrObject));
  }
}

inline void JsonParser::parseNumber(JsonNode *node) {
  auto p = m_input.begin() + m_pos;
  p += (*p == '-');
  if (p == m_input.end()) {
    throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidNumber));
  }
  if (*p == '0') {
    ++p;
  } else {
    if (!std::isdigit(*p)) {
      throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidNumber));
    }
    for (; p != m_input.end() && std::isdigit(*p); ++p)
      ;
  }
  if (p != m_input.end() && *p == '.') {
    ++p;
    if (p == m_input.end() || !std::isdigit(*p)) {
      throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidNumber));
    }
    for (; p != m_input.end() && std::isdigit(*p); ++p)
      ;
  }
  if (p != m_input.end() && (*p == 'e' || *p == 'E')) {
    ++p;
    if (p == m_input.end()) {
      throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidNumber));
    }
    if (*p == '+' || *p == '-')
      ++p;
    if (p == m_input.end()) {
      throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidNumber));
    }
    for (; p != m_input.end() && std::isdigit(*p); ++p)
      ;
  }

  JsonNum_t ret;
  size_t idx = 0;
  const std::string &numStr =
      m_input.substr(m_pos, p - m_input.begin() - m_pos);
  try {
    ret = strToJsonNum_t<JsonNum_t>(numStr, &idx);
  } catch (const std::invalid_argument &) {
    throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidNumber));
  } catch (const std::out_of_range &) {
    throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidNumber));
  }
  m_pos += idx;
  node->m_value.emplace<JsonNum_t>(ret);
}

#endif
