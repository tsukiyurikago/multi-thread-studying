#include <iostream>
#include <mutex>
#include<vector>
#include<chrono>
#include<thread>

class NODE;

class MPTR
{
	int value;
public:
	void Set(NODE* node, bool marked)
	//	주소자료형은 결국 int 와 같은 크기의 비트
	//	컴파일러가 자료형 크기를 4의 배수 단위로 할당하여 캐싱 용이, 등등의 이점을 얻는다 -- SIMD 와 비슷한 경우라고 생각중 -- float4개를 묶어서 로드 스토어 한다는 점에서? 32비트4개 - 128비트 , 이건 32비트
	//	bool char 를 4byte 하는건 알겠는데
	//	int 4byte 고, 오른쪽 2bit 사용하지 않는다 -- ??
	//	생각해보자, int를 갖다가 메모리번지수로 쓰겠다 이걸 포인터 자료형으로 써왔다 지금까지
	//	포인터자료형은 4칸 단위로 +- 한다 왜냐? 주로 쓰는게 int고, int 4바이트니까, 그리고 4의 배수 아닌 자료형도 4에 맞춰서 컴파일 (패킷 크기 줄이려고 프라그마 패킹 같은거 써야 실크기로 컴파일하겠다)
	//	그러니까 포인터자료형은 항상 비트2자리가 00 이겠다 ㅇㅎ
	{
		value = reinterpret_cast<int>(node);
		if (value)
			//	xxxx xxxx .... xxxx 과 0000 0000 .... 0001 을 OR 하자
			value = value | 0x01;
		else
			value = value & 0xFFFFFFFE;
	}

	NODE* Getptr(void)
	{
		return reinterpret_cast<NODE*>(value & 0xFFFFFFFE);
	}

	NODE* Getptr(bool* marked)
	{
		int temp = value;
		if (0 == (temp & 0x1))
			*marked = false;
		else
			*marked = true;
		return reinterpret_cast<NODE*>(temp & 0xFFFFFFFE);
	}
	bool CAS(NODE* old_node, NODE* new_node, bool old_marked, bool new_marked)
	{
		int old_value, new_value;

		old_value = reinterpret_cast<int>(old_node);

	}
};

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