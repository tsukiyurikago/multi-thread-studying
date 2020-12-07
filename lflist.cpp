#include <iostream>
#include <mutex>
#include<vector>
#include<chrono>
#include<thread>
class NODE
{
public:
	std::mutex mlock;
	int key;
	NODE* next;
	bool marked;
	NODE() : next{ nullptr }, marked{ false } {}
	NODE(int a) :key{ a }, next{ nullptr }, marked{ false } {}
	~NODE() {}
};

class CLIST
{
	NODE head, tail;
public:
	CLIST() { head.key = 0x80000000; tail.key = 0x7FFFFFFF; head.next = &tail; }
	~CLIST() {}
	void Init()
	{
		NODE* ptr;
		while (head.next != &tail)
		{
			ptr = head.next;
			head.next = head.next->next;
			delete ptr;
		}
	}
private:
	bool validate(NODE* pred, NODE* curr)
	{
		return !pred->marked && !curr->marked && pred->next == curr;
	}
public:
	bool Add(int key)
	{
		while (1)
		{
			NODE* pred, * curr;
			pred = &head;
			curr = pred->next;
			while (curr->key < key)
			{
				pred = curr;
				curr = curr->next;
			}
			pred->mlock.lock();
			curr->mlock.lock();
			if (validate(pred, curr))
			{
				if (curr->key == key)
				{
					pred->mlock.unlock();
					curr->mlock.unlock();
					return false;
				}
				else
				{
					NODE* temp = new NODE{ key };
					temp->next = curr;
					pred->next = temp;
					pred->mlock.unlock();
					curr->mlock.unlock();
					return true;
				}
			}
			else
			{
				pred->mlock.unlock();
				curr->mlock.unlock();
			}
		}
	}
	bool Remove(int key)
	{
		while (1)
		{
			NODE* pred, * curr;
			pred = &head;
			curr = pred->next;
			while (curr->key < key)
			{
				pred = curr;
				curr = curr->next;
			}

			pred->mlock.lock();
			curr->mlock.lock();

			if (validate(pred, curr))
			{
				if (curr->key == key)
				{
					curr->marked = true;
					pred->next = curr->next;
					curr->mlock.unlock();
					pred->mlock.unlock();
					return true;
				}
				else
				{
					pred->mlock.unlock();
					curr->mlock.unlock();
					return false;
				}
			}
			else
			{
				pred->mlock.unlock();
				curr->mlock.unlock();
			}
		}
	}
	bool Contains(int key)
	{
		NODE* temp = &head;
		while (temp->key < key)
			temp = temp->next;
		return temp->key == key && !temp->marked;
	}
};

constexpr auto NUM_TEST = 4'000'000;
constexpr auto KEY_RANGE = 1'000;
int NumOfThreads{ 6 };
CLIST clist;

void ThreadFunc(int num_thread)
{
	int key;
	for (int i = 0; i < NUM_TEST / num_thread; ++i)
	{
		switch (rand() % 3)
		{
		case 0:
			key = rand() % KEY_RANGE;
			clist.Add(key);
			break;
		case 1:
			key = rand() % KEY_RANGE;
			clist.Remove(key);
			break;
		case 2:
			key = rand() % KEY_RANGE;
			clist.Contains(key);
			break;
		default:
			std::cout << "Error\n";
			exit(-1);
		}
	}
}

int main(void)
{
	std::vector<std::thread> threads;

	auto start_t = std::chrono::high_resolution_clock::now();

	for (int i = 0; i < NumOfThreads; ++i)
		threads.emplace_back(ThreadFunc, i + 1);
	for (int i = 0; i < NumOfThreads; ++i)
		threads[i].join();

	auto end_t = std::chrono::high_resolution_clock::now();

	auto elapsed = end_t - start_t;

	std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "ms" << std::endl;
}