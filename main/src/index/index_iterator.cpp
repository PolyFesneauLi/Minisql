#include "index/index_iterator.h"

#include "index/basic_comparator.h"
#include "index/generic_key.h"

IndexIterator::IndexIterator() = default;

IndexIterator::IndexIterator(page_id_t page_id, BufferPoolManager *bpm, int index)
    : current_page_id(page_id), item_index(index), buffer_pool_manager(bpm) {
  if (current_page_id != INVALID_PAGE_ID) {
    page = reinterpret_cast<LeafPage *>(buffer_pool_manager->FetchPage(current_page_id)->GetData());
  }
}

IndexIterator::~IndexIterator() {
  if (current_page_id != INVALID_PAGE_ID) {
    buffer_pool_manager->UnpinPage(current_page_id, false);
  }
}

/**
 * TODO: Student Implement
 */
std::pair<GenericKey *, RowId> IndexIterator::operator*() {
  ASSERT(current_page_id != INVALID_PAGE_ID && page != nullptr, "Invalid page");
  ASSERT(item_index >= 0 && item_index < page->GetSize(), "Invalid index");
  return {page->KeyAt(item_index), page->ValueAt(item_index)};
}

/**
 * TODO: Student Implement
 */
IndexIterator &IndexIterator::operator++() {
  ASSERT(current_page_id != INVALID_PAGE_ID && page != nullptr, "Invalid page");
  
  // Move to next item in current page
  item_index++;
  
  // If reached end of current page, move to next page
  if (item_index >= page->GetSize()) {
    page_id_t next_page_id = page->GetNextPageId();
    
    // Unpin current page
    buffer_pool_manager->UnpinPage(current_page_id, false);
    
    if (next_page_id == INVALID_PAGE_ID) {
      // No more pages, set to end iterator
      current_page_id = INVALID_PAGE_ID;
      page = nullptr;
      item_index = 0;
    } else {
      // Move to next page
      current_page_id = next_page_id;
      page = reinterpret_cast<LeafPage *>(buffer_pool_manager->FetchPage(current_page_id)->GetData());
      item_index = 0;
    }
  }
  
  return *this;
}

bool IndexIterator::operator==(const IndexIterator &itr) const {
  return current_page_id == itr.current_page_id && item_index == itr.item_index;
}

bool IndexIterator::operator!=(const IndexIterator &itr) const {
  return !(*this == itr);
}