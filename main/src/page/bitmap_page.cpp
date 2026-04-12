#include "page/bitmap_page.h"

#include "glog/logging.h"

/**
 * TODO: Student Implement
 * @brief Allocate a page from bitmap
 * @param[out] page_offset Return the allocated page offset
 * @return true if successfully allocate a page
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::AllocatePage(uint32_t &page_offset) {
  // If no free pages available, return false
  if (page_allocated_ >= GetMaxSupportedSize()) {
    return false;
  }

  // Find first free page using next_free_page_ hint
  uint32_t byte_index = next_free_page_ / 8;
  uint8_t bit_index = next_free_page_ % 8;

  // Search for a free page
  while (byte_index < MAX_CHARS) {
    if (bytes[byte_index] != 0xFF) {  // Found byte with free bit
      while (bit_index < 8) {
        if (!(bytes[byte_index] & (0x1 << bit_index))) {  // Found free bit
          // Allocate the page by setting the bit
          bytes[byte_index] |= (0x1 << bit_index);
          page_offset = byte_index * 8 + bit_index;
          page_allocated_++;
          
          // Update next_free_page_ hint
          next_free_page_ = page_offset + 1;
          return true;
        }
        bit_index++;
      }
    }
    byte_index++;
    bit_index = 0;
  }
  return false;
}

/**
 * TODO: Student Implement
 * @brief Deallocate a page in bitmap
 * @param page_offset The page offset to be deallocated
 * @return true if successfully deallocate a page
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::DeAllocatePage(uint32_t page_offset) {
  if (page_offset >= GetMaxSupportedSize()) {
    return false;
  }

  // Calculate byte and bit index
  uint32_t byte_index = page_offset / 8;
  uint8_t bit_index = page_offset % 8;

  // Check if page is already free
  if (!(bytes[byte_index] & (0x1 << bit_index))) {
    return false;
  }

  // Free the page by clearing the bit
  bytes[byte_index] &= ~(0x1 << bit_index);
  page_allocated_--;

  // Update next_free_page_ hint if needed
  if (page_offset < next_free_page_) {
    next_free_page_ = page_offset;
  }

  return true;
}

/**
 * TODO: Student Implement
 * @brief Check if a page is free
 * @param page_offset The page offset to check
 * @return true if the page is free
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::IsPageFree(uint32_t page_offset) const {
  if (page_offset >= GetMaxSupportedSize()) {
    return false;
  }

  // Calculate byte and bit index
  uint32_t byte_index = page_offset / 8;
  uint8_t bit_index = page_offset % 8;

  // Check if bit is set (page allocated) or clear (page free)
  return !(bytes[byte_index] & (0x1 << bit_index));
}

/**
 * @brief Check if a page is free using byte and bit indexes
 * @param byte_index Index of the byte to check
 * @param bit_index Index of the bit to check
 * @return true if the page is free
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::IsPageFreeLow(uint32_t byte_index, uint8_t bit_index) const {
  if (byte_index >= MAX_CHARS || bit_index >= 8) {
    return false;
  }
  return !(bytes[byte_index] & (0x1 << bit_index));
}

template class BitmapPage<64>;

template class BitmapPage<128>;

template class BitmapPage<256>;

template class BitmapPage<512>;

template class BitmapPage<1024>;

template class BitmapPage<2048>;

template class BitmapPage<4096>;