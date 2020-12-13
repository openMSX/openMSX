#include "catch.hpp"

#include "ObjectPool.hh"
#include "xrange.hh"
#include <vector>


struct Tracked
{
	static inline std::vector<int> constructed;
	static inline std::vector<int> destructed;

	Tracked(int i_) : i(i_) {
		constructed.push_back(i);
	}
	~Tracked() {
		destructed.push_back(i);
	}
	int i;
};

TEST_CASE("ObjectPool")
{
	REQUIRE(Tracked::constructed.empty());
	REQUIRE(Tracked::destructed.empty());

	{
		ObjectPool<Tracked> pool;
		CHECK(pool.capacity() == 0); // empty pool does no memory allocation

		// insert '10'
		auto [idx1, ptr1] = pool.emplace(10);
		CHECK(ptr1->i == 10);
		CHECK(&pool[idx1] == ptr1);
		CHECK(Tracked::constructed == std::vector{10});
		CHECK(Tracked::destructed  == std::vector<int>{});
		CHECK(pool.capacity() == 256); // allocates in chunks of 256

		// insert '20'
		auto [idx2, ptr2] = pool.emplace(20);
		CHECK(ptr1->i == 10);
		CHECK(ptr2->i == 20);
		CHECK(&pool[idx1] == ptr1);
		CHECK(&pool[idx2] == ptr2);
		CHECK(Tracked::constructed == std::vector{10, 20});
		CHECK(Tracked::destructed  == std::vector<int>{});

		// insert '30'
		auto [idx3, ptr3] = pool.emplace(30);
		CHECK(ptr1->i == 10);
		CHECK(ptr2->i == 20);
		CHECK(ptr3->i == 30);
		CHECK(&pool[idx1] == ptr1);
		CHECK(&pool[idx2] == ptr2);
		CHECK(&pool[idx3] == ptr3);
		CHECK(Tracked::constructed == std::vector{10, 20, 30});
		CHECK(Tracked::destructed  == std::vector<int>{});

		// remove '20'
		pool.remove(idx2);
		CHECK(ptr1->i == 10);
		CHECK(ptr3->i == 30);
		CHECK(&pool[idx1] == ptr1);
		CHECK(&pool[idx3] == ptr3);
		CHECK(Tracked::constructed == std::vector{10, 20, 30});
		CHECK(Tracked::destructed  == std::vector{20});

		// insert '40'
		auto [idx4, ptr4] = pool.emplace(40);
		CHECK(idx4 == idx2); // recycled
		CHECK(ptr1->i == 10);
		CHECK(ptr3->i == 30);
		CHECK(ptr4->i == 40);
		CHECK(&pool[idx1] == ptr1);
		CHECK(&pool[idx3] == ptr3);
		CHECK(&pool[idx4] == ptr4);
		CHECK(Tracked::constructed == std::vector{10, 20, 30, 40});
		CHECK(Tracked::destructed  == std::vector{20});
		CHECK(pool.capacity() == 256);

		// insert a lot more (force allocating more memory in the pool)
		for (auto i : xrange(1000)) {
			auto val = 1000 + i;
			auto [idx, ptr] = pool.emplace(val);
			CHECK(ptr->i == val);
			CHECK(&pool[idx] == ptr);
		}
		CHECK(Tracked::constructed.size() == 1004);
		CHECK(Tracked::destructed == std::vector{20});
		CHECK(pool.capacity() == 1024);
	}

	// When 'pool' goes out of scope it does _not_ destruct the elements it
	// still contained. (But it does free the memory it used to store those
	// elements.)
	CHECK(Tracked::constructed.size() == 1004);
	CHECK(Tracked::destructed == std::vector{20});
}


// To (manually) inspect quality of generated code
#if 0

auto inspect_operator(ObjectPool<int>& pool, unsigned idx)
{
	return pool[idx];
}

auto inspect_emplace(ObjectPool<int>& pool, int val)
{
	return pool.emplace(val);
}

void inspect_remove(ObjectPool<int>& pool, unsigned idx)
{
	return pool.remove(idx);
}

#endif
