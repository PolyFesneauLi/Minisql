#include "page/b_plus_tree_internal_page.h"

#include "index/generic_key.h"

#define pairs_off (data_)
#define pair_size (GetKeySize() + sizeof(page_id_t))
#define key_off 0
#define val_off GetKeySize()

/**
 * TODO: Student Implement
 * @brief Initialize a new internal page
 */
void InternalPage::Init(page_id_t page_id, page_id_t parent_id, int key_size, int max_size) {
  // Set page type
  SetPageType(IndexPageType::INTERNAL_PAGE);
  
  // Set page metadata
  SetPageId(page_id);
  SetParentPageId(parent_id);
  SetKeySize(key_size);
  SetMaxSize(max_size);
  SetSize(0);
}

/**
 * Helper methods to access/modify key/value pairs
 */
GenericKey *InternalPage::KeyAt(int index) {
  return reinterpret_cast<GenericKey *>(pairs_off + index * pair_size + key_off);
}

void InternalPage::SetKeyAt(int index, GenericKey *key) {
  memcpy(pairs_off + index * pair_size + key_off, key, GetKeySize());
}

page_id_t InternalPage::ValueAt(int index) const {
  return *reinterpret_cast<const page_id_t *>(pairs_off + index * pair_size + val_off);
}

void InternalPage::SetValueAt(int index, page_id_t value) {
  *reinterpret_cast<page_id_t *>(pairs_off + index * pair_size + val_off) = value;
}

int InternalPage::ValueIndex(const page_id_t &value) const {
  for (int i = 0; i < GetSize(); ++i) {
    if (ValueAt(i) == value)
      return i;
  }
  return -1;
}

void *InternalPage::PairPtrAt(int index) {
  return KeyAt(index);
}

void InternalPage::PairCopy(void *dest, void *src, int pair_num) {
  memcpy(dest, src, pair_num * (GetKeySize() + sizeof(page_id_t)));
}

/**
 * TODO: Student Implement
 * @brief Find child page id containing input key
 */
page_id_t InternalPage::Lookup(const GenericKey *key, const KeyManager &KM) {
  // Binary search for the key
  int left = 1;  // Start from 1 since first key is invalid
  int right = GetSize() - 1;

  while (left <= right) {
    int mid = (left + right) / 2;
    int cmp = KM.CompareKeys(KeyAt(mid), key);
    
    if (cmp == 0) {
      return ValueAt(mid);  // Found exact match
    } else if (cmp < 0) {
      left = mid + 1;  // Search right half
    } else {
      right = mid - 1;  // Search left half
    }
  }

  // Return the child pointer before insertion point
  return ValueAt(left - 1);
}

/**
 * TODO: Student Implement
 * @brief Create new root page with given values
 */
void InternalPage::PopulateNewRoot(const page_id_t &old_value, GenericKey *new_key, const page_id_t &new_value) {
  // Insert old_value at index 0 with invalid key
  SetValueAt(0, old_value);
  
  // Insert new key-value pair at index 1
  SetKeyAt(1, new_key);
  SetValueAt(1, new_value);
  
  SetSize(2);
}

/**
 * TODO: Student Implement
 * @brief Insert new key-value pair right after the pair with old_value
 */
int InternalPage::InsertNodeAfter(const page_id_t &old_value, GenericKey *new_key, const page_id_t &new_value) {
  if (GetSize() >= GetMaxSize()) {
    return GetSize();  // Page is full
  }

  int index = ValueIndex(old_value);
  if (index == -1) {
    return GetSize();  // Old value not found
  }

  // Make room for new pair
  index++;  // Insert after old value
  if (index < GetSize()) {
    memmove(PairPtrAt(index + 1), PairPtrAt(index), (GetSize() - index) * pair_size);
  }

  // Insert new pair
  SetKeyAt(index, new_key);
  SetValueAt(index, new_value);
  IncreaseSize(1);

  return GetSize();
}

/**
 * TODO: Student Implement
 * @brief Move half of key-value pairs into recipient page
 */
void InternalPage::MoveHalfTo(InternalPage *recipient, BufferPoolManager *buffer_pool_manager) {
  int size = GetSize();
  int move_size = size / 2;
  int start_index = size - move_size;

  // Copy pairs to recipient
  recipient->CopyNFrom(PairPtrAt(start_index), move_size, buffer_pool_manager);
  
  // Update size
  SetSize(size - move_size);
}

/**
 * TODO: Student Implement
 * @brief Copy entries into page and update parent page id
 */
void InternalPage::CopyNFrom(void *src, int size, BufferPoolManager *buffer_pool_manager) {
  // Copy pairs
  memcpy(PairPtrAt(GetSize()), src, size * pair_size);
  
  // Update parent page id for all children
  for (int i = 0; i < size; i++) {
    page_id_t child_page_id = ValueAt(GetSize() + i);
    auto child_page = buffer_pool_manager->FetchPage(child_page_id);
    if (child_page != nullptr) {
      auto child = reinterpret_cast<BPlusTreePage *>(child_page->GetData());
      child->SetParentPageId(GetPageId());
      buffer_pool_manager->UnpinPage(child_page_id, true);
    }
  }

  IncreaseSize(size);
}

/**
 * TODO: Student Implement
 * @brief Remove key-value pair at index
 */
void InternalPage::Remove(int index) {
  if (index >= GetSize()) {
    return;
  }

  // Move pairs to close the gap
  if (index < GetSize() - 1) {
    memmove(PairPtrAt(index), PairPtrAt(index + 1), (GetSize() - index - 1) * pair_size);
  }

  IncreaseSize(-1);
}

/**
 * TODO: Student Implement
 * @brief Remove the only key-value pair and return the value
 */
page_id_t InternalPage::RemoveAndReturnOnlyChild() {
  page_id_t value = ValueAt(0);
  SetSize(0);
  return value;
}

/**
 * TODO: Student Implement
 * @brief Move all pairs into recipient page
 */
void InternalPage::MoveAllTo(InternalPage *recipient, GenericKey *middle_key, BufferPoolManager *buffer_pool_manager) {
  // Insert middle key with first value
  recipient->CopyLastFrom(middle_key, ValueAt(0), buffer_pool_manager);

  // Copy remaining pairs
  recipient->CopyNFrom(PairPtrAt(1), GetSize() - 1, buffer_pool_manager);

  SetSize(0);
}

/**
 * TODO: Student Implement
 * @brief Move first pair to end of recipient page
 */
void InternalPage::MoveFirstToEndOf(InternalPage *recipient, GenericKey *middle_key,
                                  BufferPoolManager *buffer_pool_manager) {
  recipient->CopyLastFrom(middle_key, ValueAt(0), buffer_pool_manager);
  
  // Remove from this page
  memmove(PairPtrAt(0), PairPtrAt(1), (GetSize() - 1) * pair_size);
  IncreaseSize(-1);
}

/**
 * TODO: Student Implement
 * @brief Append an entry at the end
 */
void InternalPage::CopyLastFrom(GenericKey *key, const page_id_t value, BufferPoolManager *buffer_pool_manager) {
  SetKeyAt(GetSize(), key);
  SetValueAt(GetSize(), value);

  // Update child's parent page id
  auto child_page = buffer_pool_manager->FetchPage(value);
  if (child_page != nullptr) {
    auto child = reinterpret_cast<BPlusTreePage *>(child_page->GetData());
    child->SetParentPageId(GetPageId());
    buffer_pool_manager->UnpinPage(value, true);
  }

  IncreaseSize(1);
}

/**
 * TODO: Student Implement
 * @brief Move last pair to front of recipient page
 */
void InternalPage::MoveLastToFrontOf(InternalPage *recipient, GenericKey *middle_key,
                                   BufferPoolManager *buffer_pool_manager) {
  // Move pairs in recipient to make room
  memmove(recipient->PairPtrAt(1), recipient->PairPtrAt(0), recipient->GetSize() * pair_size);

  // Insert last pair at front with middle key
  recipient->SetKeyAt(0, middle_key);
  recipient->SetValueAt(0, ValueAt(GetSize() - 1));

  // Update child's parent page id
  auto child_page = buffer_pool_manager->FetchPage(ValueAt(GetSize() - 1));
  if (child_page != nullptr) {
    auto child = reinterpret_cast<BPlusTreePage *>(child_page->GetData());
    child->SetParentPageId(recipient->GetPageId());
    buffer_pool_manager->UnpinPage(child_page->GetPageId(), true);
  }

  recipient->IncreaseSize(1);
  IncreaseSize(-1);
}

/**
 * TODO: Student Implement
 * @brief Insert an entry at the beginning with given value
 */
void InternalPage::CopyFirstFrom(const page_id_t value, BufferPoolManager *buffer_pool_manager) {
  memmove(PairPtrAt(1), PairPtrAt(0), GetSize() * pair_size);
  SetValueAt(0, value);

  // Update child's parent page id
  auto child_page = buffer_pool_manager->FetchPage(value);
  if (child_page != nullptr) {
    auto child = reinterpret_cast<BPlusTreePage *>(child_page->GetData());
    child->SetParentPageId(GetPageId());
    buffer_pool_manager->UnpinPage(value, true);
  }

  IncreaseSize(1);
}