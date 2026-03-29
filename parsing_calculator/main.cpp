#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

class Node {
 public:
  virtual ~Node() = default;
  virtual int evaluate() const = 0;
};

class Int : public Node {
 private:
  int m_value;

 public:
  ~Int() override;
  explicit Int(int value) : m_value(value) {}

  int evaluate() const override { return m_value; }
};

Int::~Int() = default;

class Add : public Node {
 private:
  std::unique_ptr<Node> m_left;
  std::unique_ptr<Node> m_right;

 public:
  ~Add() override;
  explicit Add(std::unique_ptr<Node> left, std::unique_ptr<Node> right)
      : m_left(std::move(left)), m_right(std::move(right)) {}

  int evaluate() const override {
    return m_left->evaluate() + m_right->evaluate();
  }
};

Add::~Add() = default;

class Mul : public Node {
 private:
  std::unique_ptr<Node> m_left;
  std::unique_ptr<Node> m_right;

 public:
  ~Mul() override;
  Mul(std::unique_ptr<Node> left, std::unique_ptr<Node> right)
      : m_left(std::move(left)), m_right(std::move(right)) {}

  int evaluate() const override {
    return m_left->evaluate() * m_right->evaluate();
  }
};

Mul::~Mul() = default;

std::unique_ptr<Node> parse_expression(const char*& first, const char* last);

std::unique_ptr<Node> parse_integer(const char*& first, const char* last) {
  int r = 0;

  while (first != last and *first >= '0' and *first <= '9') {
    r = r * 10 + (*first - '0');
    ++first;
  }

  return std::make_unique<Int>(r);
}

std::unique_ptr<Node> parse_term(const char*& first, const char* last) {
  if (first == last) {
    throw std::runtime_error("Unexpected end of output");
  }

  const auto ch = *first;
  if (ch >= '0' and ch <= '9') {
    return parse_integer(first, last);
  } else if (ch == '(') {
    ++first;
    auto result = parse_expression(first, last);

    if (first == last or *first != ')') {
      throw std::runtime_error("Expected ')'");
    }

    ++first;
    return result;
  }

  throw std::runtime_error("Expected integer or (expression)");
}

std::unique_ptr<Node> parse_factor(const char*& first, const char* last) {
  auto left = parse_term(first, last);

  while (first != last and *first == '*') {
    ++first;
    auto right = parse_term(first, last);

    left = std::make_unique<Mul>(std::move(left), std::move(right));
  }

  return left;
}

std::unique_ptr<Node> parse_expression(const char*& first, const char* last) {
  auto left = parse_factor(first, last);

  while (first != last and *first == '+') {
    ++first;
    auto right = parse_factor(first, last);

    left = std::make_unique<Add>(std::move(left), std::move(right));
  }

  return left;
}

int evaluate(std::string_view expr) {
  auto begin = expr.data();
  auto end = expr.data() + expr.size();

  return parse_expression(begin, end)->evaluate();
}

int main() {
  char const* expr = "(2+2)*2";

  std::cout << "Result: " << evaluate(expr);
}