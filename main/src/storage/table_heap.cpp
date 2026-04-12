#include "storage/table_heap.h"

/**
 * TODO: Student Implement
 * @brief Insert a tuple into the table
 * @param[in/out] row Row to insert
 * @param[in] txn Transaction performing the insert
 * @return true if insert succeeded, false otherwise
 */
bool TableHeap::InsertTuple(Row &row, Txn *txn) {
  uint32_t serialized_size = row.GetSerializedSize(schema_);
  if (serialized_size + TablePage::SIZE_TABLE_PAGE_HEADER > PAGE_SIZE) {
    return false;  // Row too large to fit in a page
  }

  // Try to find a page with enough space
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(first_page_id_));
  if (page == nullptr) {
    return false;
  }

  page->WLatch();
  while (true) {
    if (page->GetFreeSpaceSize() >= serialized_size) {
      // Found a page with enough space
      if (page->InsertTuple(row, schema_, txn, lock_manager_, log_manager_)) {
        page->WUnlatch();
        buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
        return true;
      }
    }

    // Current page full, try next page
    page_id_t next_page_id = page->GetNextPageId();
    if (next_page_id == INVALID_PAGE_ID) {
      // Need to create new page
      auto new_page = reinterpret_cast<TablePage *>(buffer_pool_manager_->NewPage(next_page_id));
      if (new_page == nullptr) {
        page->WUnlatch();
        buffer_pool_manager_->UnpinPage(page->GetTablePageId(), false);
        return false;
      }
      // Initialize new page and link it
      new_page->Init(next_page_id, PAGE_SIZE, page->GetTablePageId(), log_manager_, txn);
      page->SetNextPageId(next_page_id);
      page->WUnlatch();
      buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
      page = new_page;
    } else {
      // Move to next existing page
      auto next_page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(next_page_id));
      page->WUnlatch();
      buffer_pool_manager_->UnpinPage(page->GetTablePageId(), false);
      if (next_page == nullptr) {
        return false;
      }
      page = next_page;
      page->WLatch();
    }
  }
}

bool TableHeap::MarkDelete(const RowId &rid, Txn *txn) {
  // Find the page which contains the tuple.
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(rid.GetPageId()));
  // If the page could not be found, then abort the recovery.
  if (page == nullptr) {
    return false;
  }
  // Otherwise, mark the tuple as deleted.
  page->WLatch();
  page->MarkDelete(rid, txn, lock_manager_, log_manager_);
  page->WUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
  return true;
}

/**
 * TODO: Student Implement
 * @brief Update a tuple in the table
 * @param[in] row Row to update with
 * @param[in] rid Row ID to update
 * @param[in] txn Transaction performing the update
 * @return true if update succeeded, false otherwise
 */
bool TableHeap::UpdateTuple(Row &row, const RowId &rid, Txn *txn) {
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(rid.GetPageId()));
  if (page == nullptr) {
    return false;
  }

  page->WLatch();
  bool update_success = page->UpdateTuple(row, schema_, rid, txn, lock_manager_, log_manager_);
  page->WUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), update_success);

  return update_success;
}

/**
 * TODO: Student Implement
 * @brief Delete a tuple from the table
 * @param[in] rid Row ID to delete
 * @param[in] txn Transaction performing the delete
 */
void TableHeap::ApplyDelete(const RowId &rid, Txn *txn) {
  // Step1: Find the page which contains the tuple
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(rid.GetPageId()));
  if (page == nullptr) {
    return;
  }

  // Step2: Delete the tuple from the page
  page->WLatch();
  page->ApplyDelete(rid, txn, log_manager_);
  page->WUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
}

void TableHeap::RollbackDelete(const RowId &rid, Txn *txn) {
  // Find the page which contains the tuple.
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(rid.GetPageId()));
  assert(page != nullptr);
  // Rollback to delete.
  page->WLatch();
  page->RollbackDelete(rid, txn, log_manager_);
  page->WUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
}

/**
 * TODO: Student Implement
 * @brief Get a tuple from the table
 * @param[in/out] row Row to fill with tuple data
 * @param[in] txn Transaction performing the read
 * @return true if get succeeded, false otherwise
 */
bool TableHeap::GetTuple(Row *row, Txn *txn) {
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(row->GetRowId().GetPageId()));
  if (page == nullptr) {
    return false;
  }

  page->RLatch();
  bool get_success = page->GetTuple(row, schema_, txn, lock_manager_);
  page->RUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), false);

  return get_success;
}

void TableHeap::DeleteTable(page_id_t page_id) {
  if (page_id != INVALID_PAGE_ID) {
    auto temp_table_page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(page_id));  // 删除table_heap
    if (temp_table_page->GetNextPageId() != INVALID_PAGE_ID)
      DeleteTable(temp_table_page->GetNextPageId());
    buffer_pool_manager_->UnpinPage(page_id, false);
    buffer_pool_manager_->DeletePage(page_id);
  } else {
    DeleteTable(first_page_id_);
  }
}

/**
 * TODO: Student Implement
 * @brief Return the begin iterator of this table
 * @param[in] txn Transaction performing the scan
 * @return Iterator pointing to the first tuple
 */
TableIterator TableHeap::Begin(Txn *txn) {
  auto first_page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(first_page_id_));
  RowId first_rid;
  
  // Find first valid tuple
  while (first_page != nullptr) {
    first_page->RLatch();
    bool found = first_page->GetFirstTupleRid(&first_rid);
    first_page->RUnlatch();
    
    if (found) {
      // Found first tuple
      Row *row = new Row(first_rid);
      GetTuple(row, txn);
      return TableIterator(this, first_rid, txn);
    }
    
    // Try next page
    page_id_t next_page_id = first_page->GetNextPageId();
    buffer_pool_manager_->UnpinPage(first_page->GetTablePageId(), false);
    if (next_page_id == INVALID_PAGE_ID) {
      break;
    }
    first_page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(next_page_id));
  }
  
  // No tuples found
  if (first_page != nullptr) {
    buffer_pool_manager_->UnpinPage(first_page->GetTablePageId(), false);
  }
  return End();
}

/**
 * TODO: Student Implement
 * @brief Return the end iterator of this table
 * @return Iterator pointing past the last tuple
 */
TableIterator TableHeap::End() {
  return TableIterator(this, RowId(INVALID_PAGE_ID, 0), nullptr);
}
