#pragma once

#include <concepts>
#include <memory>
#include <stack>
#include <type_traits>
#include <vector>

template <typename T = int>
  requires std::totally_ordered<T>
class BST {
 private:
  struct Node {
    T value{};
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
  };

 public:
  BST() = default;

  ~BST() {
    std::stack<std::unique_ptr<Node>> stack;
    if (_head)
      stack.push(std::move(_head));

    while (!stack.empty()) {
      auto node = std::move(stack.top());
      stack.pop();
      if (node->right)
        stack.push(std::move(node->right));
      if (node->left)
        stack.push(std::move(node->left));
    }
  }

  BST(const BST&) = delete;
  BST& operator=(const BST&) = delete;
  BST(BST&&) noexcept = default;
  BST& operator=(BST&&) noexcept = default;

  void insert(const T& value) { insertImpl(_head, value); }

  bool search(const T& value) const { return findNode(value) != nullptr; }

  void remove(const T& value) {
    auto& node = findNode(value);
    if (!node) {
      return;  // not found
    }

    // no children case
    if ((node->left == nullptr) and (node->right == nullptr)) {
      node.reset();
      --_size;
      return;
    }

    // one children case
    if ((node->left == nullptr) xor (node->right == nullptr)) {
      if (node->left != nullptr) {
        node = std::move(node->left);
      } else if (node->right != nullptr) {
        node = std::move(node->right);
      }
      --_size;
      return;
    }

    // two children case
    if ((node->left != nullptr) and (node->right != nullptr)) {
      auto* current = &(node->right);

      while ((*current)->left != nullptr) {
        current = &((*current)->left);
      }

      node->value = (*current)->value;
      (*current) = std::move((*current)->right);
      --_size;
    }
  }

  std::size_t size() const { return _size; }

  std::vector<T> inorder() const {
    std::stack<Node*> stack{};
    std::vector<T> res{};

    auto current = _head.get();
    while (current != nullptr or !stack.empty()) {
      while (current != nullptr) {
        stack.push(current);
        current = current->left.get();
      }

      current = stack.top();
      stack.pop();
      res.push_back(current->value);
      current = current->right.get();
    }

    return res;
  }

 private:
  void insertImpl(std::unique_ptr<Node>& node, const T& value) {
    if (!node) {
      node = std::make_unique<Node>(value);
      ++_size;
      return;
    }

    if (value < node->value) {
      insertImpl(node->left, value);
    } else if (value > node->value) {
      insertImpl(node->right, value);
    } else {
      // drop duplicate
    }
  }

  std::unique_ptr<Node>& findNode(const T& value) {
    return const_cast<std::unique_ptr<Node>&>(
        std::as_const(*this).findNode(value));
  }

  const std::unique_ptr<Node>& findNode(const T& value) const {
    auto* current = &_head;

    while ((*current) != nullptr) {
      if (value == (*current)->value) {
        break;
      } else if (value < (*current)->value) {
        current = &(*current)->left;
      } else {
        current = &(*current)->right;
      }
    }

    return *current;
  }

  std::size_t _size = 0;
  std::unique_ptr<Node> _head;
};