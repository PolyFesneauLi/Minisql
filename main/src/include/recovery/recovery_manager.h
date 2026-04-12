#ifndef MINISQL_RECOVERY_MANAGER_H
#define MINISQL_RECOVERY_MANAGER_H

#include <map>
#include <unordered_map>
#include <vector>

#include "recovery/log_rec.h"

using KvDatabase = std::unordered_map<KeyType, ValType>;
using ATT = std::unordered_map<txn_id_t, lsn_t>;

struct CheckPoint {
    lsn_t checkpoint_lsn_{INVALID_LSN};
    ATT active_txns_{};
    KvDatabase persist_data_{};

    inline void AddActiveTxn(txn_id_t txn_id, lsn_t last_lsn) { active_txns_[txn_id] = last_lsn; }

    inline void AddData(KeyType key, ValType val) { persist_data_.emplace(std::move(key), val); }
};

class RecoveryManager {
public:
    /**
    * TODO: Student Implement
    */
    void Init(CheckPoint &last_checkpoint) {
        // Initialize database state from checkpoint
        data_ = last_checkpoint.persist_data_;
        active_txns_ = last_checkpoint.active_txns_;
        persist_lsn_ = last_checkpoint.checkpoint_lsn_;
    }

    /**
    * TODO: Student Implement
    */
    void RedoPhase() {
        // Redo all operations after checkpoint
        for (const auto &[lsn, log_rec] : log_recs_) {
            if (lsn <= persist_lsn_) {
                continue;  // Skip logs before checkpoint
            }

            switch (log_rec->type_) {
                case LogRecType::kInsert:
                    data_[log_rec->key_] = log_rec->val_;
                    break;
                case LogRecType::kDelete:
                    data_.erase(log_rec->key_);
                    break;
                case LogRecType::kUpdate:
                    data_[log_rec->new_key_] = log_rec->new_val_;
                    break;
                case LogRecType::kBegin:
                    active_txns_[log_rec->txn_id_] = log_rec->lsn_;
                    break;
                case LogRecType::kCommit:
                case LogRecType::kAbort:
                    active_txns_.erase(log_rec->txn_id_);
                    break;
                default:
                    break;
            }
        }
    }

    /**
    * TODO: Student Implement
    */
    void UndoPhase() {
        // Undo active transactions in reverse order
        while (!active_txns_.empty()) {
            // Find the latest LSN among active transactions
            lsn_t max_lsn = INVALID_LSN;
            txn_id_t txn_to_undo = INVALID_TXN_ID;
            
            for (const auto &[txn_id, last_lsn] : active_txns_) {
                if (last_lsn > max_lsn) {
                    max_lsn = last_lsn;
                    txn_to_undo = txn_id;
                }
            }

            // Undo the transaction's operations
            lsn_t current_lsn = max_lsn;
            while (current_lsn != INVALID_LSN) {
                auto log_rec = log_recs_[current_lsn];
                
                switch (log_rec->type_) {
                    case LogRecType::kInsert:
                        data_.erase(log_rec->key_);
                        break;
                    case LogRecType::kDelete:
                        data_[log_rec->key_] = log_rec->val_;
                        break;
                    case LogRecType::kUpdate:
                        data_[log_rec->key_] = log_rec->val_;
                        break;
                    default:
                        break;
                }
                
                current_lsn = log_rec->prev_lsn_;
            }

            // Remove transaction from active set
            active_txns_.erase(txn_to_undo);
        }
    }

    // used for test only
    void AppendLogRec(LogRecPtr log_rec) { log_recs_.emplace(log_rec->lsn_, log_rec); }

    // used for test only
    inline KvDatabase &GetDatabase() { return data_; }

private:
    std::map<lsn_t, LogRecPtr> log_recs_{};
    lsn_t persist_lsn_{INVALID_LSN};
    ATT active_txns_{};
    KvDatabase data_{};  // all data in database
};

#endif  // MINISQL_RECOVERY_MANAGER_H
