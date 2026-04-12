#include "storage/table_iterator.h"

#include "common/macros.h"
#include "storage/table_heap.h"

/**
 * TODO: Student Implement
 */
TableIterator::TableIterator(TableHeap *table_heap, RowId rid, Txn *txn) 
  : table_heap_(table_heap), row_(new Row(rid)), txn_(txn) {
  if (table_heap_ != nullptr && !row_->GetRowId().IsNull()) {
    table_heap_->GetTuple(row_, txn_);
  }
}

TableIterator::TableIterator(const TableIterator &other) {
  table_heap_ = other.table_heap_;
  txn_ = other.txn_;
  row_ = new Row(*other.row_);
}

TableIterator::~TableIterator() {
  delete row_;
}

bool TableIterator::operator==(const TableIterator &itr) const {
  return table_heap_ == itr.table_heap_ && 
         (row_->GetRowId() == itr.row_->GetRowId() || 
          (row_->GetRowId().IsNull() && itr.row_->GetRowId().IsNull()));
}

bool TableIterator::operator!=(const TableIterator &itr) const {
  return !(*this == itr);
}

const Row &TableIterator::operator*() {
  ASSERT(row_ != nullptr, "Invalid row pointer");
  return *row_;
}

Row *TableIterator::operator->() {
  ASSERT(row_ != nullptr, "Invalid row pointer");
  return row_;
}

TableIterator &TableIterator::operator=(const TableIterator &itr) noexcept {
  if (this == &itr) {
    return *this;
  }
  
  table_heap_ = itr.table_heap_;
  txn_ = itr.txn_;
  delete row_;
  row_ = new Row(*itr.row_);
  return *this;
}

// ++iter
TableIterator &TableIterator::operator++() {
  ASSERT(table_heap_ != nullptr, "Invalid table heap");
  ASSERT(row_ != nullptr, "Invalid row pointer");

  auto next_rid = row_->GetRowId().Next();
  delete row_;
  row_ = new Row(next_rid);

  // Keep moving to next valid tuple
  while (!row_->GetRowId().IsNull() && !table_heap_->GetTuple(row_, txn_)) {
    next_rid = row_->GetRowId().Next();
    delete row_;
    row_ = new Row(next_rid);
  }

  return *this;
}

// iter++
TableIterator TableIterator::operator++(int) {
  TableIterator clone(*this);
  ++(*this);
  return clone;
}
