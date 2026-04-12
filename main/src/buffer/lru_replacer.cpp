#include "buffer/lru_replacer.h"

LRUReplacer::LRUReplacer(size_t num_pages) {
  // Initialize max capacity
  max_size_ = num_pages;
}

LRUReplacer::~LRUReplacer() = default;

/**
 * TODO: Student Implement
 * @brief Remove the object that was accessed the least recently compared to all the elements being tracked
 * @param[out] frame_id The frame ID that is removed
 * @return true if successfully removed the frame, false if frame not found
 * @note This method should remove and return the frame that was accessed least recently compared to all other frames
 */
bool LRUReplacer::Victim(frame_id_t *frame_id) {
  std::lock_guard<std::mutex> lock(latch_);
  
  // Check if LRU list is empty
  if (lru_list_.empty()) {
    return false;
  }

  // Get least recently used frame from back of list
  *frame_id = lru_list_.back();
  
  // Remove from list and hash map
  lru_list_.pop_back();
  lru_hash_.erase(*frame_id);
  
  return true;
}

/**
 * TODO: Student Implement
 * @brief Pin a frame, preventing it from being victimized
 * @param frame_id The frame ID to pin
 * @note This method should remove the frame from the replacer as it should not be victimized until unpinned
 */
void LRUReplacer::Pin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> lock(latch_);
  
  // Check if frame exists in hash map
  auto it = lru_hash_.find(frame_id);
  if (it != lru_hash_.end()) {
    // Remove from list and hash map
    lru_list_.erase(it->second);
    lru_hash_.erase(it);
  }
}

/**
 * TODO: Student Implement
 * @brief Unpin a frame, making it eligible for victimization
 * @param frame_id The frame ID to unpin
 * @note This method should add the frame back to the replacer so it can be victimized in the future
 */
void LRUReplacer::Unpin(frame_id_t frame_id) {
  std::lock_guard<std::mutex> lock(latch_);
  
  // Check if frame is already in replacer
  if (lru_hash_.find(frame_id) != lru_hash_.end()) {
    return;
  }

  // Add to front of list (most recently used)
  lru_list_.push_front(frame_id);
  lru_hash_[frame_id] = lru_list_.begin();
}

/**
 * TODO: Student Implement
 * @brief Return the number of frames that can be victimized
 * @return The number of frames in the replacer that can be victimized
 * @note This method should return the number of unpinned frames in the replacer
 */
size_t LRUReplacer::Size() {
  std::lock_guard<std::mutex> lock(latch_);
  return lru_list_.size();
}