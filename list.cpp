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
	NODE() : next(nullptr) {}
	NODE(int a) :key(a), next(nullptr) {}
	~NODE() {}
};

class CLIST
{
protected:
	NODE head, tail;
	std::mutex glock;
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
	virtual bool Add(int key)
	{
		NODE* pred, * curr;
		pred = &head;
		glock.lock();
		curr = pred->next;
		while (curr->key < key)
		{
			pred = curr;
			curr = curr->next;
		}

		if (key == curr->key)
		{
			glock.unlock();
			return false;
		}
		else
		{
			NODE* node = new NODE{ key };
			node->next = curr;
			pred->next = node;
			glock.unlock();
			return true;
		}
	}
	virtual bool Remove(int key)
	{
		NODE* pred, * curr;
		pred = &head;
		glock.lock();
		curr = pred->next;
		while (curr->key < key)
		{
			pred = curr;
			curr = curr->next;
		}
		if (key == curr->key)
		{
			pred->next = curr->next;
			delete curr;
			glock.unlock();
			return true;
		}
		else
		{
			glock.unlock();
			return false;
		}
	}
	virtual bool Contains(int key)
	{
		NODE* pred, * curr;
		pred = &head;
		glock.lock();
		curr = pred->next;
		while (curr->key < key)
		{
			pred = curr;
			curr = curr->next;
		}
		if (key == curr->key)
		{
			glock.unlock();
			return true;
		}
		else
		{
			glock.unlock();
			return false;
		}
	}
};
class fine_grainedCLIST :public CLIST
{
public:
	bool Add(int key) override
	{
		NODE* pred, * curr;
		head.mlock.lock();
		pred = &head;

		/****	pred == *head	****/

		curr = pred->next;

		curr->mlock.lock();
		while (curr->key < key)
		{
			pred->mlock.unlock();
			pred = curr;
			curr = curr->next;
			curr->mlock.lock();
		}
		
		if (curr->key == key)
		{
			pred->mlock.unlock();
			curr->mlock.unlock();
			return false;
		}

		NODE* node = new NODE{ key };
		node->next = curr;
		pred->next = node;
		curr->mlock.unlock();
		pred->mlock.unlock();
		return true;
	}
	bool Remove(int key) override
	{
		NODE* pred, * curr;
		head.mlock.lock();
		pred = &head;

		curr = pred->next;

		curr->mlock.lock();
		while (curr->key < key)
		{
			pred->mlock.unlock();
			pred = curr;
			curr = curr->next;
			curr->mlock.lock();
		}

		if (curr->key == key)
		{
			pred->next = curr->next;
			curr->mlock.unlock();
			delete curr;
			pred->mlock.unlock();
			return true;
		}

		curr->mlock.unlock();
		pred->mlock.unlock();
		return false;
	}
	bool Contains(int key) override
	{
		NODE* pred, * curr;
		head.mlock.lock();
		pred = &head;

		curr = pred->next;

		curr->mlock.lock();
		while (curr->key < key)
		{
			pred->mlock.unlock();
			pred = curr;
			curr = curr->next;
			curr->mlock.lock();
		}

		if (curr->key == key)
		{
			curr->mlock.unlock();
			pred->mlock.unlock();
			return true;
		}

		curr->mlock.unlock();
		pred->mlock.unlock();
		return false;
	}
};
class optimisticCLIST :public CLIST
{
private:
	bool validate(NODE* pred, NODE* curr)
	{
		NODE* temp = &head;
		while (temp->key <= pred->key)
		{
			if (temp == pred)
				return pred->next == curr;
			temp = temp->next;
		}
		return 0;
	}
public:
	bool Add(int key) override
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
	bool Remove(int key) override
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
					pred->next = curr->next;
					curr->mlock.unlock();
					//delete curr;
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
	bool Contains(int key) override
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
};

constexpr auto NUM_TEST = 4'000'000;
constexpr auto KEY_RANGE = 1'000;
int NumOfThreads{ 6 };
CLIST clist;
fine_grainedCLIST fCLIST;
optimisticCLIST oCLIST;
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
void fThreadFunc(int num_thread)
{
	int key;
	for (int i = 0; i < NUM_TEST / num_thread; ++i)
	{
		switch (rand() % 3)
		{
		case 0:
			key = rand() % KEY_RANGE;
			fCLIST.Add(key);
			break;
		case 1:
			key = rand() % KEY_RANGE;
			fCLIST.Remove(key);
			break;
		case 2:
			key = rand() % KEY_RANGE;
			fCLIST.Contains(key);
			break;
		default:
			std::cout << "Error\n";
			exit(-1);
		}
	}
}
void oThreadFunc(int num_thread)
{
	int key;
	for (int i = 0; i < NUM_TEST / num_thread; ++i)
	{
		switch (rand() % 3)
		{
		case 0:
			key = rand() % KEY_RANGE;
			oCLIST.Add(key);
			break;
		case 1:
			key = rand() % KEY_RANGE;
			oCLIST.Remove(key);
			break;
		case 2:
			key = rand() % KEY_RANGE;
			oCLIST.Contains(key);
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
		threads.emplace_back(oThreadFunc, i+1);
	for (int i = 0; i < NumOfThreads; ++i)
		threads[i].join();

	auto end_t = std::chrono::high_resolution_clock::now();

	auto elapsed = end_t - start_t;
	
	std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "ms" << std::endl;
}