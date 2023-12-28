#ifndef JSON_PARSER_HPP_
#define JSON_PARSER_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iomanip>
#include <ios>
#include <iosfwd>
#include <map>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

// #define JSON_ENABLE_EMPTY_NODE
// If JSON_ENABLE_EMPTY_NODE is defined, JsonNode can be empty to distinguish
// from null. Otherwise, JsonNode is null by default.
// Note that empty values are not valid json values and need to be cleared
// manually before calling toString() or size() in order to get correct results.

inline constexpr struct JsonNull_t {
} JsonNull;

class JsonNode;

using JsonStr_t = std::string;
using JsonArr_t = std::vector<JsonNode>;
using JsonObj_t = std::map<std::string, JsonNode>;

struct JsonKeyLiteral_t {
  std::string_view key;
  JsonKeyLiteral_t(const std::string_view &k) : key(k) {}
};

inline JsonKeyLiteral_t operator""_key(const char *key, size_t len) {
  return JsonKeyLiteral_t{std::string_view(key, len)};
}

enum class JsonType : uint8_t {
#ifdef JSON_ENABLE_EMPTY_NODE
  Empty = 0,
  Null,
#else
  Null = 0,
#endif
  Bool,
  Num,
  Str,
  Arr,
  Obj,
};

namespace detail {

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

  InvalidJsonAccess,
};

inline const char *JsonErrorMsg[]{
    "invalid json",           "unexpected end of input",
    "invalid literal",        "invalid number",
    "invalid string",         "invalid character",
    "invalid unicode",        "invalid escape sequence",
    "invalid key-value pair", "invalid array or object",
    "invalid json access",
};

inline const char *getJsonErrorMsg(JsonErrorCode code) {
  return JsonErrorMsg[static_cast<uint8_t>(code)];
}
}; // namespace detail

class JsonNode {
  friend class JsonParser;

private:
  struct TraverseState {
    JsonNode *node;
    union {
      JsonArr_t::iterator arrIt;
      JsonObj_t::iterator objIt;
    };

    TraverseState(JsonNode *n) : node(n) {
      if (n->isArr())
        arrIt = n->val_.a->begin();
      else if (n->isObj())
        objIt = n->val_.o->begin();
    }
  };

  struct ConstTraverseState {
    const JsonNode *node;
    union {
      JsonArr_t::const_iterator arrIt;
      JsonObj_t::const_iterator objIt;
    };

    ConstTraverseState(const JsonNode *n) : node(n) {
      if (n->isArr())
        arrIt = n->val_.a->cbegin();
      else if (n->isObj())
        objIt = n->val_.o->cbegin();
    }
  };

private:
  template <typename T> struct isSignedIntegral {
    static constexpr bool value =
        std::is_integral_v<T> && std::is_signed_v<T> &&
        !std::is_same_v<T, char> && !std::is_same_v<T, bool>;
  };

  template <typename T> struct isUnsignedIntegral {
    static constexpr bool value =
        std::is_integral<T>::value && std::is_unsigned<T>::value &&
        !std::is_same_v<T, char> && !std::is_same_v<T, bool>;
  };

  // Constructors
public:
  JsonNode() = default;

  JsonNode(JsonNull_t) : ty_(NullType_) {}

  JsonNode(bool b) : ty_(BoolType_) { val_.b = b; }

  JsonNode(const JsonStr_t &str) : ty_(StrType_) {
    val_.s = new JsonStr_t(str);
  }

  JsonNode(JsonStr_t &&str) : ty_(StrType_) {
    val_.s = new JsonStr_t(std::move(str));
  }

  JsonNode(const JsonArr_t &arr) : ty_(ArrType_) {
    val_.a = new JsonArr_t(arr);
  }
  JsonNode(JsonArr_t &&arr) : ty_(ArrType_) {
    val_.a = new JsonArr_t(std::move(arr));
  }

  JsonNode(const JsonObj_t &obj) : ty_(ObjType_) {
    val_.o = new JsonObj_t(obj);
  }
  JsonNode(JsonObj_t &&obj) : ty_(ObjType_) {
    val_.o = new JsonObj_t(std::move(obj));
  }

  template <typename T,
            typename std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
  JsonNode(T num) : ty_(DoubleType_) {
    val_.d = static_cast<double>(num);
  }

  template <typename T,
            typename std::enable_if_t<isSignedIntegral<T>::value, int> = 0>
  JsonNode(T num) : ty_(IntType_) {
    val_.i = static_cast<int64_t>(num);
  }

  template <typename T,
            typename std::enable_if_t<isUnsignedIntegral<T>::value, int> = 0>
  JsonNode(T num) : ty_(UintType_) {
    val_.u = static_cast<uint64_t>(num);
  }

  JsonNode(std::nullptr_t) : ty_(NullType_) {}

  JsonNode(char c) : ty_(StrType_) { val_.s = new JsonStr_t(1, c); }

  JsonNode(const char *str) : ty_(StrType_) { val_.s = new JsonStr_t(str); }

  JsonNode(const std::initializer_list<JsonNode> &arr) : ty_(ArrType_) {
    val_.a = new JsonArr_t(arr);
  }
  JsonNode(
      const std::initializer_list<std::pair<const JsonKeyLiteral_t, JsonNode>>
          &obj)
      : ty_(ObjType_) {
    val_.o = new JsonObj_t{};
    for (const auto &p : obj) {
      val_.o->emplace(p.first.key, p.second);
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
      node->ty_ = otherNode->ty_;
      switch (otherNode->ty_) {
      case ArrType_: {
        auto &otherArr = *otherNode->val_.a;
        auto &it = stateStack.top().arrIt;
        if (it == otherArr.cbegin()) {
          (node->val_.a = new JsonArr_t{})->reserve(otherArr.size());
        }
        if (it != otherArr.cend()) {
          const auto otherChild = &(*it);
          ++it;
          stateStack.emplace(otherChild);
          nodeStack.emplace(&(node->val_.a->emplace_back()));
        } else {
          stateStack.pop();
          nodeStack.pop();
        }
      } break;
      case ObjType_: {
        auto &otherObj = *otherNode->val_.o;
        auto &it = stateStack.top().objIt;
        if (it == otherObj.cbegin()) {
          node->val_.o = new JsonObj_t{};
        }
        if (it != otherObj.cend()) {
          const auto otherChild = &(it->second);
          stateStack.emplace(otherChild);
          nodeStack.emplace(
              &(node->val_.o->emplace(it->first, JsonNode{}).first->second));
          ++it;
        } else {
          stateStack.pop();
          nodeStack.pop();
        }
      } break;
      case StrType_:
        nodeStack.top()->val_.s = new JsonStr_t(*otherNode->val_.s);
        stateStack.pop();
        nodeStack.pop();
        break;
      case DoubleType_:
      case IntType_:
      case UintType_:
      case BoolType_:
        nodeStack.top()->val_ = otherNode->val_;
        stateStack.pop();
        nodeStack.pop();
        break;
      default:
        stateStack.pop();
        nodeStack.pop();
      }
    }
  }

  JsonNode(JsonNode &&other) noexcept : ty_(other.ty_), val_(other.val_) {
    other.ty_ = {};
  }

  // Destructor
public:
  ~JsonNode() {
#ifdef JSON_ENABLE_EMPTY_NODE
    if (isEmpty())
      return;
#endif
    std::stack<TraverseState> stateStack;
    stateStack.emplace(this);
    while (!stateStack.empty()) {
      auto node = stateStack.top().node;
      switch (node->ty_) {
      case ArrType_: {
        auto &arr = node->val_.a;
        auto &it = stateStack.top().arrIt;
        if (it != arr->cend()) {
          const auto child = &(*it);
          ++it;
          stateStack.emplace(child);
        } else {
          delete arr;
          node->ty_ = {};
          stateStack.pop();
        }
      } break;
      case ObjType_: {
        auto &obj = node->val_.o;
        auto &it = stateStack.top().objIt;
        if (it != obj->cend()) {
          const auto child = &(it->second);
          ++it;
          stateStack.emplace(child);
        } else {
          delete obj;
          node->ty_ = {};
          stateStack.pop();
        }
      } break;
      case StrType_:
        delete node->val_.s;
        node->ty_ = {};
        stateStack.pop();
        break;
      default:
        node->ty_ = {};
        stateStack.pop();
      }
    }
  }

  void clear() {
    switch (ty_) {
    case ArrType_:
      delete val_.a;
      break;
    case ObjType_:
      delete val_.o;
      break;
    case StrType_:
      delete val_.s;
      break;
    default:
      break;
    }
    ty_ = {};
  }

  // Assignment operators
public:
  JsonNode &operator=(const JsonNode &other) {
    if (this == &other)
      return *this;

    switch (other.ty_) {
    case ArrType_: {
      auto otherArr = *other.val_.a;
      if (ty_ == ArrType_)
        val_.a->swap(otherArr);
      else {
        *this = std::move(otherArr);
      }
    } break;
    case ObjType_: {
      auto otherObj = *other.val_.o;
      if (ty_ == ObjType_)
        *val_.o = std::move(otherObj);
      else {
        *this = std::move(otherObj);
      }
    } break;
    case StrType_: {
      auto otherStr = *other.val_.s;
      if (ty_ == StrType_)
        *val_.s = std::move(otherStr);
      else {
        *this = std::move(otherStr);
      }
    } break;
    case DoubleType_:
    case IntType_:
    case UintType_:
    case BoolType_:
      val_ = other.val_;
      break;
    default:
      break;
    }
    ty_ = other.ty_;
    return *this;
  }

  JsonNode &operator=(JsonNode &&other) noexcept {
    if (this == &other)
      return *this;
    clear();
    ty_ = other.ty_;
    val_ = other.val_;
    other.ty_ = {};
    return *this;
  }

  JsonNode &operator=(JsonNull_t) {
    clear();
    ty_ = NullType_;
    return *this;
  }

  JsonNode &operator=(bool b) {
    clear();
    ty_ = BoolType_;
    val_.b = b;
    return *this;
  }

  template <typename T>
  typename std::enable_if_t<std::is_floating_point_v<T>, JsonNode &>
  operator=(T value) {
    clear();
    ty_ = DoubleType_;
    val_.d = static_cast<double>(value);
    return *this;
  }

  template <typename T>
  typename std::enable_if_t<isSignedIntegral<T>::value, JsonNode &>
  operator=(T value) {
    clear();
    ty_ = IntType_;
    val_.i = static_cast<int64_t>(value);
    return *this;
  }

  template <typename T>
  typename std::enable_if_t<isUnsignedIntegral<T>::value, JsonNode &>
  operator=(T value) {
    clear();
    ty_ = UintType_;
    val_.u = static_cast<uint64_t>(value);
    return *this;
  }

  JsonNode &operator=(std::nullptr_t) {
    clear();
    ty_ = NullType_;
    return *this;
  }

  JsonNode &operator=(char c) {
    if (ty_ == StrType_)
      *val_.s = c;
    else {
      *this = JsonStr_t(1, c);
    }
    return *this;
  }

  JsonNode &operator=(const char *str) {
    if (ty_ == StrType_)
      *val_.s = str;
    else {
      *this = JsonStr_t(str);
    }
    return *this;
  }

  // Index and key
private:
  void requireType(size_t ty) const {
    if (ty_ != ty)
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidJsonAccess));
  }

public:
  JsonStr_t &str() {
    if (ty_ != StrType_) {
      *this = JsonStr_t{};
    }
    return *val_.s;
  }
  const JsonStr_t &str() const {
    requireType(StrType_);
    return *val_.s;
  }

  JsonArr_t &arr() {
    if (ty_ != ArrType_) {
      *this = JsonArr_t{};
    }
    return *val_.a;
  }
  const JsonArr_t &arr() const {
    requireType(ArrType_);
    return *val_.a;
  }

  JsonObj_t &obj() {
    if (ty_ != ObjType_) {
      *this = JsonObj_t{};
    }
    return *val_.o;
  }
  const JsonObj_t &obj() const {
    requireType(ObjType_);
    return *val_.o;
  }

  void push_back(const JsonNode &node) {
    if (ty_ != ArrType_) {
      *this = JsonArr_t{};
    }
    val_.a->push_back(node);
  }
  void push_back(JsonNode &&node) {
    if (ty_ != ArrType_) {
      *this = JsonArr_t{};
    }
    val_.a->push_back(std::move(node));
  }

  JsonNode &operator[](size_t idx) {
    requireType(ArrType_);
    return (*val_.a)[idx];
  }
  JsonNode &at(size_t idx) {
    requireType(ArrType_);
    return val_.a->at(idx);
  }
  const JsonNode &operator[](size_t idx) const {
    requireType(ArrType_);
    return (*val_.a)[idx];
  }
  const JsonNode &at(size_t idx) const {
    requireType(ArrType_);
    return val_.a->at(idx);
  }

  JsonNode &operator[](const std::string &key) {
    if (ty_ != ObjType_) {
      *this = JsonObj_t{};
    }
    return (*val_.o)[key];
  }
  JsonNode &at(const std::string &key) {
    requireType(ObjType_);
    return val_.o->at(key);
  }
  const JsonNode &operator[](const std::string &key) const {
    requireType(ObjType_);
    return val_.o->at(key);
  }
  const JsonNode &at(const std::string &key) const {
    requireType(ObjType_);
    return val_.o->at(key);
  }

  bool contains(const JsonStr_t &key) const {
    requireType(ObjType_);
    return val_.o->find(key) != val_.o->end();
  }

  JsonObj_t::iterator find(const JsonStr_t &key) {
    requireType(ObjType_);
    return val_.o->find(key);
  }

  JsonObj_t::const_iterator find(const JsonStr_t &key) const {
    requireType(ObjType_);
    return val_.o->find(key);
  }

  size_t size() const {
    if (ty_ == StrType_) {
      return val_.s->size();
    } else if (ty_ == ArrType_) {
      return val_.a->size();
    } else if (ty_ == ObjType_) {
      return val_.o->size();
    } else {
      return 0;
    }
  }

  // Type and getter
private:
public:
  JsonType type() const {
    constexpr JsonType types[]{
#ifdef JSON_ENABLE_EMPTY_NODE
        JsonType::Empty,
#endif
        JsonType::Null,  JsonType::Bool, JsonType::Num, JsonType::Num,
        JsonType::Num,   JsonType::Str,  JsonType::Arr, JsonType::Obj};

    return types[ty_];
  }
  std::string typeStr() const {
    constexpr const char *types[]{
#ifdef JSON_ENABLE_EMPTY_NODE
        "empty",
#endif
        "null",  "bool", "num", "num", "num", "str", "arr", "obj"};
    return types[ty_];
  }

#ifdef JSON_ENABLE_EMPTY_NODE
  bool isEmpty() const { return type() == JsonType::Empty; }
#endif

  bool isNull() const { return type() == JsonType::Null; }
  bool isBool() const { return type() == JsonType::Bool; }
  bool isNum() const { return type() == JsonType::Num; }
  bool isStr() const { return type() == JsonType::Str; }
  bool isArr() const { return type() == JsonType::Arr; }
  bool isObj() const { return type() == JsonType::Obj; }

  template <typename T>
  typename std::enable_if_t<std::is_same_v<T, bool>, T &> get() {
    requireType(BoolType_);
    return val_.b;
  }
  template <typename T>
  typename std::enable_if_t<std::is_same_v<T, JsonStr_t>, T &> get() {
    requireType(StrType_);
    return *val_.s;
  }
  template <typename T>
  typename std::enable_if_t<std::is_same_v<T, JsonArr_t>, T &> get() {
    requireType(ArrType_);
    return *val_.a;
  }
  template <typename T>
  typename std::enable_if_t<std::is_same_v<T, JsonObj_t>, T &> get() {
    requireType(ObjType_);
    return *val_.o;
  }

  template <typename T>
  typename std::enable_if_t<std::is_same_v<T, bool>, const T> get() const {
    requireType(BoolType_);
    return val_.b;
  }
  template <typename T>
  typename std::enable_if_t<std::is_same_v<T, JsonStr_t>, const T &>
  get() const {
    requireType(StrType_);
    return *val_.s;
  }
  template <typename T>
  typename std::enable_if_t<std::is_same_v<T, JsonArr_t>, const T &>
  get() const {
    requireType(ArrType_);
    return *val_.a;
  }
  template <typename T>
  typename std::enable_if_t<std::is_same_v<T, JsonObj_t>, const T &>
  get() const {
    requireType(ObjType_);
    return *val_.o;
  }

  template <typename T>
  typename std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>,
                            const T>
  get() const {
    switch (ty_) {
    case DoubleType_:
      return static_cast<T>(val_.d);
    case IntType_:
      return static_cast<T>(val_.i);
    case UintType_:
      return static_cast<T>(val_.u);
    default:
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidJsonAccess));
    }
  }

  template <typename T>
  typename std::enable_if_t<std::is_same_v<T, const char *>, const char *>
  get() const {
    requireType(StrType_);
    return val_.s->c_str();
  }

  template <typename T>
  typename std::enable_if_t<std::is_same_v<T, std::filesystem::path>,
                            std::filesystem::path>
  get() const {
    requireType(StrType_);
#if __cplusplus >= 202002L
    return std::filesystem::path(reinterpret_cast<char8_t *>(val_.s->data()));
#else
    return std::filesystem::u8path(*val_.s);
#endif
  }

  // Iterators
private:
  template <typename ElemType, typename ArrIter, typename ObjIter>
  class IteratorGen {
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

    IteratorGen operator++(int) {
      IteratorGen tmp = *this;
      ++(*this);
      return tmp;
    }

    IteratorGen &operator--() {
      if (m_iter.index() == 0)
        --std::get<0>(m_iter);
      else
        --std::get<1>(m_iter);
      return *this;
    }

    IteratorGen operator--(int) {
      IteratorGen tmp = *this;
      --(*this);
      return tmp;
    }

    bool operator==(const IteratorGen &rhs) const {
#ifdef _MSC_VER
      return m_iter.index() == rhs.m_iter.index() &&
             ((m_iter.index() == 0 &&
               std::get<0>(m_iter) == std::get<0>(rhs.m_iter)) ||
              (m_iter.index() == 1 &&
               std::get<1>(m_iter) == std::get<1>(rhs.m_iter)));
#else
      return m_iter == rhs.m_iter;
#endif
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
#ifdef _MSC_VER
      return m_iter.index() != rhs.m_iter.index() ||
             (m_iter.index() == 0 &&
              std::get<0>(m_iter) != std::get<0>(rhs.m_iter)) ||
             (m_iter.index() == 1 &&
              std::get<1>(m_iter) != std::get<1>(rhs.m_iter));
#else
      return m_iter != rhs.m_iter;
#endif
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

    ElemType &operator*() const {
      if (m_iter.index() == 0)
        return *std::get<0>(m_iter);
      else
        return std::get<1>(m_iter)->second;
    }
    ElemType *operator->() const {
      if (m_iter.index() == 0)
        return &*std::get<0>(m_iter);
      else
        return &std::get<1>(m_iter)->second;
    }

  private:
    std::variant<ArrIter, ObjIter> m_iter;
  };

public:
  using Iterator =
      IteratorGen<JsonNode, JsonArr_t::iterator, JsonObj_t::iterator>;
  using ConstIterator = IteratorGen<const JsonNode, JsonArr_t::const_iterator,
                                    JsonObj_t::const_iterator>;
  using ReverseIterator = IteratorGen<JsonNode, JsonArr_t::reverse_iterator,
                                      JsonObj_t::reverse_iterator>;
  using ConstReverseIterator =
      IteratorGen<const JsonNode, JsonArr_t::const_reverse_iterator,
                  JsonObj_t::const_reverse_iterator>;

  Iterator begin() {
    if (ty_ == ArrType_)
      return val_.a->begin();
    else if (ty_ == ObjType_)
      return val_.o->begin();
    else
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidJsonAccess));
  }
  Iterator end() {
    if (ty_ == ArrType_)
      return val_.a->end();
    else if (ty_ == ObjType_)
      return val_.o->end();
    else
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidJsonAccess));
  }
  ConstIterator begin() const {
    if (ty_ == ArrType_)
      return val_.a->cbegin();
    else if (ty_ == ObjType_)
      return val_.o->cbegin();
    else
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidJsonAccess));
  }
  ConstIterator end() const {
    if (ty_ == ArrType_)
      return val_.a->cend();
    else if (ty_ == ObjType_)
      return val_.o->cend();
    else
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidJsonAccess));
  }
  ConstIterator cbegin() const { return begin(); }
  ConstIterator cend() const { return end(); }
  ReverseIterator rbegin() {
    if (ty_ == ArrType_)
      return val_.a->rbegin();
    else if (ty_ == ObjType_)
      return val_.o->rbegin();
    else
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidJsonAccess));
  }
  ReverseIterator rend() {
    if (ty_ == ArrType_)
      return val_.a->rend();
    else if (ty_ == ObjType_)
      return val_.o->rend();
    else
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidJsonAccess));
  }
  ConstReverseIterator rbegin() const {
    if (ty_ == ArrType_)
      return val_.a->crbegin();
    else if (ty_ == ObjType_)
      return val_.o->crbegin();
    else
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidJsonAccess));
  }
  ConstReverseIterator rend() const {
    if (ty_ == ArrType_)
      return val_.a->crend();
    else if (ty_ == ObjType_)
      return val_.o->crend();
    else
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidJsonAccess));
  }
  ConstReverseIterator crbegin() const { return rbegin(); }
  ConstReverseIterator crend() const { return rend(); }

  // serialization
private:
  class Serializer {
  public:
    Serializer(const JsonNode &node) : m_node(node) {}
    Serializer(const Serializer &) = delete;
    Serializer &operator=(const Serializer &) = delete;

    Serializer &precision(size_t p) {
      m_precision = p;
      return *this;
    }

    Serializer &indent(size_t i) {
      m_indent = i;
      return *this;
    }

    Serializer &ascii(bool a) {
      m_ascii = a;
      return *this;
    }

    Serializer &dump(std::ostream &os) {
      size_t p = os.precision();
      os.precision(m_precision);
      m_node.dump(os, m_indent, m_ascii);
      os << std::setprecision(p);
      return *this;
    }

  private:
    const JsonNode &m_node;
    size_t m_precision = 16;
    size_t m_indent = -1;
    bool m_ascii = true;
  };

public:
  Serializer serializer() const { return Serializer(*this); }

  void dump(std::ostream &os, size_t indent = -1, bool ascii = true) const {

    bool formatted = (indent != static_cast<size_t>(-1));

    std::stack<ConstTraverseState> stateStack;
    stateStack.emplace(this);

    while (!stateStack.empty()) {
      auto node = stateStack.top().node;
      switch (node->ty_) {
      case NullType_:
        os << "null";
        stateStack.pop();
        break;
      case BoolType_:
        os << (node->val_.b ? "true" : "false");
        stateStack.pop();
        break;
      case DoubleType_:
        os << node->val_.d;
        stateStack.pop();
        break;
      case IntType_:
        os << node->val_.i;
        stateStack.pop();
        break;
      case UintType_:
        os << node->val_.u;
        stateStack.pop();
        break;
      case StrType_:
        os << "\"";
        toJsonString(os, *(node->val_.s), ascii);
        os << "\"";
        stateStack.pop();
        break;
      case ArrType_: {
        const auto &arr = *node->val_.a;
        if (arr.empty()) {
          os << "[]";
          stateStack.pop();
          break;
        }
        auto &it = stateStack.top().arrIt;
        if (it == arr.cbegin()) {
          if (formatted)
            os << "[\n" << std::setw(indent * stateStack.size()) << ' ';
          else
            os << '[';
        } else if (it != arr.cend()) {
          if (formatted)
            os << ",\n" << std::setw(indent * stateStack.size()) << ' ';
          else
            os << ',';
        }
        if (it != arr.cend()) {
          const auto child = &(*it);
          ++it;
          stateStack.emplace(child);
        } else {
          if (formatted)
            os << '\n'
               << std::string(indent * (stateStack.size() - 1), ' ') << ']';
          else
            os << ']';
          stateStack.pop();
        }
      } break;
      case ObjType_: {
        const auto &obj = *node->val_.o;
        if (obj.empty()) {
          os << "{}";
          stateStack.pop();
          break;
        }
        auto &it = stateStack.top().objIt;
        if (it == obj.cbegin()) {
          if (formatted)
            os << "{\n" << std::setw(indent * stateStack.size()) << ' ';
          else
            os << '{';
        } else if (it != obj.cend()) {
          if (formatted)
            os << ",\n" << std::setw(indent * stateStack.size()) << ' ';
          else
            os << ',';
        }

        if (it != obj.cend()) {
          os << "\"";
          toJsonString(os, it->first, ascii);
          os << "\":";
          const auto child = &(it->second);
          ++it;
          stateStack.emplace(child);
        } else {
          if (formatted)
            os << '\n'
               << std::string(indent * (stateStack.size() - 1), ' ') << '}';
          else
            os << '}';
          stateStack.pop();
        }
      } break;
      default:
        stateStack.pop();
        break;
      }
    }
  }

  std::string toString(size_t indent = -1, bool ascii = true) const {
    std::stringstream ss;
    ss << std::setprecision(16);
    dump(ss, indent, ascii);
    return ss.str();
  }

private:
  static void toJsonString(std::ostream &os, const JsonStr_t &src, bool ascii) {
    for (auto ite = src.begin(); ite != src.end(); ++ite) {
      switch (*ite) {
      case '"':
      case '\\':
      case '/':
        os << '\\' << *ite;
        break;
      case '\b':
        os << "\\b";
        break;
      case '\f':
        os << "\\f";
        break;
      case '\n':
        os << "\\n";
        break;
      case '\r':
        os << "\\r";
        break;
      case '\t':
        os << "\\t";
        break;
      default: {
        if (ascii && *ite & 0x80)
          decodeUtf8(os, ite);
        else
          os << *ite;
      } break;
      }
    }
  }

  static void decodeUtf8(std::ostream &os, std::string::const_iterator &ite) {
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
      uint32_t l = 0xDC00 + (u & 0x3FF);
      uint32_t u0 = ((u - 0x10000) >> 10) + 0xD800;
      if (u0 <= 0xDBFF) {
        toHex4(os, u0);
        toHex4(os, l);
      }
    }
    toHex4(os, u);
  }

  static void toHex4(std::ostream &os, uint32_t u) {
    char h[4];
    for (int i = 3; i >= 0; --i) {
      h[i] = u & 0xF;
      h[i] = h[i] < 10 ? (h[i] + '0') : (h[i] - 10 + 'a');
      u >>= 4;
    }
    os << "\\u" << h[0] << h[1] << h[2] << h[3];
  }

private:
  enum JsonInternalTypeId : size_t {
#ifdef JSON_ENABLE_EMPTY_NODE
    EmptyType_ = 0,
    NullType_,
#else
    NullType_ = 0,
#endif
    BoolType_,
    DoubleType_,
    IntType_,
    UintType_,
    StrType_,
    ArrType_,
    ObjType_,
  } ty_ = {};

  union {
    bool b;
    double d;
    int64_t i;
    uint64_t u;
    JsonStr_t *s;
    JsonArr_t *a;
    JsonObj_t *o;
  } val_;
};

class JsonParser {
public:
  JsonParser() = default;
  JsonParser(const JsonParser &) = delete;
  JsonParser &operator=(const JsonParser &) = delete;

  JsonNode parse(std::string_view, size_t *offset = nullptr);
  JsonNode parse(std::ifstream &, bool checkEnd = true);
  JsonNode parse(std::istream &, bool checkEnd = true);

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

  template <size_t BufSize>
  class JsonFileInputStream
      : public JsonInputStreamBase<JsonFileInputStream<BufSize>> {
  public:
    JsonFileInputStream(std::istream &is)
        : m_is(is), m_bufPos(BufSize), m_endPos(-1) {
      fillBuf();
    }

    ~JsonFileInputStream() {
      if (m_is.eof()) {
        if (!eoi()) {
          m_is.clear();
          m_is.seekg(m_bufPos - m_endPos, std::ios_base::end);
        }
      } else if (m_bufPos != BufSize) {
        m_is.seekg(m_bufPos - BufSize, std::ios_base::cur);
      }
    }

    char ch() const { return m_buf[m_bufPos]; }
    void next() {
      ++m_bufPos;
      if (m_bufPos == BufSize)
        fillBuf();
    }
    char get() {
      char c = m_buf[m_bufPos++];
      if (m_bufPos == BufSize)
        fillBuf();
      return c;
    }
    bool eoi() const { return m_bufPos == m_endPos; }

  private:
    void fillBuf() {
      m_is.read(m_buf.data(), BufSize);
      m_bufPos = 0;
      if (m_is.eof())
        m_endPos = m_is.gcount();
    }

  private:
    std::istream &m_is;
    std::array<char, BufSize> m_buf;
    size_t m_bufPos;
    size_t m_endPos;
  };

  class JsonStringViewInputStream
      : public JsonInputStreamBase<JsonStringViewInputStream> {
  public:
    JsonStringViewInputStream(const std::string_view &input, size_t offset)
        : m_input(input), m_pos(input.cbegin() + offset) {}
    char ch() const { return *m_pos; }
    void next() { ++m_pos; }
    char get() { return *m_pos++; }
    bool eoi() const { return m_pos == m_input.cend(); }

    size_t pos() const { return std::distance(m_input.cbegin(), m_pos); }

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
  parse(DerivedInputStream &, bool checkEnd);

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
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidJson));
  }

private:
  std::stack<JsonNode *> m_nodeStack;
};

template <>
class JsonParser::JsonFileInputStream<1>
    : public JsonInputStreamBase<JsonFileInputStream<1>> {
public:
  JsonFileInputStream(std::istream &is) : m_is(is) {}
  char ch() const { return m_is.peek(); }
  void next() { m_is.get(); }
  char get() { return m_is.get(); }
  bool eoi() const { return m_is.eof(); }

private:
  std::istream &m_is;
};

template <typename DerivedInputStream>
inline typename std::enable_if_t<
    JsonParser::IsJsonInputStream<DerivedInputStream>::value, JsonNode>
JsonParser::parse(DerivedInputStream &is, bool checkEnd) {
  m_nodeStack = {};

  JsonNode ret;

  m_nodeStack.push(&ret);

  while (!m_nodeStack.empty()) {
    skipSpace(is);
    if (is.eoi())
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::UnexpectedEndOfInput));

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
      *node = parseString(is);
      popStack();
      break;
    case '[':
      is.next();
      beginArray(is);
      break;
    case ']':
      is.next();
      if (!node->isArr())
        throw std::runtime_error(
            getJsonErrorMsg(detail::JsonErrorCode::InvalidJson));
      popStack();
      break;
    case '{':
      is.next();
      beginObject(is);
      break;
    case '}':
      is.next();
      if (!node->isObj())
        throw std::runtime_error(
            getJsonErrorMsg(detail::JsonErrorCode::InvalidJson));
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
        throw std::runtime_error(
            getJsonErrorMsg(detail::JsonErrorCode::InvalidJson));
      }
    }
  }

  skipSpace(is);
  if (checkEnd && !is.eoi())
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::InvalidJson));

  return ret;
}

inline JsonNode JsonParser::parse(std::string_view inputView, size_t *offset) {
  auto stringViewInputStream =
      JsonStringViewInputStream(inputView, offset == nullptr ? 0 : *offset);
  auto ret = parse(stringViewInputStream, offset == nullptr);
  if (offset != nullptr)
    *offset = stringViewInputStream.pos();
  return ret;
}

inline JsonNode JsonParser::parse(std::ifstream &is, bool checkEnd) {
  auto fileInputStream = JsonFileInputStream<64>(is);
  return parse(fileInputStream, checkEnd);
}

inline JsonNode JsonParser::parse(std::istream &is, bool checkEnd) {
  auto fileInputStream = JsonFileInputStream<1>(is);
  return parse(fileInputStream, checkEnd);
}

template <typename DerivedInputStream>
inline void JsonParser::skipSpace(DerivedInputStream &is) {
  while (!is.eoi() && (is.ch() == ' ' || is.ch() == '\t' || is.ch() == '\n' ||
                       is.ch() == '\r')) {
    is.next();
  }
}

template <typename DerivedInputStream>
inline void JsonParser::parseLiteral(DerivedInputStream &is, JsonNode *node) {
  if (is.ch() == 'n') {
    is.next();
    if (!is.eoi() && is.get() == 'u' && !is.eoi() && is.get() == 'l' &&
        !is.eoi() && is.get() == 'l') {
      *node = JsonNull;
      return;
    }
  }
  if (is.ch() == 't') {
    is.next();
    if (!is.eoi() && is.get() == 'r' && !is.eoi() && is.get() == 'u' &&
        !is.eoi() && is.get() == 'e') {
      *node = true;
      return;
    }
  }
  if (is.ch() == 'f') {
    is.next();
    if (!is.eoi() && is.get() == 'a' && !is.eoi() && is.get() == 'l' &&
        !is.eoi() && is.get() == 's' && !is.eoi() && is.get() == 'e') {
      *node = false;
      return;
    }
  }
  throw std::runtime_error(
      getJsonErrorMsg(detail::JsonErrorCode::InvalidLiteral));
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
            getJsonErrorMsg(detail::JsonErrorCode::InvalidCharacter));

      parseUtf8(is, ret);
    }
  }

  if (is.eoi())
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::InvalidString));

  is.next();
  return ret;
}

template <typename DerivedInputStream>
inline void JsonParser::parseEscape(DerivedInputStream &is, JsonStr_t &ret) {
  if (is.eoi())
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::InvalidString));

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
            getJsonErrorMsg(detail::JsonErrorCode::InvalidUnicode));

      if (is.eoi() || is.get() != 'u')
        throw std::runtime_error(
            getJsonErrorMsg(detail::JsonErrorCode::InvalidUnicode));

      uint32_t u2 = parseHex4(is);
      if (u2 < 0xDC00 || u2 > 0xDFFF)
        throw std::runtime_error(
            getJsonErrorMsg(detail::JsonErrorCode::InvalidUnicode));

      u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
    }
    encodeUtf8(ret, u);
  } break;
  default:
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::InvalidEscapeSequence));
  }
}

template <typename DerivedInputStream>
inline uint32_t JsonParser::parseHex4(DerivedInputStream &is) {
  uint32_t u = 0;
  for (int i = 0; i < 4; ++i) {
    if (is.eoi())
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidString));

    char c = is.get();
    u <<= 4;
    if (c >= '0' && c <= '9') {
      u |= c - '0';
    } else if (c >= 'a' && c <= 'f') {
      u |= c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      u |= c - 'A' + 10;
    } else {
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidUnicode));
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
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::InvalidCharacter));

  ret.push_back(is.get());
  while (byteCount-- > 1) {
    u = static_cast<uint8_t>(is.ch());
    if ((u >= 0x80) && (u <= 0xBF))
      ret.push_back(is.get());
    else
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidCharacter));
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
        getJsonErrorMsg(detail::JsonErrorCode::InvalidKeyValuePair));

  return key;
}

template <typename DerivedInputStream>
inline void JsonParser::beginArray(DerivedInputStream &is) {
  skipSpace(is);
  if (is.eoi())
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::UnexpectedEndOfInput));

  auto *node = m_nodeStack.top();
  *node = JsonArr_t{};
  if (is.ch() == ']') {
    is.next();
    popStack();
  } else {
    m_nodeStack.push(&(node->val_.a->emplace_back()));
  }
}

template <typename DerivedInputStream>
inline void JsonParser::beginObject(DerivedInputStream &is) {
  skipSpace(is);
  if (is.eoi())
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::UnexpectedEndOfInput));

  auto *node = m_nodeStack.top();
  *node = JsonObj_t{};
  if (is.ch() == '}') {
    is.next();
    popStack();
  } else {
    if (is.get() != '"')
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidKeyValuePair));

    auto key = parseKeyWithColon(is);
    m_nodeStack.push(
        &(node->val_.o->emplace(std::move(key), JsonNode{}).first->second));
  }
}

template <typename DerivedInputStream>
inline void JsonParser::handleComma(DerivedInputStream &is) {
  auto *node = m_nodeStack.top();
  if (node->ty_ == JsonNode::ArrType_) {
    m_nodeStack.push(&node->val_.a->emplace_back());
  } else if (node->ty_ == JsonNode::ObjType_) {
    skipSpace(is);
    if (is.eoi() || is.get() != '"')
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::UnexpectedEndOfInput));

    auto key = parseKeyWithColon(is);
    m_nodeStack.push(
        &(node->val_.o->emplace(std::move(key), JsonNode{}).first->second));
  } else {
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::InvalidArrayOrObject));
  }
}

template <typename DerivedInputStream>
inline void JsonParser::parseNumber(DerivedInputStream &is, JsonNode *node) {
  std::string numStr;

  bool isSigned = false;
  bool isFloatingPoint = false;

  if (is.ch() == '-') {
    isSigned = true;
    numStr.push_back(is.get());
  }
  if (is.eoi())
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::InvalidNumber));
  if (is.ch() == '0') {
    numStr.push_back(is.get());
  } else {
    if (!std::isdigit(is.ch()))
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidNumber));
    while (!is.eoi() && std::isdigit(is.ch()))
      numStr.push_back(is.get());
  }
  if (!is.eoi() && is.ch() == '.') {
    isFloatingPoint = true;
    numStr.push_back(is.get());
    if (is.eoi() || !std::isdigit(is.ch()))
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidNumber));
    while (!is.eoi() && std::isdigit(is.ch()))
      numStr.push_back(is.get());
  }
  if (!is.eoi() && (is.ch() == 'e' || is.ch() == 'E')) {
    isFloatingPoint = true;
    numStr.push_back(is.get());
    if (is.eoi())
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidNumber));
    if (is.ch() == '+' || is.ch() == '-')
      numStr.push_back(is.get());
    if (is.eoi() || !std::isdigit(is.ch()))
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidNumber));
    while (!is.eoi() && std::isdigit(is.ch()))
      numStr.push_back(is.get());
  }

  try {
    if (isFloatingPoint) {
      *node = std::stod(numStr);
    } else if (isSigned) {
      constexpr std::string_view maxSignedIntStr =
          "9223372036854775808"; // 2^63
      size_t unsignedLen = numStr.size() - 1;
      bool overflow = false;
      if (unsignedLen == maxSignedIntStr.size()) {
        for (size_t i = 0; i < maxSignedIntStr.size(); ++i) {
          if (numStr[i + 1] > maxSignedIntStr[i]) {
            overflow = true;
            break;
          } else if (numStr[i + 1] < maxSignedIntStr[i]) {
            break;
          }
        }
      } else {
        overflow = unsignedLen > maxSignedIntStr.size();
      }

      if (overflow) {
        *node = std::stod(numStr);
      } else {
        *node = std::stoll(numStr);
      }

    } else {
      constexpr std::string_view maxUnsignedIntStr =
          "18446744073709551615"; // 2^64 - 1
      bool overflow = false;
      if (numStr.size() == maxUnsignedIntStr.size()) {
        for (size_t i = 0; i < maxUnsignedIntStr.size(); ++i) {
          if (numStr[i] > maxUnsignedIntStr[i]) {
            overflow = true;
            break;
          } else if (numStr[i] < maxUnsignedIntStr[i]) {
            break;
          }
        }
      } else {
        overflow = numStr.size() > maxUnsignedIntStr.size();
      }

      if (overflow) {
        *node = std::stod(numStr);
      } else {
        *node = std::stoull(numStr);
      }
    }
  } catch (const std::out_of_range &) {
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::InvalidNumber));
  } catch (const std::invalid_argument &) {
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::InvalidNumber));
  }
}

inline JsonNode parseJsonString(std::string_view inputView,
                                size_t *offset = nullptr) {
  return JsonParser{}.parse(inputView, offset);
}

inline JsonNode parseJsonFile(std::string_view filename, bool checkEnd = true) {
  std::ifstream ifs(filename.data());
  return JsonParser{}.parse(ifs, checkEnd);
}

inline JsonNode parseJsonFile(std::ifstream &is, bool checkEnd = true) {
  return JsonParser{}.parse(is, checkEnd);
}

#endif
