#include "buffer/buffer_pool_manager.h"

#include "glog/logging.h"
#include "page/bitmap_page.h"

static const char EMPTY_PAGE_DATA[PAGE_SIZE] = {0};

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager)
    : pool_size_(pool_size), disk_manager_(disk_manager) {
  pages_ = new Page[pool_size_];
  replacer_ = new LRUReplacer(pool_size_);
  for (size_t i = 0; i < pool_size_; i++) {
    free_list_.emplace_back(i);
  }
}

BufferPoolManager::~BufferPoolManager() {
  for (auto page : page_table_) {
    FlushPage(page.first);
  }
  delete[] pages_;
  delete replacer_;
}

/**
 * TODO: Student Implement
 * @brief Fetch the requested page from buffer pool
 * @param page_id The page ID to fetch
 * @return Pointer to the requested page
 */
Page *BufferPoolManager::FetchPage(page_id_t page_id) {
  std::lock_guard<std::mutex> lock();

  // 1. Search the page table for the requested page
  auto it = page_table_.find(page_id);
  if (it != page_table_.end()) {
    // Page exists in buffer pool
    frame_id_t frame_id = it->second;
    Page *page = &pages_[frame_id];
    page->pin_count_++;
    replacer_->Pin(frame_id);
    return page;
  }

  // 2. Page not in buffer pool, find a replacement frame
  frame_id_t frame_id;
  if (!free_list_.empty()) {
    // Use frame from free list
    frame_id = free_list_.front();
    free_list_.pop_front();
  } else {
    // Try to get victim from replacer
    if (!replacer_->Victim(&frame_id)) {
      return nullptr;  // No free frames available
    }
    
    // If victim page is dirty, write it back
    Page *victim_page = &pages_[frame_id];
    if (victim_page->is_dirty_) {
      disk_manager_->WritePage(victim_page->GetPageId(), victim_page->GetData());
    }
    
    // Remove victim from page table
    page_table_.erase(victim_page->GetPageId());
  }

  // 3. Initialize the new page
  Page *page = &pages_[frame_id];
  page->page_id_ = page_id;
  page->pin_count_ = 1;
  page->is_dirty_ = false;
  
  // Read page content from disk
  disk_manager_->ReadPage(page_id, page->data_);
  
  // Add to page table and pin in replacer
  page_table_[page_id] = frame_id;
  replacer_->Pin(frame_id);
  
  return page;
}

/**
 * TODO: Student Implement
 * @brief Create a new page in buffer pool
 * @param[out] page_id ID of the new page
 * @return Pointer to the new page
 */
Page *BufferPoolManager::NewPage(page_id_t &page_id) {
  std::lock_guard<std::mutex> lock();

  // 1. Check if buffer pool has free frame
  frame_id_t frame_id;
  if (!free_list_.empty()) {
    frame_id = free_list_.front();
    free_list_.pop_front();
  } else {
    if (!replacer_->Victim(&frame_id)) {
      return nullptr;  // No free frames
    }
    
    // Write dirty victim page to disk
    Page *victim_page = &pages_[frame_id];
    if (victim_page->is_dirty_) {
      disk_manager_->WritePage(victim_page->GetPageId(), victim_page->GetData());
    }
    
    // Remove from page table
    page_table_.erase(victim_page->GetPageId());
  }

  // 2. Allocate new page from disk
  page_id = AllocatePage();
  
  // 3. Initialize the new page
  Page *page = &pages_[frame_id];
  page->page_id_ = page_id;
  page->pin_count_ = 1;
  page->is_dirty_ = false;
  memcpy(page->data_, EMPTY_PAGE_DATA, PAGE_SIZE);
  
  // Add to page table and pin in replacer
  page_table_[page_id] = frame_id;
  replacer_->Pin(frame_id);
  
  return page;
}

/**
 * TODO: Student Implement
 * @brief Delete a page from buffer pool
 * @param page_id ID of the page to delete
 * @return True if deletion succeeded, false otherwise
 */
bool BufferPoolManager::DeletePage(page_id_t page_id) {
  std::lock_guard<std::mutex> lock();

  // 1. Search page table
  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    // Page not in buffer pool, deallocate from disk
    DeallocatePage(page_id);
    return true;
  }

  // 2. If page is pinned, cannot delete
  frame_id_t frame_id = it->second;
  Page *page = &pages_[frame_id];
  if (page->pin_count_ > 0) {
    return false;
  }

  // 3. Remove page from buffer pool
  if (page->is_dirty_) {
    disk_manager_->WritePage(page_id, page->GetData());
  }
  
  // Reset page metadata
  page->is_dirty_ = false;
  page->pin_count_ = 0;
  page->page_id_ = INVALID_PAGE_ID;
  
  // Remove from page table and add frame to free list
  page_table_.erase(it);
  free_list_.push_back(frame_id);
  
  // Deallocate from disk
  DeallocatePage(page_id);
  
  return true;
}

/**
 * TODO: Student Implement
 * @brief Unpin a page from buffer pool
 * @param page_id ID of the page to unpin
 * @param is_dirty Whether the page is dirty
 * @return True if unpin succeeded, false otherwise
 */
bool BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty) {
  std::lock_guard<std::mutex> lock();

  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    return false;
  }

  frame_id_t frame_id = it->second;
  Page *page = &pages_[frame_id];
  
  if (page->pin_count_ <= 0) {
    return false;
  }

  page->pin_count_--;
  if (is_dirty) {
    page->is_dirty_ = true;
  }

  if (page->pin_count_ == 0) {
    replacer_->Unpin(frame_id);
  }

  return true;
}

/**
 * TODO: Student Implement
 * @brief Flush a page to disk
 * @param page_id ID of the page to flush
 * @return True if flush succeeded, false otherwise
 */
bool BufferPoolManager::FlushPage(page_id_t page_id) {
  std::lock_guard<std::mutex> lock();

  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    return false;
  }

  frame_id_t frame_id = it->second;
  Page *page = &pages_[frame_id];
  
  // Write page to disk
  disk_manager_->WritePage(page_id, page->GetData());
  page->is_dirty_ = false;
  
  return true;
}

page_id_t BufferPoolManager::AllocatePage() {
  int next_page_id = disk_manager_->AllocatePage();
  return next_page_id;
}

void BufferPoolManager::DeallocatePage(__attribute__((unused)) page_id_t page_id) {
  disk_manager_->DeAllocatePage(page_id);
}

bool BufferPoolManager::IsPageFree(page_id_t page_id) {
  return disk_manager_->IsPageFree(page_id);
}

// Only used for debug
bool BufferPoolManager::CheckAllUnpinned() {
  bool res = true;
  for (size_t i = 0; i < pool_size_; i++) {
    if (pages_[i].pin_count_ != 0) {
      res = false;
      LOG(ERROR) << "page " << pages_[i].page_id_ << " pin count:" << pages_[i].pin_count_ << endl;
    }
  }
  return res;
}