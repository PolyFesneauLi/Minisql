#include "storage/disk_manager.h"

#include <sys/stat.h>

#include <filesystem>
#include <stdexcept>

#include "glog/logging.h"
#include "page/bitmap_page.h"

DiskManager::DiskManager(const std::string &db_file) : file_name_(db_file) {
  std::scoped_lock<std::recursive_mutex> lock(db_io_latch_);
  db_io_.open(db_file, std::ios::binary | std::ios::in | std::ios::out);
  // directory or file does not exist
  if (!db_io_.is_open()) {
    db_io_.clear();
    // create a new file
    std::filesystem::path p = db_file;
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    db_io_.open(db_file, std::ios::binary | std::ios::trunc | std::ios::out);
    db_io_.close();
    // reopen with original mode
    db_io_.open(db_file, std::ios::binary | std::ios::in | std::ios::out);
    if (!db_io_.is_open()) {
      throw std::exception();
    }
  }
  ReadPhysicalPage(META_PAGE_ID, meta_data_);
}

void DiskManager::Close() {
  std::scoped_lock<std::recursive_mutex> lock(db_io_latch_);
  WritePhysicalPage(META_PAGE_ID, meta_data_);
  if (!closed) {
    db_io_.close();
    closed = true;
  }
}

void DiskManager::ReadPage(page_id_t logical_page_id, char *page_data) {
  ASSERT(logical_page_id >= 0, "Invalid page id.");
  ReadPhysicalPage(MapPageId(logical_page_id), page_data);
}

void DiskManager::WritePage(page_id_t logical_page_id, const char *page_data) {
  ASSERT(logical_page_id >= 0, "Invalid page id.");
  WritePhysicalPage(MapPageId(logical_page_id), page_data);
}

/**
 * TODO: Student Implement
 */
page_id_t DiskManager::AllocatePage() {
  std::scoped_lock<std::recursive_mutex> lock(db_io_latch_);

  // Get extent id and page id within extent
  uint32_t extent_id = 0;
  page_id_t page_id = INVALID_PAGE_ID;
  
  // Read meta page to find next free extent
  char bitmap_data[PAGE_SIZE];
  while (extent_id < MAX_EXTENT_SIZE) {
    // Read bitmap page for current extent
    ReadPhysicalPage(BITMAP_PAGE_ID + extent_id, bitmap_data);
    auto bitmap_page = reinterpret_cast<BitmapPage<PAGE_SIZE> *>(bitmap_data);
    
    // Try to allocate page in current extent
    uint32_t page_offset;
    if (bitmap_page->AllocatePage(page_offset)) {
      // Write updated bitmap back to disk
      WritePhysicalPage(BITMAP_PAGE_ID + extent_id, bitmap_data);
      
      // Calculate logical page id
      page_id = 1 + extent_id * BITMAP_SIZE + page_offset;
      break;
    }
    extent_id++;
  }

  return page_id;
}

/**
 * TODO: Student Implement
 */
void DiskManager::DeAllocatePage(page_id_t logical_page_id) {
  std::scoped_lock<std::recursive_mutex> lock(db_io_latch_);

  // Calculate extent id and page offset
  uint32_t extent_id = (logical_page_id - 1) / BITMAP_SIZE;
  uint32_t page_offset = (logical_page_id - 1) % BITMAP_SIZE;

  if (extent_id >= MAX_EXTENT_SIZE) {
    return;
  }

  // Read bitmap page
  char bitmap_data[PAGE_SIZE];
  ReadPhysicalPage(BITMAP_PAGE_ID + extent_id, bitmap_data);
  auto bitmap_page = reinterpret_cast<BitmapPage<PAGE_SIZE> *>(bitmap_data);

  // Deallocate page
  if (bitmap_page->DeAllocatePage(page_offset)) {
    // Write updated bitmap back to disk
    WritePhysicalPage(BITMAP_PAGE_ID + extent_id, bitmap_data);
  }
}

/**
 * TODO: Student Implement
 */
bool DiskManager::IsPageFree(page_id_t logical_page_id) {
  std::scoped_lock<std::recursive_mutex> lock(db_io_latch_);

  // Calculate extent id and page offset
  uint32_t extent_id = (logical_page_id - 1) / BITMAP_SIZE;
  uint32_t page_offset = (logical_page_id - 1) % BITMAP_SIZE;

  if (extent_id >= MAX_EXTENT_SIZE) {
    return false;
  }

  // Read bitmap page
  char bitmap_data[PAGE_SIZE];
  ReadPhysicalPage(BITMAP_PAGE_ID + extent_id, bitmap_data);
  auto bitmap_page = reinterpret_cast<BitmapPage<PAGE_SIZE> *>(bitmap_data);

  // Check if page is free
  return bitmap_page->IsPageFree(page_offset);
}

/**
 * TODO: Student Implement
 */
page_id_t DiskManager::MapPageId(page_id_t logical_page_id) {
  // Special case for meta pages
  if (logical_page_id < META_PAGE_COUNT) {
    return logical_page_id;
  }

  // Calculate physical page id
  // Meta pages + bitmap pages + data pages
  return META_PAGE_COUNT + 
         ((logical_page_id - 1) / BITMAP_SIZE) + // Bitmap pages before this extent
         logical_page_id; // The actual data page
}

int DiskManager::GetFileSize(const std::string &file_name) {
  struct stat stat_buf;
  int rc = stat(file_name.c_str(), &stat_buf);
  return rc == 0 ? stat_buf.st_size : -1;
}

void DiskManager::ReadPhysicalPage(page_id_t physical_page_id, char *page_data) {
  int offset = physical_page_id * PAGE_SIZE;
  // check if read beyond file length
  if (offset >= GetFileSize(file_name_)) {
#ifdef ENABLE_BPM_DEBUG
    LOG(INFO) << "Read less than a page" << std::endl;
#endif
    memset(page_data, 0, PAGE_SIZE);
  } else {
    // set read cursor to offset
    db_io_.seekp(offset);
    db_io_.read(page_data, PAGE_SIZE);
    // if file ends before reading PAGE_SIZE
    int read_count = db_io_.gcount();
    if (read_count < PAGE_SIZE) {
#ifdef ENABLE_BPM_DEBUG
      LOG(INFO) << "Read less than a page" << std::endl;
#endif
      memset(page_data + read_count, 0, PAGE_SIZE - read_count);
    }
  }
}

void DiskManager::WritePhysicalPage(page_id_t physical_page_id, const char *page_data) {
  size_t offset = static_cast<size_t>(physical_page_id) * PAGE_SIZE;
  // set write cursor to offset
  db_io_.seekp(offset);
  db_io_.write(page_data, PAGE_SIZE);
  // check for I/O error
  if (db_io_.bad()) {
    LOG(ERROR) << "I/O error while writing";
    return;
  }
  // needs to flush to keep disk file in sync
  db_io_.flush();
}