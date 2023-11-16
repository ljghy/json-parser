#ifndef JSON_PARSER_HPP_
#define JSON_PARSER_HPP_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <istream>
#include <map>
#include <memory>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
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
        arrIt = std::get<JsonArr_t *>(node->m_value)->begin();
      else if (node->isObj())
        objIt = std::get<JsonObj_t *>(node->m_value)->begin();
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
        arrIt = std::get<JsonArr_t *>(node->m_value)->cbegin();
      else if (node->isObj())
        objIt = std::get<JsonObj_t *>(node->m_value)->cbegin();
    }
  };

private:
  template <typename T> struct isJsonType {
    static constexpr bool value =
        std::is_same_v<T, JsonNull_t> || std::is_same_v<T, bool> ||
        std::is_same_v<T, JsonNum_t> || std::is_same_v<T, JsonStr_t> ||
        std::is_same_v<T, JsonArr_t> || std::is_same_v<T, JsonObj_t>;
  };

  template <typename T> struct isJsonValType {
    static constexpr bool value = std::is_same_v<T, JsonNull_t> ||
                                  std::is_same_v<T, bool> ||
                                  std::is_same_v<T, JsonNum_t>;
  };

  template <typename T> struct isJsonPtrType {
    static constexpr bool value = std::is_same_v<T, JsonStr_t> ||
                                  std::is_same_v<T, JsonArr_t> ||
                                  std::is_same_v<T, JsonObj_t>;
  };

public:
  // Constructors

  JsonNode() = default;

  template <typename T, typename = std::enable_if_t<isJsonValType<T>::value>>
  JsonNode(const T &value) : m_value(value) {}

  template <typename T, typename = std::enable_if_t<isJsonValType<T>::value>>
  JsonNode(T &&value) : m_value(value) {}

  JsonNode(const JsonArr_t &arr) : m_value(new JsonArr_t(arr)) {}
  JsonNode(JsonArr_t &&arr) : m_value(new JsonArr_t(std::move(arr))) {}

  JsonNode(const JsonObj_t &obj) : m_value(new JsonObj_t(obj)) {}
  JsonNode(JsonObj_t &&obj) : m_value(new JsonObj_t(std::move(obj))) {}

  template <typename T, typename = std::enable_if_t<!isJsonType<T>::value &&
                                                    std::is_arithmetic_v<T>>>
  JsonNode(T num) : m_value(static_cast<JsonNum_t>(num)) {}

  JsonNode(const char *str) : m_value(new JsonStr_t(str)) {}

  JsonNode(const std::initializer_list<JsonNode> &arr)
      : m_value(new JsonArr_t(arr)) {}
  JsonNode(
      const std::initializer_list<std::pair<const JsonKeyLiteral_t, JsonNode>>
          &obj) {
    auto o = m_value.emplace<JsonObj_t *>(new JsonObj_t{});
    for (const auto &p : obj) {
      o->emplace(p.first.key, p.second);
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
        auto &otherArr = *std::get<JsonArr_t *>(otherNode->m_value);
        auto &it = stateStack.top().arrIt;
        if (it == otherArr.cbegin()) {
          node->m_value.emplace<JsonArr_t *>(new JsonArr_t{})
              ->reserve(otherArr.size());
        }
        if (it != otherArr.cend()) {
          const auto otherChild = &(*it);
          ++it;
          stateStack.emplace(otherChild);
          nodeStack.emplace(
              &(std::get<JsonArr_t *>(node->m_value)->emplace_back()));
        } else {
          stateStack.pop();
          nodeStack.pop();
        }
      } break;
      case JsonType::Obj: {
        auto &otherObj = *std::get<JsonObj_t *>(otherNode->m_value);
        auto &it = stateStack.top().objIt;
        if (it == otherObj.cbegin()) {
          node->m_value.emplace<JsonObj_t *>(new JsonObj_t{});
        }
        if (it != otherObj.cend()) {
          const auto otherChild = &(it->second);
          stateStack.emplace(otherChild);
          nodeStack.emplace(&(std::get<JsonObj_t *>(node->m_value)
                                  ->emplace(it->first, JsonNode{})
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
        nodeStack.top()->m_value.emplace<JsonStr_t *>(
            new JsonStr_t(*std::get<JsonStr_t *>(otherNode->m_value)));
        stateStack.pop();
        nodeStack.pop();
        break;
      default:
        stateStack.pop();
        nodeStack.pop();
      }
    }
  }

  JsonNode(JsonNode &&other) noexcept {
    switch (other.type()) {
    case JsonType::Arr:
      m_value = std::get<JsonArr_t *>(other.m_value);
      break;
    case JsonType::Obj:
      m_value = std::get<JsonObj_t *>(other.m_value);
      break;
    case JsonType::Str:
      m_value = std::get<JsonStr_t *>(other.m_value);
      break;
    default:
      m_value = other.m_value;
    }
    other.m_value = std::monostate{};
  }

  // Destructor

  ~JsonNode() {
    if (isEmpty())
      return;

    std::stack<TraverseState> stateStack;
    stateStack.emplace(this);
    while (!stateStack.empty()) {
      auto node = stateStack.top().node;
      switch (node->type()) {
      case JsonType::Arr: {
        auto &arr = std::get<JsonArr_t *>(node->m_value);
        auto &it = stateStack.top().arrIt;
        if (it != arr->cend()) {
          const auto child = &(*it);
          ++it;
          stateStack.emplace(child);
        } else {
          delete arr;
          node->m_value = std::monostate{};
          stateStack.pop();
        }
      } break;
      case JsonType::Obj: {
        auto &obj = std::get<JsonObj_t *>(node->m_value);
        auto &it = stateStack.top().objIt;
        if (it != obj->cend()) {
          const auto child = &(it->second);
          ++it;
          stateStack.emplace(child);
        } else {
          delete obj;
          node->m_value = std::monostate{};
          stateStack.pop();
        }
      } break;
      case JsonType::Str:
        delete std::get<JsonStr_t *>(node->m_value);
        node->m_value = std::monostate{};
        stateStack.pop();
        break;
      default:
        node->m_value = std::monostate{};
        stateStack.pop();
      }
    }
  }

  void clear() {
    switch (type()) {
    case JsonType::Arr:
      delete std::get<JsonArr_t *>(m_value);
      break;
    case JsonType::Obj:
      delete std::get<JsonObj_t *>(m_value);
      break;
    case JsonType::Str:
      delete std::get<JsonStr_t *>(m_value);
      break;
    default:
      break;
    }
    m_value = std::monostate{};
  }

  // Assignment operators
  JsonNode &operator=(const JsonNode &other) {
    if (this == &other)
      return *this;

    switch (other.type()) {
    case JsonType::Arr: {
      const auto &otherArr = *std::get<JsonArr_t *>(other.m_value);
      if (isArr())
        *std::get<JsonArr_t *>(m_value) = otherArr;
      else {
        clear();
        m_value = new JsonArr_t(otherArr);
      }
    } break;
    case JsonType::Obj: {
      const auto &otherObj = *std::get<JsonObj_t *>(other.m_value);
      if (isObj())
        *std::get<JsonObj_t *>(m_value) = otherObj;
      else {
        clear();
        m_value = new JsonObj_t(otherObj);
      }
    } break;
    case JsonType::Str: {
      const auto &otherStr = *std::get<JsonStr_t *>(other.m_value);
      if (isStr())
        *std::get<JsonStr_t *>(m_value) = otherStr;
      else {
        clear();
        m_value = new JsonStr_t(otherStr);
      }
    } break;
    default:
      clear();
      m_value = other.m_value;
    }
    return *this;
  }

  JsonNode &operator=(JsonNode &&other) noexcept {
    if (this == &other)
      return *this;

    switch (other.type()) {
    case JsonType::Arr:
      m_value = std::get<JsonArr_t *>(other.m_value);
      break;
    case JsonType::Obj:
      m_value = std::get<JsonObj_t *>(other.m_value);
      break;
    case JsonType::Str:
      m_value = std::get<JsonStr_t *>(other.m_value);
      break;
    default:
      m_value = other.m_value;
    }
    other.m_value = std::monostate{};
    return *this;
  }

  template <typename T>
  typename std::enable_if_t<isJsonValType<T>::value, JsonNode &>
  operator=(const T &value) {
    clear();
    m_value = value;
    return *this;
  }

  template <typename T>
  typename std::enable_if_t<isJsonValType<T>::value, JsonNode &>
  operator=(T &&value) {
    clear();
    m_value = std::move(value);
    return *this;
  }

  template <typename T>
  typename std::enable_if_t<!(isJsonType<T>::value) && std::is_arithmetic_v<T>,
                            JsonNode &>
  operator=(T value) {
    clear();
    m_value = static_cast<JsonNum_t>(value);
    return *this;
  }

  JsonNode &operator=(const char *str) {
    if (str == nullptr)
      return *this;
    if (isStr())
      *std::get<JsonStr_t *>(m_value) = str;
    else {
      clear();
      m_value = new JsonStr_t(str);
    }
    return *this;
  }

  // Index and key

  JsonNode &operator[](size_t idx) {
    return (*std::get<JsonArr_t *>(m_value))[idx];
  }
  JsonNode &at(size_t idx) { return std::get<JsonArr_t *>(m_value)->at(idx); }
  const JsonNode &operator[](size_t idx) const {
    return (*std::get<JsonArr_t *>(m_value))[idx];
  }
  const JsonNode &at(size_t idx) const {
    return std::get<JsonArr_t *>(m_value)->at(idx);
  }

  JsonNode &operator[](const std::string &key) {
    return (*std::get<JsonObj_t *>(m_value))[key];
  }
  JsonNode &at(const std::string &key) {
    return std::get<JsonObj_t *>(m_value)->at(key);
  }
  const JsonNode &operator[](const std::string &key) const {
    return std::get<JsonObj_t *>(m_value)->at(key);
  }
  const JsonNode &at(const std::string &key) const {
    return std::get<JsonObj_t *>(m_value)->at(key);
  }

  bool hasKey(const JsonStr_t &key) const {
    if (!isObj())
      return false;
    const auto &obj = *std::get<JsonObj_t *>(m_value);
    return obj.find(key) != obj.end();
  }

  JsonObj_t::iterator find(const JsonStr_t &key) {
    return std::get<JsonObj_t *>(m_value)->find(key);
  }

  JsonObj_t::const_iterator find(const JsonStr_t &key) const {
    return std::get<JsonObj_t *>(m_value)->find(key);
  }

  size_t size() const {
    if (isStr()) {
      return std::get<JsonStr_t *>(m_value)->size();
    } else if (isArr()) {
      return std::get<JsonArr_t *>(m_value)->size();
    } else if (isObj()) {
      return std::get<JsonObj_t *>(m_value)->size();
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
  typename std::enable_if_t<isJsonValType<T>::value, T &> get() {
    return std::get<T>(m_value);
  }

  template <typename T>
  typename std::enable_if_t<isJsonPtrType<T>::value, T &> get() {
    return *std::get<T *>(m_value);
  }

  template <typename T>
  typename std::enable_if_t<isJsonValType<T>::value, const T> get() const {
    return std::get<T>(m_value);
  }

  template <typename T>
  typename std::enable_if_t<isJsonPtrType<T>::value, const T &> get() const {
    return *std::get<T *>(m_value);
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
      return std::get<JsonStr_t *>(m_value)->c_str();
    else
      return nullptr;
  }

private:
  template <typename ArrIter, typename ObjIter> class IteratorGen {
  public:
    IteratorGen(ArrIter iter) { m_iter.template emplace<0>(iter); }
    IteratorGen(ObjIter iter) { m_iter.template emplace<1>(iter); }

    IteratorGen &operator++() {
      if (m_iter.index() == 0)
        ++std::get<0>(m_iter);
      else
        ++std::get<1>(m_iter);
      return *this;
    }

    bool operator==(const IteratorGen &rhs) const {
      return m_iter == rhs.m_iter;
    }
    bool operator==(const ArrIter &rhs) const {
      return m_iter.index() == 0 && std::get<0>(m_iter) == rhs;
    }
    bool operator==(const ObjIter &rhs) const {
      return m_iter.index() == 1 && std::get<1>(m_iter) == rhs;
    }
    friend bool operator==(const ArrIter &lhs, const IteratorGen &rhs) {
      return rhs == lhs;
    }
    friend bool operator==(const ObjIter &lhs, const IteratorGen &rhs) {
      return rhs == lhs;
    }

    bool operator!=(const IteratorGen &rhs) const {
      return m_iter != rhs.m_iter;
    }
    bool operator!=(const ArrIter &rhs) const {
      return m_iter.index() != 0 || std::get<0>(m_iter) != rhs;
    }
    bool operator!=(const ObjIter &rhs) const {
      return m_iter.index() != 1 || std::get<1>(m_iter) != rhs;
    }
    friend bool operator!=(const ArrIter &lhs, const IteratorGen &rhs) {
      return rhs != lhs;
    }
    friend bool operator!=(const ObjIter &lhs, const IteratorGen &rhs) {
      return rhs != lhs;
    }

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
  using Iterator = IteratorGen<JsonArr_t::iterator, JsonObj_t::iterator>;
  using ConstIterator =
      IteratorGen<JsonArr_t::const_iterator, JsonObj_t::const_iterator>;
  using ReverseIterator =
      IteratorGen<JsonArr_t::reverse_iterator, JsonObj_t::reverse_iterator>;
  using ConstReverseIterator = IteratorGen<JsonArr_t::const_reverse_iterator,
                                           JsonObj_t::const_reverse_iterator>;

  Iterator begin() {
    if (isArr()) {
      return Iterator(std::get<JsonArr_t *>(m_value)->begin());
    } else
      return Iterator(std::get<JsonObj_t *>(m_value)->begin());
  }
  Iterator end() {
    if (isArr()) {
      return Iterator(std::get<JsonArr_t *>(m_value)->end());
    } else
      return Iterator(std::get<JsonObj_t *>(m_value)->end());
  }
  ConstIterator begin() const {
    if (isArr()) {
      return ConstIterator(std::get<JsonArr_t *>(m_value)->begin());
    } else
      return ConstIterator(std::get<JsonObj_t *>(m_value)->begin());
  }
  ConstIterator end() const {
    if (isArr()) {
      return ConstIterator(std::get<JsonArr_t *>(m_value)->end());
    } else
      return ConstIterator(std::get<JsonObj_t *>(m_value)->end());
  }
  ConstIterator cbegin() const {
    if (isArr()) {
      return ConstIterator(std::get<JsonArr_t *>(m_value)->cbegin());
    } else
      return ConstIterator(std::get<JsonObj_t *>(m_value)->cbegin());
  }
  ConstIterator cend() const {
    if (isArr()) {
      return ConstIterator(std::get<JsonArr_t *>(m_value)->cend());
    } else
      return ConstIterator(std::get<JsonObj_t *>(m_value)->cend());
  }
  ReverseIterator rbegin() {
    if (isArr()) {
      return ReverseIterator(std::get<JsonArr_t *>(m_value)->rbegin());
    } else
      return ReverseIterator(std::get<JsonObj_t *>(m_value)->rbegin());
  }
  ReverseIterator rend() {
    if (isArr()) {
      return ReverseIterator(std::get<JsonArr_t *>(m_value)->rend());
    } else
      return ReverseIterator(std::get<JsonObj_t *>(m_value)->rend());
  }
  ConstReverseIterator rbegin() const {
    if (isArr()) {
      return ConstReverseIterator(std::get<JsonArr_t *>(m_value)->crbegin());
    } else
      return ConstReverseIterator(std::get<JsonObj_t *>(m_value)->crbegin());
  }
  ConstReverseIterator rend() const {
    if (isArr()) {
      return ConstReverseIterator(std::get<JsonArr_t *>(m_value)->crend());
    } else
      return ConstReverseIterator(std::get<JsonObj_t *>(m_value)->crend());
  }

public:
  std::string toString(size_t indent = -1, bool ascii = true) const {

    bool formatted = (indent != static_cast<size_t>(-1));

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
        ss << "\"" << toJsonString(*std::get<JsonStr_t *>(node->m_value), ascii)
           << "\"";
        stateStack.pop();
        break;
      case JsonType::Arr: {
        const auto &arr = *std::get<JsonArr_t *>(node->m_value);
        if (arr.empty()) {
          ss << "[]";
          stateStack.pop();
          break;
        }
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
        const auto &obj = *std::get<JsonObj_t *>(node->m_value);
        if (obj.empty()) {
          ss << "{}";
          stateStack.pop();
          break;
        }
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
          ss << decodeUtf8(ite);
        else
          ss << *ite;
      } break;
      }
    }
    return ss.str();
  }

  static std::string decodeUtf8(std::string::const_iterator &ite) {
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
  std::variant<std::monostate, JsonNull_t, bool, JsonNum_t, JsonStr_t *,
               JsonArr_t *, JsonObj_t *>
      m_value;
};

class JsonParser {
public:
  JsonParser() = default;
  JsonParser(const JsonParser &) = delete;
  JsonParser &operator=(const JsonParser &) = delete;

  JsonNode parse(const std::string_view &);
  JsonNode parse(std::istream &);

private:
  template <typename Derived> class JsonInputStreamBase {
  public:
    char ch() const { return (static_cast<Derived *>(this))->ch(); } // peek
    void next() { (static_cast<Derived *>(this))->next(); }
    char get() { return (static_cast<Derived *>(this))->get(); }
    bool eoi() const {
      return (static_cast<Derived *>(this))->eoi();
    } // end of input
  };

  class JsonFileInputStream : public JsonInputStreamBase<JsonFileInputStream> {
  public:
    JsonFileInputStream(std::istream &is) : m_is(is) {}
    char ch() const { return m_is.peek(); }
    void next() { m_is.get(); }
    char get() { return m_is.get(); }
    bool eoi() const { return m_is.eof(); }

  private:
    std::istream &m_is;
  };

  class JsonStringViewInputStream
      : public JsonInputStreamBase<JsonStringViewInputStream> {
  public:
    JsonStringViewInputStream(const std::string_view &input)
        : m_input(input), m_pos(input.cbegin()) {}
    char ch() const { return *m_pos; }
    void next() { ++m_pos; }
    char get() { return *m_pos++; }
    bool eoi() const { return m_pos == m_input.cend(); }

  private:
    std::string_view m_input;
    std::string_view::const_iterator m_pos;
  };

  template <typename T>
  using IsJsonInputStream = std::is_base_of<JsonInputStreamBase<T>, T>;

private:
  template <typename DerivedInputStream>
  typename std::enable_if_t<IsJsonInputStream<DerivedInputStream>::value,
                            JsonNode>
  parse(DerivedInputStream &);

  template <typename DerivedInputStream> void skipSpace(DerivedInputStream &);

  template <typename DerivedInputStream>
  void parseLiteral(DerivedInputStream &, JsonNode *);

  template <typename DerivedInputStream>
  JsonStr_t parseString(DerivedInputStream &);

  template <typename DerivedInputStream>
  void parseEscape(DerivedInputStream &, JsonStr_t &);

  template <typename DerivedInputStream>
  void parseUtf8(DerivedInputStream &, JsonStr_t &);

  template <typename DerivedInputStream>
  uint32_t parseHex4(DerivedInputStream &);

  void encodeUtf8(JsonStr_t &, uint32_t);

  template <typename DerivedInputStream> void beginArray(DerivedInputStream &);

  template <typename DerivedInputStream> void beginObject(DerivedInputStream &);

  template <typename DerivedInputStream>
  JsonStr_t parseKeyWithColon(DerivedInputStream &);

  template <typename DerivedInputStream> void handleComma(DerivedInputStream &);

  template <typename DerivedInputStream>
  void parseNumber(DerivedInputStream &, JsonNode *);

  void popStack() {
    if (!m_nodeStack.empty())
      m_nodeStack.pop();
    else
      throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidJson));
  }

private:
  std::stack<JsonNode *> m_nodeStack;
};

template <typename DerivedInputStream>
inline typename std::enable_if_t<
    JsonParser::IsJsonInputStream<DerivedInputStream>::value, JsonNode>
JsonParser::parse(DerivedInputStream &is) {
  m_nodeStack = {};

  JsonNode ret;

  m_nodeStack.push(&ret);

  while (!m_nodeStack.empty()) {
    skipSpace(is);
    if (is.eoi())
      throw std::runtime_error(
          getJsonErrorMsg(JsonErrorCode::UnexpectedEndOfInput));

    auto *node = m_nodeStack.top();

    switch (is.ch()) {
    case 'n':
    case 't':
    case 'f':
      parseLiteral(is, node);
      popStack();
      break;
    case '"':
      is.next();
      node->m_value.emplace<JsonStr_t *>(new JsonStr_t(parseString(is)));
      popStack();
      break;
    case '[':
      is.next();
      beginArray(is);
      break;
    case ']':
      is.next();
      if (!node->isArr())
        throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidJson));
      popStack();
      break;
    case '{':
      is.next();
      beginObject(is);
      break;
    case '}':
      is.next();
      if (!node->isObj())
        throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidJson));
      popStack();
      break;
    case ',':
      is.next();
      handleComma(is);
      break;
    default:
      if (std::isdigit(is.ch()) || is.ch() == '-') {
        parseNumber(is, node);
        popStack();
      } else {
        throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidJson));
      }
    }
  }

  skipSpace(is);
  if (!is.eoi())
    throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidJson));

  return ret;
}

inline JsonNode JsonParser::parse(const std::string_view &input_view) {
  auto stringViewInputStream = JsonStringViewInputStream(input_view);
  return parse(stringViewInputStream);
}

inline JsonNode JsonParser::parse(std::istream &is) {
  auto fileInputStream = JsonFileInputStream(is);
  return parse(fileInputStream);
}

template <typename DerivedInputStream>
inline void JsonParser::skipSpace(DerivedInputStream &is) {
  while (!is.eoi() && std::isspace(is.ch())) {
    is.next();
  }
}

template <typename DerivedInputStream>
inline void JsonParser::parseLiteral(DerivedInputStream &is, JsonNode *node) {
  if (is.ch() == 'n') {
    is.next();
    if (!is.eoi() && is.get() == 'u' && !is.eoi() && is.get() == 'l' &&
        !is.eoi() && is.get() == 'l') {
      node->m_value.emplace<JsonNull_t>(JsonNull());
      return;
    }
  }
  if (is.ch() == 't') {
    is.next();
    if (!is.eoi() && is.get() == 'r' && !is.eoi() && is.get() == 'u' &&
        !is.eoi() && is.get() == 'e') {
      node->m_value.emplace<bool>(true);
      return;
    }
  }
  if (is.ch() == 'f') {
    is.next();
    if (!is.eoi() && is.get() == 'a' && !is.eoi() && is.get() == 'l' &&
        !is.eoi() && is.get() == 's' && !is.eoi() && is.get() == 'e') {
      node->m_value.emplace<bool>(false);
      return;
    }
  }
  throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidLiteral));
}

template <typename DerivedInputStream>
inline JsonStr_t JsonParser::parseString(DerivedInputStream &is) {
  JsonStr_t ret;

  while (!is.eoi() && is.ch() != '"') {
    switch (is.ch()) {
    case '\\':
      is.next();
      parseEscape(is, ret);
      break;
    default:
      if (static_cast<uint8_t>(is.ch()) < 0x20u)
        throw std::runtime_error(
            getJsonErrorMsg(JsonErrorCode::InvalidCharacter));

      parseUtf8(is, ret);
    }
  }

  if (is.eoi())
    throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidString));

  is.next();
  return ret;
}

template <typename DerivedInputStream>
inline void JsonParser::parseEscape(DerivedInputStream &is, JsonStr_t &ret) {
  if (is.eoi())
    throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidString));

  switch (is.ch()) {
  case '"':
  case '\\':
  case '/':
    ret.push_back(is.ch());
    is.next();
    break;
  case 'b':
    ret.push_back('\b');
    is.next();
    break;
  case 'f':
    ret.push_back('\f');
    is.next();
    break;
  case 'n':
    ret.push_back('\n');
    is.next();
    break;
  case 'r':
    ret.push_back('\r');
    is.next();
    break;
  case 't':
    ret.push_back('\t');
    is.next();
    break;
  case 'u': {
    is.next();
    uint32_t u = parseHex4(is);
    if (u >= 0xD800 && u <= 0xDBFF) {
      if (is.eoi() || is.get() != '\\')
        throw std::runtime_error(
            getJsonErrorMsg(JsonErrorCode::InvalidUnicode));

      if (is.eoi() || is.get() != 'u')
        throw std::runtime_error(
            getJsonErrorMsg(JsonErrorCode::InvalidUnicode));

      uint32_t u2 = parseHex4(is);
      if (u2 < 0xDC00 || u2 > 0xDFFF)
        throw std::runtime_error(
            getJsonErrorMsg(JsonErrorCode::InvalidUnicode));

      u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
    }
    encodeUtf8(ret, u);
  } break;
  default:
    throw std::runtime_error(
        getJsonErrorMsg(JsonErrorCode::InvalidEscapeSequence));
  }
}

template <typename DerivedInputStream>
inline uint32_t JsonParser::parseHex4(DerivedInputStream &is) {
  uint64_t u = 0;
  for (int i = 0; i < 4; ++i) {
    if (is.eoi())
      throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidString));

    char c = is.get();
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

template <typename DerivedInputStream>
inline void JsonParser::parseUtf8(DerivedInputStream &is, JsonStr_t &ret) {
  uint8_t u = static_cast<uint8_t>(is.ch()), byteCount = 0;
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

  ret.push_back(is.get());
  while (byteCount-- > 1) {
    u = static_cast<uint8_t>(is.ch());
    if ((u >= 0x80) && (u <= 0xBF))
      ret.push_back(is.get());
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

template <typename DerivedInputStream>
inline JsonStr_t JsonParser::parseKeyWithColon(DerivedInputStream &is) {
  auto key = parseString(is);
  skipSpace(is);
  if (is.eoi() || is.get() != ':')
    throw std::runtime_error(
        getJsonErrorMsg(JsonErrorCode::InvalidKeyValuePair));

  return key;
}

template <typename DerivedInputStream>
inline void JsonParser::beginArray(DerivedInputStream &is) {
  skipSpace(is);
  if (is.eoi())
    throw std::runtime_error(
        getJsonErrorMsg(JsonErrorCode::UnexpectedEndOfInput));

  auto *node = m_nodeStack.top();
  node->m_value.emplace<JsonArr_t *>(new JsonArr_t{});
  if (is.ch() == ']') {
    is.next();
    popStack();
  } else {
    m_nodeStack.push(&(std::get<JsonArr_t *>(node->m_value)->emplace_back()));
  }
}

template <typename DerivedInputStream>
inline void JsonParser::beginObject(DerivedInputStream &is) {
  skipSpace(is);
  if (is.eoi())
    throw std::runtime_error(
        getJsonErrorMsg(JsonErrorCode::UnexpectedEndOfInput));

  auto *node = m_nodeStack.top();
  node->m_value.emplace<JsonObj_t *>(new JsonObj_t{});
  if (is.ch() == '}') {
    is.next();
    popStack();
  } else {
    if (is.get() != '"')
      throw std::runtime_error(
          getJsonErrorMsg(JsonErrorCode::InvalidKeyValuePair));

    auto key = parseKeyWithColon(is);
    m_nodeStack.push(&(std::get<JsonObj_t *>(node->m_value)
                           ->emplace(std::move(key), JsonNode{})
                           .first->second));
  }
}

template <typename DerivedInputStream>
inline void JsonParser::handleComma(DerivedInputStream &is) {
  auto *node = m_nodeStack.top();
  if (node->isArr()) {
    auto &arr = *std::get<JsonArr_t *>(node->m_value);
    m_nodeStack.push(&arr.emplace_back());
  } else if (node->isObj()) {
    skipSpace(is);
    auto &obj = *std::get<JsonObj_t *>(node->m_value);
    if (is.eoi() || is.get() != '"')
      throw std::runtime_error(
          getJsonErrorMsg(JsonErrorCode::UnexpectedEndOfInput));
    auto key = parseKeyWithColon(is);
    m_nodeStack.push(&(obj.emplace(std::move(key), JsonNode{}).first->second));
  } else {
    throw std::runtime_error(
        getJsonErrorMsg(JsonErrorCode::InvalidArrayOrObject));
  }
}

template <typename DerivedInputStream>
inline void JsonParser::parseNumber(DerivedInputStream &is, JsonNode *node) {
  std::string numStr;

  if (is.ch() == '-')
    numStr.push_back(is.get());
  if (is.eoi())
    throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidNumber));
  if (is.ch() == '0') {
    numStr.push_back(is.get());
  } else {
    if (!std::isdigit(is.ch()))
      throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidNumber));
    while (!is.eoi() && std::isdigit(is.ch()))
      numStr.push_back(is.get());
  }
  if (!is.eoi() && is.ch() == '.') {
    numStr.push_back(is.get());
    if (is.eoi() || !std::isdigit(is.ch()))
      throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidNumber));
    while (!is.eoi() && std::isdigit(is.ch()))
      numStr.push_back(is.get());
  }
  if (!is.eoi() && (is.ch() == 'e' || is.ch() == 'E')) {
    numStr.push_back(is.get());
    if (is.eoi())
      throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidNumber));
    if (is.ch() == '+' || is.ch() == '-')
      numStr.push_back(is.get());
    if (is.eoi() || !std::isdigit(is.ch()))
      throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidNumber));
    while (!is.eoi() && std::isdigit(is.ch()))
      numStr.push_back(is.get());
  }

  JsonNum_t ret;
  size_t idx = 0;

  try {
    ret = strToJsonNum_t<JsonNum_t>(numStr, &idx);
  } catch (const std::invalid_argument &) {
    throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidNumber));
  } catch (const std::out_of_range &) {
    throw std::runtime_error(getJsonErrorMsg(JsonErrorCode::InvalidNumber));
  }
  node->m_value.emplace<JsonNum_t>(ret);
}

#endif
