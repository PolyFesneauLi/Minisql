#include "page/bitmap_page.h"

#include "glog/logging.h"

/**
 * TODO: Student Implement
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::AllocatePage(uint32_t &page_offset) {
  // Check if we have reached the maximum number of pages
  if (!IsPageFree(page_offset)) {
    return false;
  }

  // Find the first free page by checking each byte and bit
  for (uint32_t byte_index = 0; byte_index < MAX_CHARS; byte_index++) {
    if (bytes[byte_index] != 0xFF) {  // If byte is not full
      for (uint8_t bit_index = 0; bit_index < 8; bit_index++) {
        if (IsPageFreeLow(byte_index, bit_index)) {
          // Calculate the page offset
          page_offset = byte_index * 8 + bit_index;
          
          // Set the bit to 1 (mark as allocated)
          bytes[byte_index] |= (1 << bit_index);
          page_allocated_++;
          return true;
        }
      }
    }
  }
  
  return false;
}

/**
 * TODO: Student Implement
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::DeAllocatePage(uint32_t page_offset) {
  if(IsPageFree(page_offset)){
    return false;
  }
  // Calculate byte and bit indices
  uint32_t byte_index = page_offset / 8;
  uint8_t bit_index = page_offset % 8;
  
  // Set the bit to 0 (mark as free)
  bytes[byte_index] &= ~(1 << bit_index);
  page_allocated_--;
  return true;
}

/**
 * TODO: Student Implement
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::IsPageFree(uint32_t page_offset) const {
  if (page_offset >= GetMaxSupportedSize()) {
    return false;
  }

  // Find the byte and bit index for the given page offset
  uint32_t byte_index = page_offset / 8;
  uint8_t bit_index = page_offset % 8;
  
  // Check if the page is actually allocated
  if (IsPageFreeLow(byte_index, bit_index)) {
    return true;  // Page is already free
  }
  return false;
}

template <size_t PageSize>
bool BitmapPage<PageSize>::IsPageFreeLow(uint32_t byte_index, uint8_t bit_index) const {
  return (bytes[byte_index] & (1 << bit_index)) == 0;
}

template class BitmapPage<64>;

template class BitmapPage<128>;

template class BitmapPage<256>;

template class BitmapPage<512>;

template class BitmapPage<1024>;

template class BitmapPage<2048>;

template class BitmapPage<4096>;