#pragma once
#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <mutex>
 
#include "log_duration.h"
#include "test_framework.h"
 
template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket {
        std::mutex mutex_;
        std::map<Key, Value> map_;
    };
    
    std::vector<Bucket> bucket_;
    
 public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");
 
    struct Access {
        std::lock_guard<std::mutex> lock_guard_mutex;
        Value& ref_to_value;
 
        Access(const Key& key, Bucket& bucket) : lock_guard_mutex(bucket.mutex_)
                                               , ref_to_value(bucket.map_[key]) {}
    };
 
    explicit ConcurrentMap(size_t bucket_count) : bucket_(bucket_count) {}
 
    Access operator[](const Key& key) {
    
        auto& bucket = bucket_[uint64_t(key) %bucket_.size()];
        return {key, 
                bucket};
    }
 
    std::map<Key, Value> BuildOrdinaryMap() {
    
        std::map<Key, Value> result;
        
        for (auto& [mutex_, map_] : bucket_) {
            std::lock_guard lock_guard_mutex(mutex_);
            result.insert(map_.begin(), map_.end());
        }
        
        return result;
    }
    
    void Erase(const Key& key){    
      auto& bucket = bucket_[static_cast<uint64_t>(key) % bucket_.size()];
        std::lock_guard lock_guard_mutex(bucket.mutex_);
        bucket.map_.erase(key);
    }
};