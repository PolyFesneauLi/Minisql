#include "index/b_plus_tree.h"

#include <string>

#include "glog/logging.h"
#include "index/basic_comparator.h"
#include "index/generic_key.h"
#include "page/index_roots_page.h"

/**
 * TODO: Student Implement
 * @brief Initialize B+ tree
 */
BPlusTree::BPlusTree(index_id_t index_id, BufferPoolManager *buffer_pool_manager, const KeyManager &KM,
                     int leaf_max_size, int internal_max_size)
    : index_id_(index_id),
      buffer_pool_manager_(buffer_pool_manager),
      processor_(KM),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size) {
  // Get root page id from index roots page
  auto header_page = static_cast<IndexRootsPage *>(buffer_pool_manager_->FetchPage(INDEX_ROOTS_PAGE_ID));
  page_id_t root_page_id = INVALID_PAGE_ID;
  if (header_page != nullptr) {
    root_page_id = header_page->GetRootId(index_id);
    buffer_pool_manager_->UnpinPage(INDEX_ROOTS_PAGE_ID, false);
  }
  root_page_id_ = root_page_id;
}

void BPlusTree::Destroy(page_id_t current_page_id) {
  if (current_page_id == INVALID_PAGE_ID) {
    return;
  }

  auto page = buffer_pool_manager_->FetchPage(current_page_id);
  if (page == nullptr) {
    return;
  }

  auto tree_page = reinterpret_cast<BPlusTreePage *>(page->GetData());
  if (!tree_page->IsLeafPage()) {
    auto internal_page = reinterpret_cast<InternalPage *>(tree_page);
    for (int i = 0; i < internal_page->GetSize(); i++) {
      Destroy(internal_page->ValueAt(i));
    }
  }

  buffer_pool_manager_->UnpinPage(current_page_id, false);
  buffer_pool_manager_->DeletePage(current_page_id);
}

/*
 * Helper function to decide whether current b+tree is empty
 */
bool BPlusTree::IsEmpty() const {
  return root_page_id_ == INVALID_PAGE_ID;
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/**
 * TODO: Student Implement
 * @brief Find the value associated with the given key
 * @param key Key to search for
 * @param[out] result Vector of RowIds associated with key
 * @return true if key exists, false otherwise
 */
bool BPlusTree::GetValue(const GenericKey *key, std::vector<RowId> &result, Txn *transaction) {
  if (IsEmpty()) {
    return false;
  }

  auto leaf_page = FindLeafPage(key, root_page_id_, false);
  if (leaf_page == nullptr) {
    return false;
  }

  auto leaf = reinterpret_cast<LeafPage *>(leaf_page->GetData());
  bool found = false;
  int index = leaf->KeyIndex(key, processor_);
  if (index >= 0) {
    result.push_back(leaf->ValueAt(index));
    found = true;
  }

  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
  return found;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/**
 * TODO: Student Implement
 * @brief Insert a key-value pair into the B+ tree
 * @param key Key to insert
 * @param value Value to insert
 * @return true if insertion succeeded, false if duplicate key exists
 */
bool BPlusTree::Insert(GenericKey *key, const RowId &value, Txn *transaction) {
  if (IsEmpty()) {
    StartNewTree(key, value);
    return true;
  }
  return InsertIntoLeaf(key, value, transaction);
}

/**
 * TODO: Student Implement
 * @brief Create a new tree with first key-value pair
 */
void BPlusTree::StartNewTree(GenericKey *key, const RowId &value) {
  page_id_t new_page_id;
  auto page = buffer_pool_manager_->NewPage(new_page_id);
  if (page == nullptr) {
    throw std::runtime_error("Out of memory");
  }

  auto leaf = reinterpret_cast<LeafPage *>(page->GetData());
  leaf->Init(new_page_id, INVALID_PAGE_ID, leaf_max_size_);
  leaf->Insert(key, value, processor_);

  root_page_id_ = new_page_id;
  UpdateRootPageId(1);  // insert_record = 1 for insertion

  buffer_pool_manager_->UnpinPage(new_page_id, true);
}

/**
 * TODO: Student Implement
 * @brief Insert a key-value pair into a leaf node
 */
bool BPlusTree::InsertIntoLeaf(GenericKey *key, const RowId &value, Txn *transaction) {
  auto leaf_page = FindLeafPage(key, root_page_id_, false);
  if (leaf_page == nullptr) {
    return false;
  }

  auto leaf = reinterpret_cast<LeafPage *>(leaf_page->GetData());
  
  // Check for duplicates
  if (leaf->KeyIndex(key, processor_) >= 0) {
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
    return false;
  }

  // Insert into leaf
  if (leaf->GetSize() < leaf_max_size_) {
    leaf->Insert(key, value, processor_);
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), true);
    return true;
  }

  // Need to split
  auto new_leaf = Split(leaf, transaction);
  if (new_leaf == nullptr) {
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
    return false;
  }

  // Decide which page to insert into
  if (processor_.CompareKeys(key, new_leaf->KeyAt(0)) < 0) {
    leaf->Insert(key, value, processor_);
  } else {
    new_leaf->Insert(key, value, processor_);
  }

  // Insert new leaf's first key into parent
  InsertIntoParent(leaf, new_leaf->KeyAt(0), new_leaf, transaction);

  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), true);
  buffer_pool_manager_->UnpinPage(new_leaf->GetPageId(), true);
  return true;
}

/**
 * TODO: Student Implement
 * @brief Split an internal node
 */
BPlusTreeInternalPage *BPlusTree::Split(InternalPage *node, Txn *transaction) {
  page_id_t new_page_id;
  auto new_page = buffer_pool_manager_->NewPage(new_page_id);
  if (new_page == nullptr) {
    throw std::runtime_error("Out of memory");
  }

  auto new_node = reinterpret_cast<InternalPage *>(new_page->GetData());
  new_node->Init(new_page_id, node->GetParentPageId(), internal_max_size_);

  // Move half of entries to new node
  int total = node->GetSize();
  int move_count = total / 2;
  for (int i = move_count; i < total; i++) {
    new_node->Insert(node->KeyAt(i), node->ValueAt(i), processor_);
  }
  node->SetSize(move_count);

  // Update children's parent pointer
  for (int i = 0; i < new_node->GetSize(); i++) {
    auto child_page = buffer_pool_manager_->FetchPage(new_node->ValueAt(i));
    if (child_page != nullptr) {
      auto child = reinterpret_cast<BPlusTreePage *>(child_page->GetData());
      child->SetParentPageId(new_page_id);
      buffer_pool_manager_->UnpinPage(child_page->GetPageId(), true);
    }
  }

  return new_node;
}

/**
 * TODO: Student Implement
 * @brief Split a leaf node
 */
BPlusTreeLeafPage *BPlusTree::Split(LeafPage *node, Txn *transaction) {
  page_id_t new_page_id;
  auto new_page = buffer_pool_manager_->NewPage(new_page_id);
  if (new_page == nullptr) {
    throw std::runtime_error("Out of memory");
  }

  auto new_node = reinterpret_cast<LeafPage *>(new_page->GetData());
  new_node->Init(new_page_id, node->GetParentPageId(), leaf_max_size_);

  // Move half of entries to new node
  int total = node->GetSize();
  int move_count = total / 2;
  for (int i = move_count; i < total; i++) {
    new_node->Insert(node->KeyAt(i), node->ValueAt(i), processor_);
  }
  node->SetSize(move_count);

  // Update sibling pointers
  new_node->SetNextPageId(node->GetNextPageId());
  node->SetNextPageId(new_page_id);

  return new_node;
}

/**
 * TODO: Student Implement
 * @brief Insert a key-value pair into an internal node after split
 */
void BPlusTree::InsertIntoParent(BPlusTreePage *old_node, GenericKey *key, BPlusTreePage *new_node, Txn *transaction) {
  if (old_node->IsRootPage()) {
    // Create new root
    page_id_t new_root_id;
    auto new_root_page = buffer_pool_manager_->NewPage(new_root_id);
    if (new_root_page == nullptr) {
      throw std::runtime_error("Out of memory");
    }

    auto new_root = reinterpret_cast<InternalPage *>(new_root_page->GetData());
    new_root->Init(new_root_id, INVALID_PAGE_ID, internal_max_size_);
    new_root->Insert(old_node->KeyAt(0), old_node->GetPageId(), processor_);
    new_root->Insert(key, new_node->GetPageId(), processor_);

    old_node->SetParentPageId(new_root_id);
    new_node->SetParentPageId(new_root_id);

    root_page_id_ = new_root_id;
    UpdateRootPageId(1);

    buffer_pool_manager_->UnpinPage(new_root_id, true);
    return;
  }

  // Get parent page
  auto parent_page = buffer_pool_manager_->FetchPage(old_node->GetParentPageId());
  auto parent = reinterpret_cast<InternalPage *>(parent_page->GetData());

  if (parent->GetSize() < internal_max_size_) {
    // Parent has space, just insert
    parent->Insert(key, new_node->GetPageId(), processor_);
    new_node->SetParentPageId(parent->GetPageId());
    buffer_pool_manager_->UnpinPage(parent->GetPageId(), true);
    return;
  }

  // Parent is full, need to split
  auto new_parent = Split(parent, transaction);
  new_node->SetParentPageId(new_parent->GetPageId());

  // Decide which parent to insert into
  if (processor_.CompareKeys(key, new_parent->KeyAt(0)) < 0) {
    parent->Insert(key, new_node->GetPageId(), processor_);
  } else {
    new_parent->Insert(key, new_node->GetPageId(), processor_);
  }

  // Recursively insert into grandparent
  InsertIntoParent(parent, new_parent->KeyAt(0), new_parent, transaction);

  buffer_pool_manager_->UnpinPage(parent->GetPageId(), true);
  buffer_pool_manager_->UnpinPage(new_parent->GetPageId(), true);
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/**
 * TODO: Student Implement
 * @brief Remove a key-value pair from the B+ tree
 */
void BPlusTree::Remove(const GenericKey *key, Txn *transaction) {
  if (IsEmpty()) {
    return;
  }

  auto leaf_page = FindLeafPage(key, root_page_id_, false);
  if (leaf_page == nullptr) {
    return;
  }

  auto leaf = reinterpret_cast<LeafPage *>(leaf_page->GetData());
  int index = leaf->KeyIndex(key, processor_);
  if (index < 0) {
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
    return;
  }

  // Remove from leaf
  leaf->Remove(index);

  // Handle underflow
  if (leaf->GetSize() < leaf->GetMinSize()) {
    CoalesceOrRedistribute(leaf, transaction);
  }

  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), true);
}

/**
 * TODO: Student Implement
 * @brief Handle underflow after deletion
 */
template <typename N>
bool BPlusTree::CoalesceOrRedistribute(N *&node, Txn *transaction) {
  if (node->IsRootPage()) {
    return AdjustRoot(node);
  }

  if (node->GetSize() >= node->GetMinSize()) {
    return false;
  }

  auto parent_page = buffer_pool_manager_->FetchPage(node->GetParentPageId());
  auto parent = reinterpret_cast<InternalPage *>(parent_page->GetData());
  int node_index = parent->ValueIndex(node->GetPageId());

  // Try to borrow from siblings
  if (node_index > 0) {
    // Try left sibling
    auto left_sibling_page = buffer_pool_manager_->FetchPage(parent->ValueAt(node_index - 1));
    auto left_sibling = reinterpret_cast<N *>(left_sibling_page->GetData());
    if (left_sibling->GetSize() > left_sibling->GetMinSize()) {
      Redistribute(left_sibling, node, node_index);
      buffer_pool_manager_->UnpinPage(left_sibling_page->GetPageId(), true);
      buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
      return false;
    }
    buffer_pool_manager_->UnpinPage(left_sibling_page->GetPageId(), false);
  }

  if (node_index < parent->GetSize() - 1) {
    // Try right sibling
    auto right_sibling_page = buffer_pool_manager_->FetchPage(parent->ValueAt(node_index + 1));
    auto right_sibling = reinterpret_cast<N *>(right_sibling_page->GetData());
    if (right_sibling->GetSize() > right_sibling->GetMinSize()) {
      Redistribute(right_sibling, node, node_index);
      buffer_pool_manager_->UnpinPage(right_sibling_page->GetPageId(), true);
      buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
      return false;
    }
    buffer_pool_manager_->UnpinPage(right_sibling_page->GetPageId(), false);
  }

  // Need to merge
  if (node_index > 0) {
    // Merge with left sibling
    auto left_sibling_page = buffer_pool_manager_->FetchPage(parent->ValueAt(node_index - 1));
    auto left_sibling = reinterpret_cast<N *>(left_sibling_page->GetData());
    bool should_delete = Coalesce(left_sibling, node, parent, node_index, transaction);
    buffer_pool_manager_->UnpinPage(left_sibling_page->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
    return should_delete;
  } else {
    // Merge with right sibling
    auto right_sibling_page = buffer_pool_manager_->FetchPage(parent->ValueAt(node_index + 1));
    auto right_sibling = reinterpret_cast<N *>(right_sibling_page->GetData());
    bool should_delete = Coalesce(node, right_sibling, parent, node_index + 1, transaction);
    buffer_pool_manager_->UnpinPage(right_sibling_page->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
    return should_delete;
  }
}

/**
 * TODO: Student Implement
 * @brief Merge two leaf nodes
 */
bool BPlusTree::Coalesce(LeafPage *&neighbor_node, LeafPage *&node, InternalPage *&parent, int index,
                         Txn *transaction) {
  // Move all entries from node to neighbor
  for (int i = 0; i < node->GetSize(); i++) {
    neighbor_node->Insert(node->KeyAt(i), node->ValueAt(i), processor_);
  }

  // Update sibling pointers
  neighbor_node->SetNextPageId(node->GetNextPageId());

  // Remove entry from parent
  parent->Remove(index);

  // Delete node page
  buffer_pool_manager_->DeletePage(node->GetPageId());

  // Handle parent underflow
  if (parent->GetSize() < parent->GetMinSize()) {
    return CoalesceOrRedistribute(parent, transaction);
  }
  return false;
}

/**
 * TODO: Student Implement
 * @brief Merge two internal nodes
 */
bool BPlusTree::Coalesce(InternalPage *&neighbor_node, InternalPage *&node, InternalPage *&parent, int index,
                         Txn *transaction) {
  // Move all entries from node to neighbor
  neighbor_node->Insert(parent->KeyAt(index), node->ValueAt(0), processor_);
  for (int i = 1; i < node->GetSize(); i++) {
    neighbor_node->Insert(node->KeyAt(i), node->ValueAt(i), processor_);
  }

  // Update children's parent pointers
  for (int i = 0; i < node->GetSize(); i++) {
    auto child_page = buffer_pool_manager_->FetchPage(node->ValueAt(i));
    if (child_page != nullptr) {
      auto child = reinterpret_cast<BPlusTreePage *>(child_page->GetData());
      child->SetParentPageId(neighbor_node->GetPageId());
      buffer_pool_manager_->UnpinPage(child_page->GetPageId(), true);
    }
  }

  // Remove entry from parent
  parent->Remove(index);

  // Delete node page
  buffer_pool_manager_->DeletePage(node->GetPageId());

  // Handle parent underflow
  if (parent->GetSize() < parent->GetMinSize()) {
    return CoalesceOrRedistribute(parent, transaction);
  }
  return false;
}

/**
 * TODO: Student Implement
 * @brief Redistribute entries between two leaf nodes
 */
void BPlusTree::Redistribute(LeafPage *neighbor_node, LeafPage *node, int index) {
  if (index == 0) {
    // Move neighbor's first entry to node's end
    node->Insert(neighbor_node->KeyAt(0), neighbor_node->ValueAt(0), processor_);
    neighbor_node->Remove(0);
  } else {
    // Move neighbor's last entry to node's front
    int last_index = neighbor_node->GetSize() - 1;
    node->Insert(neighbor_node->KeyAt(last_index), neighbor_node->ValueAt(last_index), processor_);
    neighbor_node->Remove(last_index);
  }

  // Update parent key
  auto parent_page = buffer_pool_manager_->FetchPage(node->GetParentPageId());
  auto parent = reinterpret_cast<InternalPage *>(parent_page->GetData());
  if (index == 0) {
    parent->SetKeyAt(1, neighbor_node->KeyAt(0));
  } else {
    parent->SetKeyAt(index, node->KeyAt(0));
  }
  buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
}

/**
 * TODO: Student Implement
 * @brief Redistribute entries between two internal nodes
 */
void BPlusTree::Redistribute(InternalPage *neighbor_node, InternalPage *node, int index) {
  auto parent_page = buffer_pool_manager_->FetchPage(node->GetParentPageId());
  auto parent = reinterpret_cast<InternalPage *>(parent_page->GetData());

  if (index == 0) {
    // Move neighbor's first entry to node's end
    node->Insert(parent->KeyAt(1), neighbor_node->ValueAt(0), processor_);
    parent->SetKeyAt(1, neighbor_node->KeyAt(1));
    neighbor_node->Remove(0);

    // Update child's parent pointer
    auto child_page = buffer_pool_manager_->FetchPage(node->ValueAt(node->GetSize() - 1));
    if (child_page != nullptr) {
      auto child = reinterpret_cast<BPlusTreePage *>(child_page->GetData());
      child->SetParentPageId(node->GetPageId());
      buffer_pool_manager_->UnpinPage(child_page->GetPageId(), true);
    }
  } else {
    // Move neighbor's last entry to node's front
    int last_index = neighbor_node->GetSize() - 1;
    node->Insert(parent->KeyAt(index), neighbor_node->ValueAt(last_index), processor_);
    parent->SetKeyAt(index, neighbor_node->KeyAt(last_index));
    neighbor_node->Remove(last_index);

    // Update child's parent pointer
    auto child_page = buffer_pool_manager_->FetchPage(node->ValueAt(0));
    if (child_page != nullptr) {
      auto child = reinterpret_cast<BPlusTreePage *>(child_page->GetData());
      child->SetParentPageId(node->GetPageId());
      buffer_pool_manager_->UnpinPage(child_page->GetPageId(), true);
    }
  }

  buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
}

/**
 * TODO: Student Implement
 * @brief Update root page if necessary
 */
bool BPlusTree::AdjustRoot(BPlusTreePage *old_root_node) {
  if (old_root_node->IsLeafPage()) {
    if (old_root_node->GetSize() == 0) {
      // Tree becomes empty
      root_page_id_ = INVALID_PAGE_ID;
      UpdateRootPageId(0);
      buffer_pool_manager_->DeletePage(old_root_node->GetPageId());
      return true;
    }
    return false;
  }

  // Internal node
  if (old_root_node->GetSize() == 1) {
    // Only one child, make it new root
    auto internal_node = reinterpret_cast<InternalPage *>(old_root_node);
    root_page_id_ = internal_node->ValueAt(0);
    UpdateRootPageId(0);

    // Update new root's parent
    auto new_root_page = buffer_pool_manager_->FetchPage(root_page_id_);
    if (new_root_page != nullptr) {
      auto new_root = reinterpret_cast<BPlusTreePage *>(new_root_page->GetData());
      new_root->SetParentPageId(INVALID_PAGE_ID);
      buffer_pool_manager_->UnpinPage(root_page_id_, true);
    }

    buffer_pool_manager_->DeletePage(old_root_node->GetPageId());
    return true;
  }
  return false;
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/**
 * TODO: Student Implement
 * @brief Create iterator for the B+ tree
 */
IndexIterator BPlusTree::Begin() {
  if (IsEmpty()) {
    return End();
  }

  auto page = FindLeafPage(nullptr, root_page_id_, true);
  auto leaf = reinterpret_cast<LeafPage *>(page->GetData());
  buffer_pool_manager_->UnpinPage(page->GetPageId(), false);

  return IndexIterator(this, leaf->GetPageId(), 0);
}

/**
 * TODO: Student Implement
 * @brief Create iterator starting from a key
 */
IndexIterator BPlusTree::Begin(const GenericKey *key) {
  if (IsEmpty()) {
    return End();
  }

  auto page = FindLeafPage(key, root_page_id_, false);
  if (page == nullptr) {
    return End();
  }

  auto leaf = reinterpret_cast<LeafPage *>(page->GetData());
  int index = leaf->KeyIndex(key, processor_);
  if (index < 0) {
    index = 0;  // Start from first entry if key not found
  }

  buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
  return IndexIterator(this, leaf->GetPageId(), index);
}

/**
 * TODO: Student Implement
 * @brief Create end iterator
 */
IndexIterator BPlusTree::End() {
  return IndexIterator(this, INVALID_PAGE_ID, 0);
}

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/**
 * TODO: Student Implement
 * @brief Find leaf page containing particular key
 */
Page *BPlusTree::FindLeafPage(const GenericKey *key, page_id_t page_id, bool leftMost) {
  auto page = buffer_pool_manager_->FetchPage(page_id);
  if (page == nullptr) {
    return nullptr;
  }

  auto tree_page = reinterpret_cast<BPlusTreePage *>(page->GetData());
  while (!tree_page->IsLeafPage()) {
    auto internal_page = reinterpret_cast<InternalPage *>(tree_page);
    page_id_t child_page_id;

    if (leftMost) {
      child_page_id = internal_page->ValueAt(0);
    } else {
      child_page_id = internal_page->Lookup(key, processor_);
    }

    buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
    page = buffer_pool_manager_->FetchPage(child_page_id);
    if (page == nullptr) {
      return nullptr;
    }
    tree_page = reinterpret_cast<BPlusTreePage *>(page->GetData());
  }

  return page;
}

/**
 * TODO: Student Implement
 * @brief Update root page ID in header page
 */
void BPlusTree::UpdateRootPageId(int insert_record) {
  auto header_page = static_cast<IndexRootsPage *>(buffer_pool_manager_->FetchPage(INDEX_ROOTS_PAGE_ID));
  if (header_page != nullptr) {
    header_page->Update(index_id_, root_page_id_);
    buffer_pool_manager_->UnpinPage(INDEX_ROOTS_PAGE_ID, true);
  }
}

/**
 * This method is used for debug only, You don't need to modify
 */
void BPlusTree::ToGraph(BPlusTreePage *page, BufferPoolManager *bpm, std::ofstream &out, Schema *schema) const {
  std::string leaf_prefix("LEAF_");
  std::string internal_prefix("INT_");
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    // Print node name
    out << leaf_prefix << leaf->GetPageId();
    // Print node properties
    out << "[shape=plain color=green ";
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">P=" << leaf->GetPageId()
        << ",Parent=" << leaf->GetParentPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
        << "max_size=" << leaf->GetMaxSize() << ",min_size=" << leaf->GetMinSize() << ",size=" << leaf->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < leaf->GetSize(); i++) {
      Row ans;
      processor_.DeserializeToKey(leaf->KeyAt(i), ans, schema);
      out << "<TD>" << ans.GetField(0)->toString() << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Leaf node link if there is a next page
    if (leaf->GetNextPageId() != INVALID_PAGE_ID) {
      out << leaf_prefix << leaf->GetPageId() << " -> " << leaf_prefix << leaf->GetNextPageId() << ";\n";
      out << "{rank=same " << leaf_prefix << leaf->GetPageId() << " " << leaf_prefix << leaf->GetNextPageId() << "};\n";
    }

    // Print parent links if there is a parent
    if (leaf->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << leaf->GetParentPageId() << ":p" << leaf->GetPageId() << " -> " << leaf_prefix
          << leaf->GetPageId() << ";\n";
    }
  } else {
    auto *inner = reinterpret_cast<InternalPage *>(page);
    // Print node name
    out << internal_prefix << inner->GetPageId();
    // Print node properties
    out << "[shape=plain color=pink ";  // why not?
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">P=" << inner->GetPageId()
        << ",Parent=" << inner->GetParentPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">"
        << "max_size=" << inner->GetMaxSize() << ",min_size=" << inner->GetMinSize() << ",size=" << inner->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < inner->GetSize(); i++) {
      out << "<TD PORT=\"p" << inner->ValueAt(i) << "\">";
      if (i > 0) {
        Row ans;
        processor_.DeserializeToKey(inner->KeyAt(i), ans, schema);
        out << ans.GetField(0)->toString();
      } else {
        out << " ";
      }
      out << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Parent link
    if (inner->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << inner->GetParentPageId() << ":p" << inner->GetPageId() << " -> " << internal_prefix
          << inner->GetPageId() << ";\n";
    }
    // Print leaves
    for (int i = 0; i < inner->GetSize(); i++) {
      auto child_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i))->GetData());
      ToGraph(child_page, bpm, out, schema);
      if (i > 0) {
        auto sibling_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i - 1))->GetData());
        if (!sibling_page->IsLeafPage() && !child_page->IsLeafPage()) {
          out << "{rank=same " << internal_prefix << sibling_page->GetPageId() << " " << internal_prefix
              << child_page->GetPageId() << "};\n";
        }
        bpm->UnpinPage(sibling_page->GetPageId(), false);
      }
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

/**
 * This function is for debug only, you don't need to modify
 */
void BPlusTree::ToString(BPlusTreePage *page, BufferPoolManager *bpm) const {
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    std::cout << "Leaf Page: " << leaf->GetPageId() << " parent: " << leaf->GetParentPageId()
              << " next: " << leaf->GetNextPageId() << std::endl;
    for (int i = 0; i < leaf->GetSize(); i++) {
      std::cout << leaf->KeyAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
  } else {
    auto *internal = reinterpret_cast<InternalPage *>(page);
    std::cout << "Internal Page: " << internal->GetPageId() << " parent: " << internal->GetParentPageId() << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      std::cout << internal->KeyAt(i) << ": " << internal->ValueAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(internal->ValueAt(i))->GetData()), bpm);
      bpm->UnpinPage(internal->ValueAt(i), false);
    }
  }
}

bool BPlusTree::Check() {
  bool all_unpinned = buffer_pool_manager_->CheckAllUnpinned();
  if (!all_unpinned) {
    LOG(ERROR) << "problem in page unpin" << endl;
  }
  return all_unpinned;
}