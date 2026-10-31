#pragma once
#include "external/atomic_queue/atomic_queue.h"
#include "std_allocator.h"

#include <cstddef>
#include <cstdint>

template <class T>
using ParallelQueueAllocator = GodotStdAllocator<T>;

// This is here to enforce the alignment of the queue without enforcing it on types that include it.
template <class QueueType>
class AlignedParallelQueue {
	alignas(std::max_align_t) unsigned char storage[sizeof(QueueType) + alignof(QueueType)] = {};
	QueueType *queue = nullptr;

	void *_aligned_storage() noexcept {
		uintptr_t addr = reinterpret_cast<uintptr_t>(storage);
		uintptr_t aligned = (addr + alignof(QueueType) - 1) & ~(alignof(QueueType) - 1);
		return reinterpret_cast<void *>(aligned);
	}

public:
	using value_type = typename QueueType::value_type;

	template <typename... Args>
	explicit AlignedParallelQueue(Args &&...p_args) :
			queue(::new (_aligned_storage()) QueueType(std::forward<Args>(p_args)...)) {}

	AlignedParallelQueue(const AlignedParallelQueue &) = delete;
	AlignedParallelQueue &operator=(const AlignedParallelQueue &) = delete;
	AlignedParallelQueue(AlignedParallelQueue &&) = delete;
	AlignedParallelQueue &operator=(AlignedParallelQueue &&) = delete;

	~AlignedParallelQueue() noexcept {
		if (queue) {
			queue->~QueueType();
		}
	}

	template <class U>
	_ALWAYS_INLINE_ bool try_push(U &&p_element) noexcept { return queue->try_push(std::forward<U>(p_element)); }
	template <class U>
	_ALWAYS_INLINE_ bool try_pop(U &r_element) noexcept { return queue->try_pop(r_element); }
	template <class U>
	_ALWAYS_INLINE_ void push(U &&p_element) noexcept { queue->push(std::forward<U>(p_element)); }
	_ALWAYS_INLINE_ auto pop() noexcept { return queue->pop(); }
	_ALWAYS_INLINE_ unsigned was_size() const noexcept { return queue->was_size(); }
	_ALWAYS_INLINE_ bool was_empty() const noexcept { return queue->was_empty(); }
	_ALWAYS_INLINE_ bool was_full() const noexcept { return queue->was_full(); }
	_ALWAYS_INLINE_ unsigned capacity() const noexcept { return queue->capacity(); }
};

template <class T, unsigned SIZE, bool MINIMIZE_CONTENTION = true, bool MAXIMIZE_THROUGHPUT = true, bool TOTAL_ORDER = false, bool SPSC = false>
using StaticParallelQueue = AlignedParallelQueue<atomic_queue::AtomicQueue2<T, SIZE, MINIMIZE_CONTENTION, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>>;

template <class T, class A = ParallelQueueAllocator<T>, bool MAXIMIZE_THROUGHPUT = true, bool TOTAL_ORDER = false, bool SPSC = false>
using ParalellQueue = AlignedParallelQueue<atomic_queue::AtomicQueueB2<T, A, MAXIMIZE_THROUGHPUT, TOTAL_ORDER, SPSC>>;
