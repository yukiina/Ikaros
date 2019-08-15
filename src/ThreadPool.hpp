#pragma once
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <future>

using std::vector;
using std::queue;
using std::condition_variable;
using std::mutex;
using std::thread;

class ThreadPool {
public:
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
	typedef std::function<void()> Task;
	explicit ThreadPool(size_t threads = thread::hardware_concurrency());
	~ThreadPool();
	template<class F, class...Args>
	auto AddTask(F&& f, Args&& ...args)->std::future<typename std::result_of< F(Args...)>::type>;
private:
	vector<thread> _workes;
	queue<Task> _tasks;
	condition_variable _cond;
	mutex _mutex;
	bool _stop;
};




template<class F, class ...Args>
inline auto ThreadPool::AddTask(F&& f, Args&& ...args) -> std::future<typename std::result_of<F(Args ...)>::type>
{
	using return_type = typename std::result_of<F(Args ...)>::type;

	auto task = std::make_shared<std::packaged_task<return_type()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);

	std::future<return_type> res = task->get_future();

	{
		std::unique_lock<mutex> ulc(_mutex);
		if (_stop)
			throw std::runtime_error("线程池已经停止运行!!!");
		_tasks.emplace([task]() {(*task)(); });
	}
	_cond.notify_one();
	return res;

}


ThreadPool::ThreadPool(size_t threads) :_stop(false) {
	for (size_t i = 0; i < threads; i++) {
		_workes.emplace_back([this]() {
			for (;;) {
				Task task;
				{
					std::unique_lock<mutex> ulc(_mutex);
					_cond.wait(ulc, [this]() {return _stop || !_tasks.empty(); });

					if (_stop && _tasks.empty())
						return;

					task = std::move(_tasks.front());
					_tasks.pop();
				}
				task();
			}

			});
	}
}

ThreadPool::~ThreadPool() {
	{
		std::unique_lock<mutex> ulc(_mutex);
		_stop = true;
	}
	_cond.notify_all();

	for (auto& thread : _workes) {
		thread.join();
	}
}