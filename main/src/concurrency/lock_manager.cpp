#include "concurrency/lock_manager.h"

#include <iostream>
#include <queue>
#include <unordered_set>

#include "common/rowid.h"
#include "concurrency/txn.h"
#include "concurrency/txn_manager.h"

void LockManager::SetTxnMgr(TxnManager *txn_mgr) { txn_mgr_ = txn_mgr; }

/**
 * TODO: Student Implement
 */
bool LockManager::LockShared(Txn *txn, const RowId &rid) {
    std::unique_lock<std::mutex> lock(latch_);
    
    auto &req_queue = lock_table_[rid];
    LockRequest req(txn->GetTxnId(), LockMode::kShared);
    
    // Check if transaction already has the lock
    for (const auto &existing_req : req_queue.req_list_) {
        if (existing_req.txn_id_ == txn->GetTxnId()) {
            if (existing_req.granted_ != LockMode::kNone) {
                return true;  // Already has lock
            }
            break;
        }
    }

    // Add request to queue
    req_queue.req_list_.push_back(req);
    
    // Check if can grant immediately
    bool can_grant = true;
    for (const auto &existing_req : req_queue.req_list_) {
        if (existing_req.granted_ == LockMode::kExclusive) {
            can_grant = false;
            break;
        }
    }

    if (can_grant) {
        req_queue.req_list_.back().granted_ = LockMode::kShared;
        txn->GetSharedLockSet()->insert(rid);
        return true;
    }

    // Add wait-for edges
    for (const auto &existing_req : req_queue.req_list_) {
        if (existing_req.granted_ == LockMode::kExclusive) {
            AddEdge(txn->GetTxnId(), existing_req.txn_id_);
        }
    }

    // Check for deadlock
    txn_id_t abort_id;
    if (HasCycle(abort_id)) {
        if (abort_id == txn->GetTxnId()) {
            req_queue.req_list_.pop_back();  // Remove request
            RemoveEdge(txn->GetTxnId(), abort_id);
            txn->SetState(TransactionState::kAborted);
            return false;
        }
    }

    return true;
}

/**
 * TODO: Student Implement
 */
bool LockManager::LockExclusive(Txn *txn, const RowId &rid) {
    std::unique_lock<std::mutex> lock(latch_);
    
    auto &req_queue = lock_table_[rid];
    LockRequest req(txn->GetTxnId(), LockMode::kExclusive);
    
    // Check if transaction already has the lock
    for (const auto &existing_req : req_queue.req_list_) {
        if (existing_req.txn_id_ == txn->GetTxnId()) {
            if (existing_req.granted_ == LockMode::kExclusive) {
                return true;  // Already has exclusive lock
            }
            break;
        }
    }

    // Add request to queue
    req_queue.req_list_.push_back(req);
    
    // Check if can grant immediately
    bool can_grant = req_queue.req_list_.size() == 1;

    if (can_grant) {
        req_queue.req_list_.back().granted_ = LockMode::kExclusive;
        txn->GetExclusiveLockSet()->insert(rid);
        return true;
    }

    // Add wait-for edges
    for (const auto &existing_req : req_queue.req_list_) {
        if (existing_req.granted_ != LockMode::kNone) {
            AddEdge(txn->GetTxnId(), existing_req.txn_id_);
        }
    }

    // Check for deadlock
    txn_id_t abort_id;
    if (HasCycle(abort_id)) {
        if (abort_id == txn->GetTxnId()) {
            req_queue.req_list_.pop_back();  // Remove request
            RemoveEdge(txn->GetTxnId(), abort_id);
            txn->SetState(TransactionState::kAborted);
            return false;
        }
    }

    return true;
}

/**
 * TODO: Student Implement
 */
bool LockManager::LockUpgrade(Txn *txn, const RowId &rid) {
    std::unique_lock<std::mutex> lock(latch_);
    
    auto &req_queue = lock_table_[rid];
    
    // Find existing shared lock
    auto it = req_queue.req_list_.begin();
    for (; it != req_queue.req_list_.end(); ++it) {
        if (it->txn_id_ == txn->GetTxnId() && it->granted_ == LockMode::kShared) {
            break;
        }
    }
    
    if (it == req_queue.req_list_.end()) {
        return false;  // No shared lock found
    }

    // Check if upgrade is possible
    bool can_upgrade = true;
    for (const auto &req : req_queue.req_list_) {
        if (req.txn_id_ != txn->GetTxnId() && req.granted_ != LockMode::kNone) {
            can_upgrade = false;
            break;
        }
    }

    if (can_upgrade) {
        // Upgrade lock
        it->granted_ = LockMode::kExclusive;
        txn->GetSharedLockSet()->erase(rid);
        txn->GetExclusiveLockSet()->insert(rid);
        return true;
    }

    // Add wait-for edges
    for (const auto &req : req_queue.req_list_) {
        if (req.txn_id_ != txn->GetTxnId() && req.granted_ != LockMode::kNone) {
            AddEdge(txn->GetTxnId(), req.txn_id_);
        }
    }

    // Check for deadlock
    txn_id_t abort_id;
    if (HasCycle(abort_id)) {
        if (abort_id == txn->GetTxnId()) {
            RemoveEdge(txn->GetTxnId(), abort_id);
            txn->SetState(TransactionState::kAborted);
            return false;
        }
    }

    return true;
}

/**
 * TODO: Student Implement
 */
bool LockManager::Unlock(Txn *txn, const RowId &rid) {
    std::unique_lock<std::mutex> lock(latch_);
    
    auto &req_queue = lock_table_[rid];
    
    // Find and remove lock request
    auto it = req_queue.req_list_.begin();
    for (; it != req_queue.req_list_.end(); ++it) {
        if (it->txn_id_ == txn->GetTxnId() && it->granted_ != LockMode::kNone) {
            // Remove from transaction's lock sets
            if (it->granted_ == LockMode::kShared) {
                txn->GetSharedLockSet()->erase(rid);
            } else {
                txn->GetExclusiveLockSet()->erase(rid);
            }
            
            // Remove request
            req_queue.req_list_.erase(it);
            
            // Grant pending requests if possible
            for (auto &req : req_queue.req_list_) {
                if (req.granted_ == LockMode::kNone) {
                    if (req.lock_mode_ == LockMode::kShared) {
                        // Grant all pending shared locks
                        bool can_grant = true;
                        for (const auto &existing_req : req_queue.req_list_) {
                            if (existing_req.granted_ == LockMode::kExclusive) {
                                can_grant = false;
                                break;
                            }
                        }
                        if (can_grant) {
                            req.granted_ = LockMode::kShared;
                            auto waiting_txn = txn_mgr_->GetTransaction(req.txn_id_);
                            waiting_txn->GetSharedLockSet()->insert(rid);
                        }
                    } else {
                        // Grant exclusive lock if no other locks
                        if (req_queue.req_list_.size() == 1) {
                            req.granted_ = LockMode::kExclusive;
                            auto waiting_txn = txn_mgr_->GetTransaction(req.txn_id_);
                            waiting_txn->GetExclusiveLockSet()->insert(rid);
                        }
                    }
                }
            }
            
            return true;
        }
    }
    
    return false;
}

/**
 * TODO: Student Implement
 */
void LockManager::LockPrepare(Txn *txn, const RowId &rid) {}

/**
 * TODO: Student Implement
 */
void LockManager::CheckAbort(Txn *txn, LockManager::LockRequestQueue &req_queue) {
}

/**
 * TODO: Student Implement
 */
void LockManager::AddEdge(txn_id_t t1, txn_id_t t2) {
    waits_for_[t1].insert(t2);
}

/**
 * TODO: Student Implement
 */
void LockManager::RemoveEdge(txn_id_t t1, txn_id_t t2) {
    if (waits_for_.find(t1) != waits_for_.end()) {
        waits_for_[t1].erase(t2);
    }
}

/**
 * TODO: Student Implement
 */
bool LockManager::HasCycle(txn_id_t &newest_tid_in_cycle) {
    std::unordered_set<txn_id_t> visited;
    std::unordered_set<txn_id_t> in_path;
    
    for (const auto &[txn_id, _] : waits_for_) {
        if (visited.find(txn_id) == visited.end()) {
            if (DFS(txn_id, visited, in_path, newest_tid_in_cycle)) {
                return true;
            }
        }
    }
    
    return false;
}

bool LockManager::DFS(txn_id_t current, std::unordered_set<txn_id_t> &visited,
                     std::unordered_set<txn_id_t> &in_path, txn_id_t &newest_tid) {
    visited.insert(current);
    in_path.insert(current);
    
    if (waits_for_.find(current) != waits_for_.end()) {
        for (txn_id_t next : waits_for_[current]) {
            if (in_path.find(next) != in_path.end()) {
                // Found cycle, find newest transaction
                newest_tid = current;
                for (txn_id_t tid : in_path) {
                    if (tid > newest_tid) {
                        newest_tid = tid;
                    }
                }
                return true;
            }
            
            if (visited.find(next) == visited.end()) {
                if (DFS(next, visited, in_path, newest_tid)) {
                    return true;
                }
            }
        }
    }
    
    in_path.erase(current);
    return false;
}

void LockManager::DeleteNode(txn_id_t txn_id) {
    waits_for_.erase(txn_id);

    auto *txn = txn_mgr_->GetTransaction(txn_id);

    for (const auto &row_id: txn->GetSharedLockSet()) {
        for (const auto &lock_req: lock_table_[row_id].req_list_) {
            if (lock_req.granted_ == LockMode::kNone) {
                RemoveEdge(lock_req.txn_id_, txn_id);
            }
        }
    }

    for (const auto &row_id: txn->GetExclusiveLockSet()) {
        for (const auto &lock_req: lock_table_[row_id].req_list_) {
            if (lock_req.granted_ == LockMode::kNone) {
                RemoveEdge(lock_req.txn_id_, txn_id);
            }
        }
    }
}

/**
 * TODO: Student Implement
 */
void LockManager::RunCycleDetection() {
    while (enable_cycle_detection_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(CYCLE_DETECTION_INTERVAL));
        
        {
            std::unique_lock<std::mutex> lock(latch_);
            
            txn_id_t abort_id;
            while (HasCycle(abort_id)) {
                // Abort transaction
                auto txn = txn_mgr_->GetTransaction(abort_id);
                txn->SetState(TransactionState::kAborted);
                
                // Remove from wait-for graph
                DeleteNode(abort_id);
            }
        }
    }
}

/**
 * TODO: Student Implement
 */
std::vector<std::pair<txn_id_t, txn_id_t>> LockManager::GetEdgeList() {
    std::vector<std::pair<txn_id_t, txn_id_t>> edges;
    for (const auto &[from, to_set] : waits_for_) {
        for (txn_id_t to : to_set) {
            edges.emplace_back(from, to);
        }
    }
    return edges;
}
