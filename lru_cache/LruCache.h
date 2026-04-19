#pragma once

#include <iostream>
#include <list>
#include <optional>
#include <unordered_map>

/* get is then just: find in map → remove → insertFront → return value.
put is: if key exists, update + remove + insertFront; if new and full, delete
LRU (tail->prev) from both structures, then create new node + insertFront +
insert in map. */

template <typename KeyT, typename ValT>
class LruCache {
 public:
  LruCache() = delete;
  explicit LruCache(std::size_t cap) : _capacity{cap} {}

  void put(const KeyT& key, const ValT& val) {
    if (auto it = _map.find(key); it != _map.end()) {
      auto& [_, mapIt] = *it;
      _list.erase(mapIt);
      auto insertIt = _list.insert(_list.begin(), Node{key, val});
      mapIt = insertIt;
    } else {
      if (_size >= _capacity) {
        _map.erase(_list.back().key);
        _list.pop_back();
        --_size;
      }

      auto insertIt = _list.insert(_list.begin(), Node{key, val});
      _map.emplace(key, insertIt);
      ++_size;
    }
  }

  std::optional<ValT> get(const KeyT& key) {
    if (auto it = _map.find(key); it != _map.end()) {
      auto& [_, mapIt] = *it;
      _list.splice(_list.begin(), _list, mapIt);
      mapIt = _list.begin();

      return _list.front().val;
    }

    return std::nullopt;
  }

 private:
  struct Node {
    KeyT key;
    ValT val;
  };

  std::size_t _size = 0;
  const std::size_t _capacity = 0;
  std::list<Node> _list{};
  std::unordered_map<KeyT, typename decltype(_list)::iterator> _map{};
};