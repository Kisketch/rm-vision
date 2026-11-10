#ifndef TOOLS__THREAD_SAFE_QUEUE_HPP
#define TOOLS__THREAD_SAFE_QUEUE_HPP

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>

namespace tools
{
// T：队列中存储的数据类型，PopWhenFull：队列满时的处理策略（默认为false）
template <typename T, bool PopWhenFull = false>
class ThreadSafeQueue
{
public:
  // 初始化
  ThreadSafeQueue(size_t max_size, std::function<void(void)> full_handler = [] {})
  : max_size_(max_size), full_handler_(full_handler){}

  // 入队操作（生产者线程）
  void push(const T & value)
  {
    std::unique_lock<std::mutex> lock(mutex_);

    if (queue_.size() >= max_size_) {
      if (PopWhenFull) {
        queue_.pop();
      } else {
        full_handler_();
        return;
      }
    }
    queue_.push(value);
    // 唤醒所有消费者线程
    not_empty_condition_.notify_all();
  }

  // 消费者线程，取队列中第一个对象
  void pop(T & value)
  {
    std::unique_lock<std::mutex> lock(mutex_);

    not_empty_condition_.wait(lock, [this] { return !queue_.empty(); });

    if (queue_.empty()) {
      std::cerr << "Error: Attempt to pop from an empty queue." << std::endl;
      return;
    }
    // 移动构造：转换所属权，会导致第一个对象为空，需要配合pop
    value = std::move(queue_.front());   
    queue_.pop();
  }

  // 消费者线程，取队列中第一个对象
  T pop()
  {
    std::unique_lock<std::mutex> lock(mutex_);

    not_empty_condition_.wait(lock, [this] { return !queue_.empty(); });

    T value = std::move(queue_.front());
    queue_.pop();
    return std::move(value);          
  }

  // 查看队首元素，不消费
  T front()
  {
    std::unique_lock<std::mutex> lock(mutex_);

    not_empty_condition_.wait(lock, [this] { return !queue_.empty(); });

    return queue_.front();
  }

  // 查看队尾元素
  void back(T & value) 
  {
    std::unique_lock<std::mutex> lock(mutex_);

    if (queue_.empty()) {
      std::cerr << "Error: Attempt to access the back of an empty queue." << std::endl;
      return;
    }

    value = queue_.back();
  }

  // 查看队列是否为空
  bool empty()
  {
    std::unique_lock<std::mutex> lock(mutex_);
    return queue_.empty();
  }

  // 清空队列
  void clear()
  {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
      queue_.pop();
    }
    not_empty_condition_.notify_all();  // 如果其他线程正在等待队列不为空，这样可以唤醒它们
  }

private:
  std::queue<T> queue_;                                    // 数据缓冲队列
  size_t max_size_;                                        // 队列容量
  mutable std::mutex mutex_;                               // 互斥锁， mutable允许在const方法中加锁
  std::condition_variable not_empty_condition_;
  std::function<void(void)> full_handler_;                 // 初始化，max_size队列，full_handler回调函数
};

}  // namespace tools

#endif  // TOOLS__THREAD_SAFE_QUEUE_HPP