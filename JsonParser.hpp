#ifndef JSON_PARSER_HPP_
#define JSON_PARSER_HPP_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <map>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#if __cplusplus >= 202002L
#include <version>
#endif

#ifdef __cpp_lib_format
#define JSON_PARSER_USE_FORMAT 1
#else
#define JSON_PARSER_USE_FORMAT 0
#include <cstdio>
#include <sstream>
#endif

#if JSON_PARSER_USE_FORMAT
#include <format>
#endif

/*
clang-format off

Synopsis:

constexpr struct JsonNull_t {} JsonNull;

using JsonStr_t = std::string;
using JsonArr_t = std::vector<JsonNode>;
using JsonObj_t = std::map<std::string, JsonNode>;

struct JsonKeyLiteral_t {
  std::string_view key;
  JsonKeyLiteral_t(const std::string_view &k);
};

JsonKeyLiteral_t operator""_key(const char *key, size_t len);

class JsonNode {
public:
  JsonNode();
  JsonNode(JsonNull_t);
  JsonNode(bool);
  JsonNode(const JsonStr_t &);
  JsonNode(JsonStr_t &&);
  JsonNode(const JsonArr_t &);
  JsonNode(JsonArr_t &&);
  JsonNode(const JsonObj_t &);
  JsonNode(JsonObj_t &&);
  template <typename NumberLike> JsonNode(NumberLike);
  JsonNode(std::nullptr_t);
  JsonNode(char);
  JsonNode(const char *);
  JsonNode(const std::initializer_list<JsonNode> &);
  JsonNode(const std::initializer_list<std::pair<const JsonKeyLiteral_t, JsonNode>> &);

  JsonNode(const JsonNode &);
  JsonNode(JsonNode &&) noexcept;

  ~JsonNode();

  void clear();

  void swap(JsonNode &) noexcept;

  JsonNode &operator=(JsonNode);

  JsonStr_t &str();
  const JsonStr_t &str() const;
  const char *c_str() const;
  JsonArr_t &arr();
  const JsonArr_t &arr() const;
  JsonObj_t &obj();
  const JsonObj_t &obj() const;

  void push_back(const JsonNode &);
  void push_back(JsonNode &&);

  JsonNode &operator[](size_t);
  JsonNode &at(size_t);
  const JsonNode &operator[](size_t) const;
  const JsonNode &at(size_t) const;

  JsonNode &operator[](const std::string &);
  JsonNode &at(const std::string &);
  const JsonNode &operator[](const std::string &) const;
  const JsonNode &at(const std::string &) const;

  bool contains(const JsonStr_t &) const;
  JsonObj_t::iterator find(const JsonStr_t &);
  JsonObj_t::const_iterator find(const JsonStr_t &) const;

  size_t size() const;

  JsonType type() const;
  std::string typeStr() const;

  bool isNull() const;
  bool isBool() const;
  bool isNum() const;
  bool isStr() const;
  bool isArr() const;
  bool isObj() const;

  template <typename T> T get() const;
  template <> bool get<bool>() const;
  template <> auto get<NumberLike>() const;
  template <> auto get<std::filesystem::path>() const;
  template <> auto get<ArrayLike>(const size_t n = -1, size_t offset = 0, const size_t stride = 1) const;
  template <> auto get<MapLike>() const;

  template <typename T> auto get(const std::string &key) const;
  template <typename T> auto get(const std::string &key, T &&fallback) const;

  template <typename Derived> class JsonOutputStreamBase {
  public:
    void put(char);
    void put(char, size_t);
    void puts(const char *, size_t);
  };

  class Serializer {
  public:
    Serializer &precision(int);
    Serializer &indent(int);
    Serializer &ascii(bool);
    Serializer &dump(std::ostream &);
    std::string dumps();
  };

  Serializer serializer() const;
};

class JsonParser {
public:
  JsonParser();
  JsonNode parse(std::string_view, size_t *offset = nullptr);
  JsonNode parse(std::ifstream &, bool checkEnd = true);
  JsonNode parse(std::istream &, bool checkEnd = true);
  JsonNode streamParse(std::string_view, size_t *offset = nullptr, bool *isComplete = nullptr);
  JsonNode streamParse(std::ifstream &, bool *isComplete = nullptr);
  JsonNode streamParse(std::istream &, bool *isComplete = nullptr);

  template <typename Derived> class JsonInputStreamBase {
  public:
    char ch() const;
    void next();
    char get();
    bool eoi() const;
  };

  template <typename Derived>
  JsonNode parse(JsonInputStreamBase<Derived> &, bool checkEnd = true);

  template <typename Derived>
  JsonNode streamParse(JsonInputStreamBase<Derived> &, bool *isComplete = nullptr);

};

JsonNode parseJsonString(std::string_view inputView, size_t *offset = nullptr);
JsonNode parseJsonFile(const std::filesystem::path &filename, bool checkEnd = true);
JsonNode parseJsonFile(std::ifstream &is, bool checkEnd = true);

JsonNode parseStreamJsonString(std::string_view inputView, size_t *offset = nullptr, bool *isComplete = nullptr);
JsonNode parseStreamJsonFile(const std::filesystem::path &filename, bool *isComplete = nullptr);
JsonNode parseStreamJsonFile(std::ifstream &is, bool *isComplete = nullptr);

std::istream &operator>>(std::ifstream &, JsonNode &);
std::istream &operator>>(std::istream &, JsonNode &);
std::ostream &operator<<(std::ostream &, const JsonNode &);
std::ostream &operator<<(std::ostream &, JsonNode::Serializer &);

*/

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
  Null = 0,
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

  // Traits
private:
  template <typename T> struct is_signed_integral {
    static constexpr bool value =
        std::is_integral_v<T> && std::is_signed_v<T> &&
        !std::is_same_v<T, char> && !std::is_same_v<T, bool>;
  };
  template <typename T>
  static constexpr bool is_signed_integral_v = is_signed_integral<T>::value;

  template <typename T> struct is_unsigned_integral {
    static constexpr bool value =
        std::is_integral_v<T> && std::is_unsigned_v<T> &&
        !std::is_same_v<T, char> && !std::is_same_v<T, bool>;
  };
  template <typename T>
  static constexpr bool is_unsigned_integral_v = is_unsigned_integral<T>::value;

  template <typename T, typename = void>
  struct has_subscript_op : std::false_type {};
  template <typename T>
  struct has_subscript_op<T,
                          std::void_t<decltype(std::declval<T &>()[size_t{}])>>
      : std::true_type {};
  template <typename T>
  static constexpr bool has_subscript_op_v = has_subscript_op<T>::value;

  template <typename T>
  using resize_op_t = decltype(std::declval<T>().resize(size_t{}));
  template <typename T, typename = void>
  struct has_resize_op : std::false_type {};
  template <typename T>
  struct has_resize_op<T, std::void_t<resize_op_t<T>>> : std::true_type {};
  template <typename T>
  static constexpr bool has_resize_op_v = has_resize_op<T>::value;

  template <typename T, typename = void> struct get_value_type {
    using type = std::remove_reference_t<decltype(std::declval<T &>()[0])>;
  };
  template <typename T>
  struct get_value_type<T, std::void_t<typename T::value_type>> {
    using type = typename T::value_type;
  };
  template <typename T>
  using get_value_type_t = typename get_value_type<T>::type;

  template <typename T, typename = void>
  struct has_size_op : std::false_type {};
  template <typename T>
  struct has_size_op<T, std::void_t<decltype(std::declval<T>().size())>>
      : std::true_type {};
  template <typename T>
  static constexpr bool has_size_op_v = has_size_op<T>::value;

  template <typename T, typename = void>
  struct has_length_op : std::false_type {};
  template <typename T>
  struct has_length_op<T, std::void_t<decltype(std::declval<T>().length())>>
      : std::true_type {};
  template <typename T>
  static constexpr bool has_static_length_op_v = has_length_op<T>::value;

  template <typename T>
  static constexpr bool is_unresizable_sequence_container_v =
      has_subscript_op_v<T> && !has_resize_op_v<T> && !std::is_pointer_v<T>;

  template <typename T>
  static constexpr bool is_resizable_sequence_container_v =
      has_subscript_op_v<T> && has_resize_op_v<T> && !std::is_pointer_v<T> &&
      !std::is_same_v<T, std::string>;

  template <typename T, typename = void>
  struct is_associative_container : std::false_type {};
  template <typename T>
  struct is_associative_container<
      T, std::void_t<typename T::key_type, typename T::mapped_type,
                     decltype(std::declval<T &>().emplace(
                         std::declval<typename T::key_type>(),
                         std::declval<typename T::mapped_type>()))>>
      : std::true_type {};
  template <typename T>
  static constexpr bool is_associative_container_v =
      is_associative_container<T>::value;

  // Constructors
public:
  JsonNode() = default;

  JsonNode(JsonNull_t) : ty_(NullType_), val_() {}

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
            typename std::enable_if_t<is_signed_integral_v<T>, int> = 0>
  JsonNode(T num) : ty_(IntType_) {
    val_.i = static_cast<int64_t>(num);
  }

  template <typename T,
            typename std::enable_if_t<is_unsigned_integral_v<T>, int> = 0>
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

  template <typename T, typename std::enable_if_t<
                            has_subscript_op_v<T> &&
                                (has_size_op_v<T> || has_static_length_op_v<T>),
                            int> = 0>
  JsonNode(const T &a, const size_t n = static_cast<size_t>(-1),
           size_t offset = 0, const size_t stride = 1) {
    ty_ = ArrType_;
    size_t sz{};
    if constexpr (has_size_op_v<T>)
      sz = a.size();
    else
      sz = a.length();
    val_.a = new JsonArr_t(n == static_cast<size_t>(-1) ? sz : n);
    for (size_t i = 0; offset < sz && i < n; offset += stride, ++i) {
      (*val_.a)[i] = a[offset];
    }
  }

  template <typename T, typename std::enable_if_t<
                            has_subscript_op_v<T> && !has_size_op_v<T> &&
                                !has_static_length_op_v<T>,
                            int> = 0>
  JsonNode(const T &a, const size_t n, size_t offset = 0,
           const size_t stride = 1) {
    ty_ = ArrType_;
    val_.a = new JsonArr_t(n);
    for (size_t i = 0; i < n; offset += stride, ++i) {
      (*val_.a)[i] = a[offset];
    }
  }

  template <typename T,
            typename std::enable_if_t<is_associative_container_v<T>, int> = 0>
  JsonNode(const T &m) {
    ty_ = ObjType_;
    val_.o = new JsonObj_t{};
    for (const auto &p : m) {
      val_.o->emplace(p.first, p.second);
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
    if (!isInternalPtr())
      return;

    std::stack<TraverseState> stateStack;
    stateStack.emplace(this);
    while (!stateStack.empty()) {
      auto node = stateStack.top().node;
      switch (node->ty_) {
      case ArrType_: {
        auto &arr = node->val_.a;
        auto &it = stateStack.top().arrIt;
        if (it != arr->cend()) {
          if (it->isInternalPtr()) {
            const auto child = &(*it);
            stateStack.emplace(child);
          }
          ++it;
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
          if (it->second.isInternalPtr()) {
            const auto child = &(it->second);
            stateStack.emplace(child);
          }
          ++it;
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
        // should not reach here
        break;
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
  void swap(JsonNode &other) noexcept {
    std::swap(ty_, other.ty_);
    std::swap(val_, other.val_);
  }

  JsonNode &operator=(JsonNode other) {
    swap(other);
    return *this;
  }

  // Index and key
private:
  void requireType(size_t ty) const {
    (void)ty; // Suppress unused variable warning
#ifndef NDEBUG
    if (ty_ != ty)
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidJsonAccess));
#endif
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
  const char *c_str() const {
    requireType(StrType_);
    return val_.s->c_str();
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
  bool isInternalPtr() const {
    return ty_ == StrType_ || ty_ == ArrType_ || ty_ == ObjType_;
  }

public:
  JsonType type() const {
    constexpr JsonType types[]{JsonType::Null, JsonType::Bool, JsonType::Num,
                               JsonType::Num,  JsonType::Num,  JsonType::Str,
                               JsonType::Arr,  JsonType::Obj};

    return types[ty_];
  }
  std::string typeStr() const {
    constexpr const char *types[]{"null", "bool", "num", "num",
                                  "num",  "str",  "arr", "obj"};
    return types[ty_];
  }

  bool isNull() const { return type() == JsonType::Null; }
  bool isBool() const { return type() == JsonType::Bool; }
  bool isNum() const { return type() == JsonType::Num; }
  bool isStr() const { return type() == JsonType::Str; }
  bool isArr() const { return type() == JsonType::Arr; }
  bool isObj() const { return type() == JsonType::Obj; }

  template <typename T>
  typename std::enable_if_t<std::is_same_v<T, bool>, T> get() const {
    if (ty_ == BoolType_)
      return val_.b;
    if (ty_ == IntType_)
      return static_cast<bool>(val_.i);
    if (ty_ == UintType_)
      return static_cast<bool>(val_.u);
    if (ty_ == DoubleType_)
      return static_cast<bool>(val_.d);
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::InvalidJsonAccess));
  }

  template <typename T>
  typename std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>,
                            T>
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
  typename std::enable_if_t<std::is_same_v<T, std::filesystem::path>,
                            std::filesystem::path>
  get() const {
    requireType(StrType_);
#if __cplusplus >= 202002L
    return std::filesystem::path(std::u8string(val_.s->begin(), val_.s->end()));
#else
    return std::filesystem::u8path(*val_.s);
#endif
  }

  template <typename T>
  typename std::enable_if_t<is_unresizable_sequence_container_v<T>, T>
  get(const size_t n = static_cast<size_t>(-1), size_t offset = 0,
      const size_t stride = 1) const {
    requireType(ArrType_);
    auto &arr = *val_.a;
    using ValueType = get_value_type_t<T>;
    T ret{};
    for (size_t i = 0; offset < arr.size() && i < n; offset += stride, ++i) {
      ret[i] = arr[offset].get<ValueType>();
    }
    return ret;
  }

  template <typename T>
  typename std::enable_if_t<is_resizable_sequence_container_v<T>, T>
  get(const size_t n = static_cast<size_t>(-1), size_t offset = 0,
      const size_t stride = 1) const {
    requireType(ArrType_);
    auto &arr = *val_.a;
    using ValueType = get_value_type_t<T>;
    T ret{};
    ret.resize(n == static_cast<size_t>(-1) ? arr.size() : n);
    for (size_t i = 0; offset < arr.size() && i < n; offset += stride, ++i) {
      ret[i] = arr[offset].get<ValueType>();
    }
    return ret;
  }

  template <typename T>
  typename std::enable_if_t<
      is_associative_container_v<T> &&
          std::is_convertible_v<std::string, typename T::key_type>,
      T>
  get() const {
    requireType(ObjType_);
    auto &obj = *val_.o;
    using MappedType = typename T::mapped_type;
    T ret{};
    for (const auto &p : obj) {
      ret.emplace(p.first, p.second.get<MappedType>());
    }
    return ret;
  }

  template <typename T>
  typename std::enable_if_t<std::is_same_v<std::string, T>, T> get() const {
    switch (ty_) {
    case NullType_:
      return "null";
    case BoolType_:
      return val_.b ? "true" : "false";
    case DoubleType_:
#if JSON_PARSER_USE_FORMAT
      return std::format("{}", val_.d);
#else
    {
      std::stringstream ss;
      ss.precision(std::numeric_limits<double>::max_digits10);
      ss << val_.d;
      return ss.str();
    }
#endif
    case IntType_:
      return std::to_string(val_.i);
    case UintType_:
      return std::to_string(val_.u);
    case StrType_:
      return *val_.s;
    default:
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidJsonAccess));
    }
  }

  template <typename T> auto get(const std::string &key) const {
    requireType(ObjType_);
    return val_.o->at(key).get<T>();
  }

  template <typename T> auto get(const std::string &key, T &&fallback) const {
    requireType(ObjType_);
    auto it = val_.o->find(key);
    if (it != val_.o->end())
      return it->second.get<T>();
    return std::forward<T>(fallback);
  }

  // serialization
public:
  template <typename Derived> class JsonOutputStreamBase;

private:
  template <size_t BufSize, typename Derived> class JsonStringRingBuffer {
  public:
    JsonStringRingBuffer(JsonOutputStreamBase<Derived> &os)
        : m_os(os), m_pos(0) {}

    ~JsonStringRingBuffer() {
      if (m_pos)
        dumpBuffer();
    }

    void put(char c) {
      if (m_pos == BufSize)
        dumpBuffer();
      m_buf[m_pos++] = c;
    }

    void put(char c, size_t rep) {
      size_t f = free();
      while (rep > f) {
        std::fill_n(m_buf + m_pos, f, c);
        m_pos = BufSize;
        dumpBuffer();
        rep -= f;
        f = BufSize;
      }
      std::fill_n(m_buf + m_pos, rep, c);
      m_pos += rep;
    }

    void puts(const char *s, size_t n) {
      size_t f = free();
      while (n > f) {
        std::copy_n(s, f, m_buf + m_pos);
        m_pos = BufSize;
        dumpBuffer();
        s += f;
        n -= f;
        f = BufSize;
      }
      std::copy_n(s, n, m_buf + m_pos);
      m_pos += n;
    }

  private:
    void dumpBuffer() {
      m_os.puts_(m_buf, m_pos);
      m_pos = 0;
    }
    size_t free() const { return BufSize - m_pos; }

  private:
    JsonOutputStreamBase<Derived> &m_os;
    char m_buf[BufSize];
    size_t m_pos;
  };

public:
  template <typename Derived> class JsonOutputStreamBase {
    friend class JsonStringRingBuffer<256, Derived>;

  public:
    JsonOutputStreamBase() : m_buf(*this) {}

    void put(char c) { m_buf.put(c); }
    void put(char c, size_t rep) { m_buf.put(c, rep); }
    void puts(const char *s, size_t n) { m_buf.puts(s, n); }

  private:
    void puts_(const char *s, size_t n) {
      static_cast<Derived *>(this)->puts_(s, n);
    }

  private:
    JsonStringRingBuffer<256, Derived> m_buf;
  };

  class JsonStringOutputStream
      : public JsonOutputStreamBase<JsonStringOutputStream> {
  public:
    JsonStringOutputStream(std::string &str) : m_str(str) {}
    void puts_(const char *s, size_t n) { m_str.append(s, n); }

  private:
    std::string &m_str;
  };

  class JsonFileOutputStream
      : public JsonOutputStreamBase<JsonFileOutputStream> {
  public:
    JsonFileOutputStream(std::ostream &os) : m_os(os) {}
    void puts_(const char *s, size_t n) { m_os.write(s, n); }

  private:
    std::ostream &m_os;
  };

public:
  class Serializer {
    friend class JsonNode;

    Serializer(const JsonNode &node) : m_node(node) {}

  public:
    Serializer(const Serializer &) = delete;
    Serializer &operator=(const Serializer &) = delete;

    Serializer &precision(int p) {
      m_precision = p;
      return *this;
    }

    Serializer &indent(int i) {
      m_indent = i;
      return *this;
    }

    Serializer &ascii(bool a) {
      m_ascii = a;
      return *this;
    }

    template <typename Derived>
    Serializer &dump(JsonOutputStreamBase<Derived> &os) {
      bool formatted = (m_indent != -1);

      std::stack<ConstTraverseState> stateStack;
      stateStack.emplace(&m_node);

      while (!stateStack.empty()) {
        auto node = stateStack.top().node;
        switch (node->ty_) {
        case NullType_:
          os.puts("null", 4);
          stateStack.pop();
          break;
        case BoolType_:
          node->val_.b ? os.puts("true", 4) : os.puts("false", 5);
          stateStack.pop();
          break;
        case DoubleType_:
          dumpDouble(os, node->val_.d, m_precision);
          stateStack.pop();
          break;
        case IntType_:
          dumpInt64(os, node->val_.i);
          stateStack.pop();
          break;
        case UintType_:
          dumpUint64(os, node->val_.u);
          stateStack.pop();
          break;
        case StrType_:
          os.put('"');
          dumpJsonString(os, *(node->val_.s), m_ascii);
          os.put('"');
          stateStack.pop();
          break;
        case ArrType_: {
          const auto &arr = *node->val_.a;
          if (arr.empty()) {
            os.puts("[]", 2);
            stateStack.pop();
            break;
          }
          auto &it = stateStack.top().arrIt;
          if (it == arr.cbegin()) {
            os.put('[');
            if (formatted) {
              os.put('\n');
              os.put(' ', m_indent * stateStack.size());
            }
          } else if (it != arr.cend()) {
            os.put(',');
            if (formatted) {
              os.put('\n');
              os.put(' ', m_indent * stateStack.size());
            }
          }
          if (it != arr.cend()) {
            const auto child = &(*it);
            ++it;
            stateStack.emplace(child);
          } else {
            if (formatted) {
              os.put('\n');
              os.put(' ', m_indent * (stateStack.size() - 1));
            }
            os.put(']');
            stateStack.pop();
          }
        } break;
        case ObjType_: {
          const auto &obj = *node->val_.o;
          if (obj.empty()) {
            os.puts("{}", 2);
            stateStack.pop();
            break;
          }
          auto &it = stateStack.top().objIt;
          if (it == obj.cbegin()) {
            os.put('{');
            if (formatted) {
              os.put('\n');
              os.put(' ', m_indent * stateStack.size());
            }
          } else if (it != obj.cend()) {
            os.put(',');
            if (formatted) {
              os.put('\n');
              os.put(' ', m_indent * stateStack.size());
            }
          }

          if (it != obj.cend()) {
            os.put('"');
            dumpJsonString(os, it->first, m_ascii);
            os.puts("\":", 2);
            if (formatted)
              os.put(' ');
            const auto child = &(it->second);
            ++it;
            stateStack.emplace(child);
          } else {
            if (formatted) {
              os.put('\n');
              os.put(' ', m_indent * (stateStack.size() - 1));
            }
            os.put('}');
            stateStack.pop();
          }
        } break;
        default:
          stateStack.pop();
          break;
        }
      }
      return *this;
    }

    std::string dumps() {
      std::string str;
      JsonStringOutputStream os(str);
      dump(os);
      return str;
    }

  private:
    template <typename Derived>
    static void dumpInt64(JsonOutputStreamBase<Derived> &os, int64_t i) {
      if (i < 0) {
        os.put('-');
        dumpUint64(os, -i);
      } else {
        dumpUint64(os, i);
      }
    }

    template <typename Derived>
    static void dumpUint64(JsonOutputStreamBase<Derived> &os, uint64_t u) {
      char buf[20];
      char *p = buf + 19;
      while (u > 9) {
        *p-- = '0' + u % 10;
        u /= 10;
      }
      *p = '0' + u;
      os.puts(p, buf + 20 - p);
    }

    template <typename Derived>
    static void dumpDouble(JsonOutputStreamBase<Derived> &os, double d,
                           int precision) {
#if JSON_PARSER_USE_FORMAT
      std::string num = precision != -1 ? std::format("{:.{}f}", d, precision)
                                        : std::format("{}", d);
      os.puts(num.c_str(), num.size());
#else
      if (precision == -1)
        precision = std::numeric_limits<double>::max_digits10;
      char buf[64];
      int len = std::snprintf(buf, 64, "%.*g", precision, d);
      os.puts(buf, len);
#endif
    }

    template <typename Derived>
    static void dumpJsonString(JsonOutputStreamBase<Derived> &os,
                               const JsonStr_t &src, bool ascii) {
      for (auto ite = src.begin(); ite != src.end(); ++ite) {
        switch (*ite) {
        case '"':
        case '\\':
          os.put('\\');
          os.put(*ite);
          break;
        case '\b':
          os.puts("\\b", 2);
          break;
        case '\f':
          os.puts("\\f", 2);
          break;
        case '\n':
          os.puts("\\n", 2);
          break;
        case '\r':
          os.puts("\\r", 2);
          break;
        case '\t':
          os.puts("\\t", 2);
          break;
        default: {
          if (ascii && *ite & 0x80)
            decodeUtf8(os, ite);
          else
            os.put(*ite);
        } break;
        }
      }
    }

    template <typename Derived>
    static void decodeUtf8(JsonOutputStreamBase<Derived> &os,
                           std::string::const_iterator &ite) {
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

    template <typename Derived>
    static void toHex4(JsonOutputStreamBase<Derived> &os, uint32_t u) {
      char h[4];
      for (int i = 3; i >= 0; --i) {
        h[i] = u & 0xF;
        h[i] = h[i] < 10 ? (h[i] + '0') : (h[i] - 10 + 'a');
        u >>= 4;
      }
      os.puts("\\u", 2);
      os.puts(h, 4);
    }

  private:
    const JsonNode &m_node;
    int m_precision = -1;
    int m_indent = -1;
    bool m_ascii = true;
  };

public:
  Serializer serializer() const { return Serializer(*this); }

private:
  enum JsonInternalTypeId : size_t {
    NullType_ = 0,
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

  JsonNode streamParse(std::string_view, size_t *offset = nullptr,
                       bool *isComplete = nullptr);
  JsonNode streamParse(std::ifstream &, bool *isComplete = nullptr);
  JsonNode streamParse(std::istream &, bool *isComplete = nullptr);

  template <typename Derived> class JsonInputStreamBase {
  public:
    char ch() const {
      return (static_cast<const Derived *>(this))->ch();
    } // peek
    void next() { (static_cast<Derived *>(this))->next(); }
    char get() { return (static_cast<Derived *>(this))->get(); }
    bool eoi() const {
      return (static_cast<const Derived *>(this))->eoi();
    } // end of input
  };

  template <typename Derived>
  JsonNode parse(JsonInputStreamBase<Derived> &, bool checkEnd = true);

  template <typename Derived>
  JsonNode streamParse(JsonInputStreamBase<Derived> &,
                       bool *isComplete = nullptr);

private:
  template <size_t BufSize>
  class JsonFileInputStream
      : public JsonInputStreamBase<JsonFileInputStream<BufSize>> {
  public:
    JsonFileInputStream(std::istream &is)
        : m_is(is), m_bufPos(BufSize), m_endPos(static_cast<size_t>(-1)) {
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
      m_is.read(m_buf, BufSize);
      m_bufPos = 0;
      if (m_is.eof())
        m_endPos = m_is.gcount();
    }

  private:
    std::istream &m_is;
    char m_buf[BufSize];
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

private:
  template <typename Derived> void parseLoop(JsonInputStreamBase<Derived> &);

  template <typename Derived> void skipSpace(JsonInputStreamBase<Derived> &);

  template <typename Derived>
  void parseLiteral(JsonInputStreamBase<Derived> &, JsonNode *);

  template <typename Derived>
  JsonStr_t parseString(JsonInputStreamBase<Derived> &);

  template <typename Derived>
  void parseEscape(JsonInputStreamBase<Derived> &, JsonStr_t &);

  template <typename Derived>
  void parseUtf8(JsonInputStreamBase<Derived> &, JsonStr_t &);

  template <typename Derived>
  uint32_t parseHex4(JsonInputStreamBase<Derived> &);

  void encodeUtf8(JsonStr_t &, uint32_t);

  template <typename Derived> void beginArray(JsonInputStreamBase<Derived> &);

  template <typename Derived> void beginObject(JsonInputStreamBase<Derived> &);

  template <typename Derived>
  JsonStr_t parseKeyWithColon(JsonInputStreamBase<Derived> &);

  template <typename Derived> void handleComma(JsonInputStreamBase<Derived> &);

  template <typename Derived>
  void parseNumber(JsonInputStreamBase<Derived> &, JsonNode *);

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

template <typename Derived>
inline void JsonParser::parseLoop(JsonInputStreamBase<Derived> &is) {
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
      *node = JsonStr_t{};
      *node->val_.s = parseString(is);
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
}

template <typename Derived>
inline JsonNode JsonParser::parse(JsonInputStreamBase<Derived> &is,
                                  bool checkEnd) {
  JsonNode ret;

  m_nodeStack = {};
  m_nodeStack.push(&ret);

  parseLoop(is);

  skipSpace(is);
  if (checkEnd && !is.eoi())
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::InvalidJson));

  return ret;
}

template <typename Derived>
inline JsonNode JsonParser::streamParse(JsonInputStreamBase<Derived> &is,
                                        bool *isComplete) {
  JsonNode ret;

  m_nodeStack = {};
  m_nodeStack.push(&ret);

  if (isComplete != nullptr)
    *isComplete = true;
  try {
    parseLoop(is);
  } catch (const std::exception &) {
    if (isComplete != nullptr)
      *isComplete = false;
  }

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
  auto fileInputStream = JsonFileInputStream<256>(is);
  return parse(fileInputStream, checkEnd);
}

inline JsonNode JsonParser::parse(std::istream &is, bool checkEnd) {
  auto fileInputStream = JsonFileInputStream<1>(is);
  return parse(fileInputStream, checkEnd);
}

inline JsonNode JsonParser::streamParse(std::string_view inputView,
                                        size_t *offset, bool *isComplete) {
  auto stringViewInputStream =
      JsonStringViewInputStream(inputView, offset == nullptr ? 0 : *offset);
  auto ret = streamParse(stringViewInputStream, isComplete);
  if (offset != nullptr)
    *offset = stringViewInputStream.pos();
  return ret;
}

inline JsonNode JsonParser::streamParse(std::ifstream &is, bool *isComplete) {
  auto fileInputStream = JsonFileInputStream<256>(is);
  return streamParse(fileInputStream, isComplete);
}

inline JsonNode JsonParser::streamParse(std::istream &is, bool *isComplete) {
  auto fileInputStream = JsonFileInputStream<1>(is);
  return streamParse(fileInputStream, isComplete);
}

template <typename Derived>
inline void JsonParser::skipSpace(JsonInputStreamBase<Derived> &is) {
  while (!is.eoi() && (is.ch() == ' ' || is.ch() == '\t' || is.ch() == '\n' ||
                       is.ch() == '\r')) {
    is.next();
  }
}

template <typename Derived>
inline void JsonParser::parseLiteral(JsonInputStreamBase<Derived> &is,
                                     JsonNode *node) {
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

template <typename Derived>
inline JsonStr_t JsonParser::parseString(JsonInputStreamBase<Derived> &is) {
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

template <typename Derived>
inline void JsonParser::parseEscape(JsonInputStreamBase<Derived> &is,
                                    JsonStr_t &ret) {
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

template <typename Derived>
inline uint32_t JsonParser::parseHex4(JsonInputStreamBase<Derived> &is) {
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

template <typename Derived>
inline void JsonParser::parseUtf8(JsonInputStreamBase<Derived> &is,
                                  JsonStr_t &ret) {
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

template <typename Derived>
inline JsonStr_t
JsonParser::parseKeyWithColon(JsonInputStreamBase<Derived> &is) {
  auto key = parseString(is);
  skipSpace(is);
  if (is.eoi() || is.get() != ':')
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::InvalidKeyValuePair));

  return key;
}

template <typename Derived>
inline void JsonParser::beginArray(JsonInputStreamBase<Derived> &is) {
  auto *node = m_nodeStack.top();
  *node = JsonArr_t{};

  skipSpace(is);
  if (is.eoi())
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::UnexpectedEndOfInput));

  if (is.ch() == ']') {
    is.next();
    popStack();
  } else {
    m_nodeStack.push(&(node->val_.a->emplace_back()));
  }
}

template <typename Derived>
inline void JsonParser::beginObject(JsonInputStreamBase<Derived> &is) {
  auto *node = m_nodeStack.top();
  *node = JsonObj_t{};

  skipSpace(is);
  if (is.eoi())
    throw std::runtime_error(
        getJsonErrorMsg(detail::JsonErrorCode::UnexpectedEndOfInput));

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

template <typename Derived>
inline void JsonParser::handleComma(JsonInputStreamBase<Derived> &is) {
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

template <typename Derived>
inline void JsonParser::parseNumber(JsonInputStreamBase<Derived> &is,
                                    JsonNode *node) {
  std::string numStr;

  bool isSigned = false;
  bool isFloatingPoint = false;

  enum class NumType { DBL, INT, UINT } numType{};

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

  if (isFloatingPoint) {
    numType = NumType::DBL;
  } else if (isSigned) {
    constexpr std::string_view maxSignedIntStr = "9223372036854775808"; // 2^63
    size_t unsignedLen = numStr.size() - 1;
    bool overflow = false;
    if (unsignedLen == maxSignedIntStr.size())
      for (size_t i = 0; i < maxSignedIntStr.size(); ++i) {
        if (numStr[i + 1] == maxSignedIntStr[i])
          continue;
        overflow = numStr[i + 1] > maxSignedIntStr[i];
        break;
      }
    else
      overflow = unsignedLen > maxSignedIntStr.size();
    numType = overflow ? NumType::DBL : NumType::INT;
  } else {
    constexpr std::string_view maxUnsignedIntStr =
        "18446744073709551615"; // 2^64 - 1
    bool overflow = false;
    if (numStr.size() == maxUnsignedIntStr.size())
      for (size_t i = 0; i < maxUnsignedIntStr.size(); ++i) {
        if (numStr[i] == maxUnsignedIntStr[i])
          continue;
        overflow = numStr[i] > maxUnsignedIntStr[i];
        break;
      }
    else
      overflow = numStr.size() > maxUnsignedIntStr.size();
    numType = overflow ? NumType::DBL : NumType::UINT;
  }

  switch (numType) {
  case NumType::DBL: {
    double num = std::strtod(numStr.c_str(), nullptr);
    if (std::isinf(num))
      throw std::runtime_error(
          getJsonErrorMsg(detail::JsonErrorCode::InvalidNumber));
    *node = num;
  } break;
  case NumType::INT: {
    int64_t num{};
    for (size_t i = 1; i < numStr.size(); ++i)
      num = num * 10 - (numStr[i] - '0');
    *node = num;
  } break;
  case NumType::UINT: {
    uint64_t num{};
    for (size_t i = 0; i < numStr.size(); ++i)
      num = num * 10 + (numStr[i] - '0');
    *node = num;
  } break;
  }
}

inline JsonNode parseJsonString(std::string_view inputView,
                                size_t *offset = nullptr) {
  return JsonParser{}.parse(inputView, offset);
}

inline JsonNode parseJsonFile(const std::filesystem::path &filename,
                              bool checkEnd = true) {
  std::ifstream ifs(filename);
  return JsonParser{}.parse(ifs, checkEnd);
}

inline JsonNode parseJsonFile(std::ifstream &is, bool checkEnd = true) {
  return JsonParser{}.parse(is, checkEnd);
}

inline JsonNode parseStreamJsonString(std::string_view inputView,
                                      size_t *offset = nullptr,
                                      bool *isComplete = nullptr) {
  return JsonParser{}.streamParse(inputView, offset, isComplete);
}

inline JsonNode parseStreamJsonFile(const std::filesystem::path &filename,
                                    bool *isComplete = nullptr) {
  std::ifstream ifs(filename);
  return JsonParser{}.streamParse(ifs, isComplete);
}

inline JsonNode parseStreamJsonFile(std::ifstream &is,
                                    bool *isComplete = nullptr) {
  return JsonParser{}.streamParse(is, isComplete);
}

inline std::ifstream &operator>>(std::ifstream &is, JsonNode &node) {
  node = JsonParser{}.parse(is, true);
  return is;
}

inline std::istream &operator>>(std::istream &is, JsonNode &node) {
  node = JsonParser{}.parse(is, true);
  return is;
}

inline std::ostream &operator<<(std::ostream &os, const JsonNode &node) {
  JsonNode::JsonFileOutputStream fout(os);
  node.serializer().dump(fout);
  return os;
}

inline std::ostream &operator<<(std::ostream &os, JsonNode::Serializer &s) {
  JsonNode::JsonFileOutputStream fout(os);
  s.dump(fout);
  return os;
}

#endif
