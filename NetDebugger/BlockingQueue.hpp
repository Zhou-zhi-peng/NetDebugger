#pragma once
#include <queue>
#include <vector>
#include <map>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <functional>

template <class T>
class BlockingQueue
{
public:
	BlockingQueue(size_t max_queue = 0)
		:m_max_queue(max_queue),
		m_quit(false)
	{
	}

	~BlockingQueue() = default;

	void Push(const T& element)
	{
		{
			std::unique_lock<std::mutex> lock(m_Mutex);
			if (m_max_queue != 0)
			{
				while (m_elements.size() > m_max_queue)
				{
					if (m_quit)
					{
						m_elements.push(std::move(element));
						return;
					}
					m_elements_nonfull.wait(lock);
				}
			}
			m_elements.push(std::move(element));
		}
		m_elements_nonempty.notify_one();
	}

	bool TryPop(T& element)
	{
		{
			std::unique_lock<std::mutex> lock(m_Mutex);
			if (m_elements.empty())
			{
				return false;
			}
			element = std::move(m_elements.front());
			m_elements.pop();
		}
		m_elements_nonfull.notify_one();
		return true;
	}

	T WaitForPop(void)
	{
		T result;
		{
			std::unique_lock<std::mutex> lock(m_Mutex);
			while (m_elements.empty() && !m_quit)
			{
				m_elements_nonempty.wait(lock);
			}

			if (m_quit)
			{
				return result;
			}

			result = std::move(m_elements.front());
			m_elements.pop();
		}
		m_elements_nonfull.notify_one();
		return result;
	}

	void PopAll(std::queue<T>& result)
	{
		std::queue<T> empty;
		result.swap(empty);
		{
			{
				std::unique_lock<std::mutex> lock(m_Mutex);
				result.swap(m_elements);
			}
			m_elements_nonfull.notify_all();
		}
	}

	void Clear()
	{
		std::queue<T> empty;
		std::unique_lock<std::mutex> lock(m_Mutex);
		m_elements.swap(empty);
		m_elements_nonfull.notify_all();
	}

	void Stop()
	{
		{
			std::unique_lock<std::mutex> lock(m_Mutex);
			m_quit = true;
		}
		m_elements_nonempty.notify_all();
		std::queue<T> empty;
		PopAll(empty);
	}

	bool IsStoped(void)const { return m_quit; }
private:
	std::mutex m_Mutex;
	std::condition_variable m_elements_nonempty;
	std::condition_variable m_elements_nonfull;
	size_t m_max_queue;
	bool m_quit;
	std::queue<T> m_elements;
};