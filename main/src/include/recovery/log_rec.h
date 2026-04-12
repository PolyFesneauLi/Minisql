#ifndef MINISQL_LOG_REC_H
#define MINISQL_LOG_REC_H

#include <unordered_map>
#include <utility>

#include "common/config.h"
#include "common/rowid.h"
#include "record/row.h"

enum class LogRecType {
    kInvalid,
    kInsert,
    kDelete,
    kUpdate,
    kBegin,
    kCommit,
    kAbort,
};

// used for testing only
using KeyType = std::string;
using ValType = int32_t;

/**
 * TODO: Student Implement
 */
struct LogRec {
    LogRec() = default;

    LogRecType type_{LogRecType::kInvalid};
    lsn_t lsn_{INVALID_LSN};
    lsn_t prev_lsn_{INVALID_LSN};
    txn_id_t txn_id_{INVALID_TXN_ID};

    // Data for different log types
    KeyType key_;
    ValType val_;
    KeyType new_key_;
    ValType new_val_;

    /* used for testing only */
    static std::unordered_map<txn_id_t, lsn_t> prev_lsn_map_;
    static lsn_t next_lsn_;
};

std::unordered_map<txn_id_t, lsn_t> LogRec::prev_lsn_map_ = {};
lsn_t LogRec::next_lsn_ = 0;

typedef std::shared_ptr<LogRec> LogRecPtr;

/**
 * TODO: Student Implement
 */
static LogRecPtr CreateInsertLog(txn_id_t txn_id, KeyType ins_key, ValType ins_val) {
    auto log_rec = std::make_shared<LogRec>();
    log_rec->type_ = LogRecType::kInsert;
    log_rec->txn_id_ = txn_id;
    log_rec->key_ = ins_key;
    log_rec->val_ = ins_val;
    log_rec->prev_lsn_ = LogRec::prev_lsn_map_[txn_id];
    log_rec->lsn_ = LogRec::next_lsn_++;
    LogRec::prev_lsn_map_[txn_id] = log_rec->lsn_;
    return log_rec;
}

/**
 * TODO: Student Implement
 */
static LogRecPtr CreateDeleteLog(txn_id_t txn_id, KeyType del_key, ValType del_val) {
    auto log_rec = std::make_shared<LogRec>();
    log_rec->type_ = LogRecType::kDelete;
    log_rec->txn_id_ = txn_id;
    log_rec->key_ = del_key;
    log_rec->val_ = del_val;
    log_rec->prev_lsn_ = LogRec::prev_lsn_map_[txn_id];
    log_rec->lsn_ = LogRec::next_lsn_++;
    LogRec::prev_lsn_map_[txn_id] = log_rec->lsn_;
    return log_rec;
}

/**
 * TODO: Student Implement
 */
static LogRecPtr CreateUpdateLog(txn_id_t txn_id, KeyType old_key, ValType old_val, KeyType new_key, ValType new_val) {
    auto log_rec = std::make_shared<LogRec>();
    log_rec->type_ = LogRecType::kUpdate;
    log_rec->txn_id_ = txn_id;
    log_rec->key_ = old_key;
    log_rec->val_ = old_val;
    log_rec->new_key_ = new_key;
    log_rec->new_val_ = new_val;
    log_rec->prev_lsn_ = LogRec::prev_lsn_map_[txn_id];
    log_rec->lsn_ = LogRec::next_lsn_++;
    LogRec::prev_lsn_map_[txn_id] = log_rec->lsn_;
    return log_rec;
}

/**
 * TODO: Student Implement
 */
static LogRecPtr CreateBeginLog(txn_id_t txn_id) {
    auto log_rec = std::make_shared<LogRec>();
    log_rec->type_ = LogRecType::kBegin;
    log_rec->txn_id_ = txn_id;
    log_rec->prev_lsn_ = INVALID_LSN;  // First log for transaction
    log_rec->lsn_ = LogRec::next_lsn_++;
    LogRec::prev_lsn_map_[txn_id] = log_rec->lsn_;
    return log_rec;
}

/**
 * TODO: Student Implement
 */
static LogRecPtr CreateCommitLog(txn_id_t txn_id) {
    auto log_rec = std::make_shared<LogRec>();
    log_rec->type_ = LogRecType::kCommit;
    log_rec->txn_id_ = txn_id;
    log_rec->prev_lsn_ = LogRec::prev_lsn_map_[txn_id];
    log_rec->lsn_ = LogRec::next_lsn_++;
    LogRec::prev_lsn_map_.erase(txn_id);  // Remove from active transactions
    return log_rec;
}

/**
 * TODO: Student Implement
 */
static LogRecPtr CreateAbortLog(txn_id_t txn_id) {
    auto log_rec = std::make_shared<LogRec>();
    log_rec->type_ = LogRecType::kAbort;
    log_rec->txn_id_ = txn_id;
    log_rec->prev_lsn_ = LogRec::prev_lsn_map_[txn_id];
    log_rec->lsn_ = LogRec::next_lsn_++;
    LogRec::prev_lsn_map_.erase(txn_id);  // Remove from active transactions
    return log_rec;
}

#endif  // MINISQL_LOG_REC_H
