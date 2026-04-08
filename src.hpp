#ifndef SRC_HPP
#define SRC_HPP

#include <cstddef>

/**
 * 枚举类，用于枚举可能的置换策略
 */
enum class ReplacementPolicy { kDEFAULT = 0, kFIFO, kLRU, kMRU, kLRU_K };

/**
 * @brief 该类用于维护每一个页对应的信息以及其访问历史，用于在尝试置换时查询需要的信息。
 */
class PageNode {
public:
  PageNode(std::size_t id, std::size_t k, std::size_t time) : page_id_(id), k_(k) {
    add_time_ = time;
    last_access_time_ = time;
    first_access_time_ = time;
    if (k_ > 0) {
      access_times_ = new std::size_t[k_];
      access_times_[0] = time;
    } else {
      access_times_ = nullptr;
    }
    access_count_ = 1;
    head_ = 0;
    prev_ = nullptr;
    next_ = nullptr;
  }

  ~PageNode() {
    if (access_times_) {
      delete[] access_times_;
    }
  }

  void Access(std::size_t time) {
    last_access_time_ = time;
    if (k_ > 0) {
      if (access_count_ < k_) {
        access_times_[access_count_] = time;
        access_count_++;
      } else {
        access_times_[head_] = time;
        head_ = (head_ + 1) % k_;
      }
    } else {
      access_count_++;
    }
  }

  std::size_t GetKthAccessTime() const {
    if (k_ == 0) return 0;
    if (access_count_ < k_) return 0;
    return access_times_[head_];
  }

  std::size_t page_id_;
  std::size_t k_;
  std::size_t add_time_;
  std::size_t last_access_time_;
  std::size_t first_access_time_;
  std::size_t* access_times_;
  std::size_t access_count_;
  std::size_t head_;
  
  PageNode* prev_;
  PageNode* next_;
};

class ReplacementManager {
public:
  constexpr static std::size_t npos = -1;

  ReplacementManager() = delete;

  /**
   * @brief 初始化整个类
   * @param max_size 缓存池可以容纳的页数量的上限
   * @param k LRU-K所基于的常数k，在类销毁前不会变更
   * @param default_policy 在置换时，如果没有显式指示，则默认使用default_policy作为策略
   * @note 我们将保证default_policy的值不是ReplacementPolicy::kDEFAULT。
   */
  ReplacementManager(std::size_t max_size, std::size_t k, ReplacementPolicy default_policy) 
      : max_size_(max_size), k_(k), default_policy_(default_policy), size_(0), time_(0), head_(nullptr), tail_(nullptr) {
  }

  /**
   * @brief 析构函数
   * @note 我们将对代码进行Valgrind Memcheck，请保证你的代码不发生内存泄漏
   */
  ~ReplacementManager() {
    PageNode* curr = head_;
    while (curr != nullptr) {
      PageNode* next = curr->next_;
      delete curr;
      curr = next;
    }
  }

  /**
   * @brief 重设当前默认的缓存置换政策
   * @param default_policy 新的默认政策，保证default_policy不是ReplacementPolicy::kDEFAULT
   */
  void SwitchDefaultPolicy(ReplacementPolicy default_policy) {
    default_policy_ = default_policy;
  }

  /**
   * @brief 访问某个页面。
   * @param page_id 访问页的编号
   * @param evict_id 需要被置换的页编号，如果不需要置换请将其设置为npos
   * @param policy 如果需要置换，那么置换所基于的策略
   */
  void Visit(std::size_t page_id, std::size_t &evict_id, ReplacementPolicy policy = ReplacementPolicy::kDEFAULT) {
    time_++;
    if (policy == ReplacementPolicy::kDEFAULT) {
      policy = default_policy_;
    }

    PageNode* node = FindPage(page_id);
    if (node != nullptr) {
      // Page is in cache
      node->Access(time_);
      evict_id = npos;
    } else {
      // Page not in cache
      if (size_ == max_size_) {
        // Cache full, evict
        evict_id = TryEvict(policy);
        if (evict_id != npos) {
          RemovePage(evict_id);
        }
      } else {
        evict_id = npos;
      }
      // Add new page
      if (size_ < max_size_) {
        PageNode* new_node = new PageNode(page_id, k_, time_);
        AddNode(new_node);
      }
    }
  }

  /**
   * @brief 强制地删除特定的页（无论缓存池是否已满）
   * @param page_id 被删除页的编号
   * @return 如果成功删除，则返回true; 如果该页不存在于缓存池中，则返回false
   */
  bool RemovePage(std::size_t page_id) {
    PageNode* node = FindPage(page_id);
    if (node == nullptr) return false;
    
    if (node->prev_) node->prev_->next_ = node->next_;
    else head_ = node->next_;
    
    if (node->next_) node->next_->prev_ = node->prev_;
    else tail_ = node->prev_;
    
    delete node;
    size_--;
    return true;
  }

  /**
   * @brief 查询特定策略下首先被置换的页
   * @param policy 置换策略
   * @return 当前策略下会被置换的页的编号。若缓存池没满，则返回npos
   */
  [[nodiscard]] std::size_t TryEvict(ReplacementPolicy policy = ReplacementPolicy::kDEFAULT) const {
    if (size_ < max_size_) return npos;
    if (size_ == 0) return npos;

    if (policy == ReplacementPolicy::kDEFAULT) {
      policy = default_policy_;
    }

    PageNode* best_node = nullptr;
    PageNode* curr = head_;

    while (curr != nullptr) {
      if (best_node == nullptr) {
        best_node = curr;
      } else {
        if (policy == ReplacementPolicy::kFIFO) {
          if (curr->add_time_ < best_node->add_time_) best_node = curr;
        } else if (policy == ReplacementPolicy::kLRU) {
          if (curr->last_access_time_ < best_node->last_access_time_) best_node = curr;
        } else if (policy == ReplacementPolicy::kMRU) {
          if (curr->last_access_time_ > best_node->last_access_time_) best_node = curr;
        } else if (policy == ReplacementPolicy::kLRU_K) {
          bool curr_less_k = curr->access_count_ < k_;
          bool best_less_k = best_node->access_count_ < k_;
          if (curr_less_k && !best_less_k) {
            best_node = curr;
          } else if (!curr_less_k && best_less_k) {
            // best_node is better
          } else if (curr_less_k && best_less_k) {
            if (curr->first_access_time_ < best_node->first_access_time_) best_node = curr;
          } else {
            if (curr->GetKthAccessTime() < best_node->GetKthAccessTime()) best_node = curr;
          }
        }
      }
      curr = curr->next_;
    }

    return best_node ? best_node->page_id_ : npos;
  }

  /**
   * @brief 返回当前缓存管理器是否为空。
   */
  [[nodiscard]] bool Empty() const {
    return size_ == 0;
  }

  /**
   * @brief 返回当前缓存管理器是否已满（即是否页数量已经达到上限）
   */
  [[nodiscard]] bool Full() const {
    return size_ == max_size_;
  }

  /**
   * @brief 返回当前缓存管理器中页的数量
   */
  [[nodiscard]] std::size_t Size() const {
    return size_;
  }

private:
  PageNode* FindPage(std::size_t page_id) const {
    PageNode* curr = head_;
    while (curr != nullptr) {
      if (curr->page_id_ == page_id) return curr;
      curr = curr->next_;
    }
    return nullptr;
  }

  void AddNode(PageNode* node) {
    if (head_ == nullptr) {
      head_ = tail_ = node;
    } else {
      tail_->next_ = node;
      node->prev_ = tail_;
      tail_ = node;
    }
    size_++;
  }

  std::size_t max_size_;
  std::size_t k_;
  ReplacementPolicy default_policy_;
  std::size_t size_;
  std::size_t time_;
  PageNode* head_;
  PageNode* tail_;
};

#endif
