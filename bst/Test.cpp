#include <gtest/gtest.h>

#include "Bst.h"

// ─────────────────────────────────────────────
// 1. EMPTY TREE
// ─────────────────────────────────────────────

TEST(BSTEmpty, SearchReturnsFalse) {
  BST tree;
  EXPECT_FALSE(tree.search(42));
}

TEST(BSTEmpty, SizeIsZero) {
  BST tree;
  EXPECT_EQ(tree.size(), 0);
}

TEST(BSTEmpty, InorderIsEmpty) {
  BST tree;
  EXPECT_TRUE(tree.inorder().empty());
}

// ─────────────────────────────────────────────
// 2. INSERT & SEARCH
// ─────────────────────────────────────────────

TEST(BSTInsert, SingleElement_CanBeFound) {
  BST tree;
  tree.insert(10);
  EXPECT_TRUE(tree.search(10));
}

TEST(BSTInsert, InsertedElementNotConfusedWithOthers) {
  BST tree;
  tree.insert(10);
  EXPECT_FALSE(tree.search(5));
  EXPECT_FALSE(tree.search(15));
}

TEST(BSTInsert, MultipleElements_AllFound) {
  BST tree;
  for (int v : {10, 5, 15, 3, 7, 12, 20})
    tree.insert(v);
  for (int v : {10, 5, 15, 3, 7, 12, 20})
    EXPECT_TRUE(tree.search(v));
}

TEST(BSTInsert, AbsentElementNotFound) {
  BST tree;
  for (int v : {10, 5, 15})
    tree.insert(v);
  EXPECT_FALSE(tree.search(99));
}

// ─────────────────────────────────────────────
// 3. DUPLICATES  (std::map semantics: ignore)
// ─────────────────────────────────────────────

TEST(BSTDuplicate, SizeUnchangedAfterDuplicateInsert) {
  BST tree;
  tree.insert(10);
  tree.insert(10);  // duplicate — should be ignored
  EXPECT_EQ(tree.size(), 1);
}

TEST(BSTDuplicate, InorderContainsValueOnce) {
  BST tree;
  tree.insert(10);
  tree.insert(10);
  auto result = tree.inorder();
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result[0], 10);
}

// ─────────────────────────────────────────────
// 4. SIZE
// ─────────────────────────────────────────────

TEST(BSTSize, GrowsWithEachUniqueInsert) {
  BST tree;
  tree.insert(1);
  EXPECT_EQ(tree.size(), 1);
  tree.insert(2);
  EXPECT_EQ(tree.size(), 2);
  tree.insert(3);
  EXPECT_EQ(tree.size(), 3);
}

// ─────────────────────────────────────────────
// 5. INORDER TRAVERSAL  (must return sorted output)
// ─────────────────────────────────────────────

TEST(BSTInorder, SingleElement) {
  BST tree;
  tree.insert(42);
  EXPECT_EQ(tree.inorder(), std::vector<int>({42}));
}

TEST(BSTInorder, ReturnsSortedSequence) {
  BST tree;
  for (int v : {5, 3, 8, 1, 4, 7, 9})
    tree.insert(v);
  std::vector<int> expected = {1, 3, 4, 5, 7, 8, 9};
  EXPECT_EQ(tree.inorder(), expected);
}

TEST(BSTInorder, InsertInReverseSortedOrder_StillSorted) {
  BST tree;
  for (int v : {9, 8, 7, 6, 5})
    tree.insert(v);
  std::vector<int> expected = {5, 6, 7, 8, 9};
  EXPECT_EQ(tree.inorder(), expected);
}

// ─────────────────────────────────────────────
// 6. DELETE  — three structural cases
// ─────────────────────────────────────────────

// Case 1: leaf node
TEST(BSTDelete, RemoveLeaf_NotFoundAfterwards) {
  BST tree;
  for (int v : {10, 5, 15})
    tree.insert(v);
  tree.remove(5);
  EXPECT_FALSE(tree.search(5));
  EXPECT_EQ(tree.size(), 2);
}

TEST(BSTDelete, RemoveLeaf_OtherNodesUntouched) {
  BST tree;
  for (int v : {10, 5, 15})
    tree.insert(v);
  tree.remove(5);
  EXPECT_TRUE(tree.search(10));
  EXPECT_TRUE(tree.search(15));
}

// Case 2: node with one child
TEST(BSTDelete, RemoveNodeWithOneChild) {
  BST tree;
  for (int v : {10, 5, 3})
    tree.insert(v);  // 5 has only left child 3
  tree.remove(5);
  EXPECT_FALSE(tree.search(5));
  EXPECT_TRUE(tree.search(3));
  EXPECT_TRUE(tree.search(10));
}

// Case 3: node with two children
TEST(BSTDelete, RemoveNodeWithTwoChildren) {
  BST tree;
  for (int v : {10, 5, 15, 3, 7})
    tree.insert(v);  // 5 has two children
  tree.remove(5);
  EXPECT_FALSE(tree.search(5));
  EXPECT_EQ(tree.size(), 4);
  // Tree must still be a valid BST
  std::vector<int> result = tree.inorder();
  EXPECT_EQ(result, std::vector<int>({3, 7, 10, 15}));
}

// Delete root
TEST(BSTDelete, RemoveRoot_TreeRemainsValid) {
  BST tree;
  for (int v : {10, 5, 15})
    tree.insert(v);
  tree.remove(10);
  EXPECT_FALSE(tree.search(10));
  std::vector<int> result = tree.inorder();
  EXPECT_EQ(result, std::vector<int>({5, 15}));
}

// Delete from single-node tree
TEST(BSTDelete, RemoveOnlyNode_TreeBecomesEmpty) {
  BST tree;
  tree.insert(42);
  tree.remove(42);
  EXPECT_EQ(tree.size(), 0);
  EXPECT_FALSE(tree.search(42));
  EXPECT_TRUE(tree.inorder().empty());
}

// Delete non-existent value (should not crash or corrupt)
TEST(BSTDelete, RemoveAbsentValue_NoEffect) {
  BST tree;
  for (int v : {10, 5, 15})
    tree.insert(v);
  tree.remove(99);  // no-op
  EXPECT_EQ(tree.size(), 3);
  EXPECT_EQ(tree.inorder(), std::vector<int>({5, 10, 15}));
}

// ─────────────────────────────────────────────
// 7. EDGE / STRESS
// ─────────────────────────────────────────────

TEST(BSTEdge, NegativeValues) {
  BST tree;
  for (int v : {-5, -10, 0, -3})
    tree.insert(v);
  std::vector<int> expected = {-10, -5, -3, 0};
  EXPECT_EQ(tree.inorder(), expected);
}

TEST(BSTEdge, LargeSequentialInserts_SortedOutput) {
  BST tree;
  for (int i = 100; i >= 1; --i)
    tree.insert(i);  // worst-case shape
  ASSERT_EQ(tree.size(), 100);
  auto result = tree.inorder();
  for (int i = 0; i < 100; ++i)
    EXPECT_EQ(result[i], i + 1);
}