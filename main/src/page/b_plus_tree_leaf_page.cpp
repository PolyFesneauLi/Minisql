#include "page/b_plus_tree_leaf_page.h"

#include <algorithm>

#include "index/generic_key.h"

#define pairs_off (data_)
#define pair_size (GetKeySize() + sizeof(RowId))
#define key_off 0
#define val_off GetKeySize()
/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/

/**
 * TODO: Student Implement
 * @brief Initialize a new leaf page
 */
void LeafPage::Init(page_id_t page_id, page_id_t parent_id, int key_size, int max_size) {
  // Set page type
  SetPageType(IndexPageType::LEAF_PAGE);
  
  // Set page metadata
  SetPageId(page_id);
  SetParentPageId(parent_id);
  SetKeySize(key_size);
  SetMaxSize(max_size);
  SetSize(0);
  
  // Initialize next page id
  SetNextPageId(INVALID_PAGE_ID);
}

/**
 * Helper methods to set/get next page id
 */
page_id_t LeafPage::GetNextPageId() const {
  return next_page_id_;
}

void LeafPage::SetNextPageId(page_id_t next_page_id) {
  next_page_id_ = next_page_id;
  if (next_page_id == 0) {
    LOG(INFO) << "Fatal error";
  }
}

/**
 * TODO: Student Implement
 * @brief Find the first index i so that pairs_[i].first >= key
 */
int LeafPage::KeyIndex(const GenericKey *key, const KeyManager &KM) {
  // Binary search for the key
  int left = 0;
  int right = GetSize() - 1;

  while (left <= right) {
    int mid = (left + right) / 2;
    int cmp = KM.CompareKeys(KeyAt(mid), key);
    
    if (cmp == 0) {
      return mid;  // Found exact match
    } else if (cmp < 0) {
      left = mid + 1;  // Search right half
    } else {
      right = mid - 1;  // Search left half
    }
  }

  return left;  // Return insertion point
}

/*
 * Helper method to find and return the key associated with input "index"(a.k.a
 * array offset)
 */
GenericKey *LeafPage::KeyAt(int index) {
  return reinterpret_cast<GenericKey *>(pairs_off + index * pair_size + key_off);
}

void LeafPage::SetKeyAt(int index, GenericKey *key) {
  memcpy(pairs_off + index * pair_size + key_off, key, GetKeySize());
}

RowId LeafPage::ValueAt(int index) const {
  return *reinterpret_cast<const RowId *>(pairs_off + index * pair_size + val_off);
}

void LeafPage::SetValueAt(int index, RowId value) {
  *reinterpret_cast<RowId *>(pairs_off + index * pair_size + val_off) = value;
}

void *LeafPage::PairPtrAt(int index) {
  return KeyAt(index);
}

void LeafPage::PairCopy(void *dest, void *src, int pair_num) {
  memcpy(dest, src, pair_num * (GetKeySize() + sizeof(RowId)));
}
/*
 * Helper method to find and return the key & value pair associated with input
 * "index"(a.k.a. array offset)
 */
std::pair<GenericKey *, RowId> LeafPage::GetItem(int index) {
  return {KeyAt(index), ValueAt(index)};
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/**
 * TODO: Student Implement
 * @brief Insert key & value pair into leaf page ordered by key
 */
int LeafPage::Insert(GenericKey *key, const RowId &value, const KeyManager &KM) {
  if (GetSize() >= GetMaxSize()) {
    return GetSize();  // Page is full
  }

  // Find insertion point
  int index = KeyIndex(key, KM);
  
  // Check for duplicate key
  if (index < GetSize() && KM.CompareKeys(KeyAt(index), key) == 0) {
    return GetSize();  // Duplicate key
  }

  // Make room for new pair
  if (index < GetSize()) {
    memmove(PairPtrAt(index + 1), PairPtrAt(index), (GetSize() - index) * pair_size);
  }

  // Insert new pair
  SetKeyAt(index, key);
  SetValueAt(index, value);
  IncreaseSize(1);

  return GetSize();
}

/*****************************************************************************
 * SPLIT
 *****************************************************************************/
/**
 * TODO: Student Implement
 * @brief Split half of key-value pairs into recipient page
 */
void LeafPage::MoveHalfTo(LeafPage *recipient) {
  int size = GetSize();
  int move_size = size / 2;
  int start_index = size - move_size;

  // Copy pairs to recipient
  recipient->CopyNFrom(PairPtrAt(start_index), move_size);
  
  // Update size
  SetSize(size - move_size);
}

/**
 * TODO: Student Implement
 * @brief Copy entries into page
 */
void LeafPage::CopyNFrom(void *src, int size) {
  memcpy(PairPtrAt(GetSize()), src, size * pair_size);
  IncreaseSize(size);
}

/*****************************************************************************
 * LOOKUP
 *****************************************************************************/
/**
 * TODO: Student Implement
 * @brief Lookup a key and store its corresponding value
 */
bool LeafPage::Lookup(const GenericKey *key, RowId &value, const KeyManager &KM) {
  int index = KeyIndex(key, KM);
  if (index < GetSize() && KM.CompareKeys(KeyAt(index), key) == 0) {
    value = ValueAt(index);
    return true;
  }
  return false;
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/**
 * TODO: Student Implement
 * @brief Remove the key-value pair at given index
 */
int LeafPage::RemoveAndDeleteRecord(const GenericKey *key, const KeyManager &KM) {
  int index = KeyIndex(key, KM);
  if (index >= GetSize() || KM.CompareKeys(KeyAt(index), key) != 0) {
    return GetSize();  // Key not found
  }

  // Move pairs to close the gap
  if (index < GetSize() - 1) {
    memmove(PairPtrAt(index), PairPtrAt(index + 1), (GetSize() - index - 1) * pair_size);
  }

  IncreaseSize(-1);
  return GetSize();
}

/*****************************************************************************
 * MERGE
 *****************************************************************************/
/**
 * TODO: Student Implement
 * @brief Move all pairs to recipient page
 */
void LeafPage::MoveAllTo(LeafPage *recipient) {
  recipient->CopyNFrom(PairPtrAt(0), GetSize());
  recipient->SetNextPageId(GetNextPageId());
  SetSize(0);
}

/*****************************************************************************
 * REDISTRIBUTE
 *****************************************************************************/
/**
 * TODO: Student Implement
 * @brief Move first pair to end of recipient page
 */
void LeafPage::MoveFirstToEndOf(LeafPage *recipient) {
  recipient->CopyLastFrom(KeyAt(0), ValueAt(0));
  
  // Remove from this page
  memmove(PairPtrAt(0), PairPtrAt(1), (GetSize() - 1) * pair_size);
  IncreaseSize(-1);
}

/**
 * TODO: Student Implement
 * @brief Append an entry at the end
 */
void LeafPage::CopyLastFrom(GenericKey *key, const RowId value) {
  SetKeyAt(GetSize(), key);
  SetValueAt(GetSize(), value);
  IncreaseSize(1);
}

/**
 * TODO: Student Implement
 * @brief Move last pair to front of recipient page
 */
void LeafPage::MoveLastToFrontOf(LeafPage *recipient) {
  recipient->CopyFirstFrom(KeyAt(GetSize() - 1), ValueAt(GetSize() - 1));
  IncreaseSize(-1);
}

/**
 * TODO: Student Implement
 * @brief Insert an entry at the beginning
 */
void LeafPage::CopyFirstFrom(GenericKey *key, const RowId value) {
  memmove(PairPtrAt(1), PairPtrAt(0), GetSize() * pair_size);
  SetKeyAt(0, key);
  SetValueAt(0, value);
  IncreaseSize(1);
}