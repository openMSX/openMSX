#include "catch.hpp"
#include "circular_buffer.hh"
#include "xrange.hh"
#include <memory>
#include <vector>

using namespace std;

static void check_buf(
	const circular_buffer<int>& buf,
	size_t expectedCapacity, const vector<int>& expectedElements)
{
	auto expectedSize = expectedElements.size();
	REQUIRE(buf.size() == expectedSize);
	CHECK(buf.capacity() == expectedCapacity);
	CHECK(buf.reserve() == expectedCapacity - expectedSize);
	CHECK(buf.empty() == (expectedSize == 0));
	CHECK(buf.full()  == (expectedCapacity == expectedSize));

	auto iter_diff = buf.end() - buf.begin();
	CHECK(ptrdiff_t(buf.size()) == iter_diff);

	if (expectedSize != 0) {
		CHECK(buf.front() == expectedElements.front());
		CHECK(buf.back()  == expectedElements.back());
	}

	for (auto i : xrange(expectedSize)) {
		CHECK(buf[i] == expectedElements[i]);
	}

	auto it1 = expectedElements.begin();
	for (auto& i : buf) {
		CHECK(i == *it1++);
	}

	auto rit = expectedElements.rbegin();
	for (auto it = buf.rbegin(); it != buf.rend(); ++it) {
		CHECK(*it == *rit++);
	}
}
TEST_CASE("circular_buffer") {
	circular_buffer<int> buf1;    check_buf(buf1, 0, {});
	buf1.set_capacity(3);         check_buf(buf1, 3, {});
	buf1.push_back(1);            check_buf(buf1, 3, {1});
	buf1.push_back(2);            check_buf(buf1, 3, {1,2});
	buf1.push_front(3);           check_buf(buf1, 3, {3,1,2});
	buf1.pop_front();             check_buf(buf1, 3, {1,2});
	buf1.pop_back();              check_buf(buf1, 3, {1});
	buf1.clear();                 check_buf(buf1, 3, {});

	circular_buffer<int> buf2(5); check_buf(buf2, 5, {});
	buf1.push_back({4,5});        check_buf(buf1, 3, {4,5});
	buf2.push_back({7,8,9});      check_buf(buf2, 5, {7,8,9});

	swap(buf1, buf2);             check_buf(buf1, 5, {7,8,9});
	                              check_buf(buf2, 3, {4,5});
}

static void check_buf(
	const circular_buffer<unique_ptr<int>>& buf,
	size_t expectedCapacity, const vector<int>& expectedElements)
{
	auto expectedSize = expectedElements.size();
	REQUIRE(buf.size() == expectedSize);
	CHECK(buf.capacity() == expectedCapacity);
	CHECK(buf.reserve() == expectedCapacity - expectedSize);
	CHECK(buf.empty() == (expectedSize == 0));
	CHECK(buf.full()  == (expectedCapacity == expectedSize));

	auto iter_diff = buf.end() - buf.begin();
	CHECK(ptrdiff_t(buf.size()) == iter_diff);

	if (expectedSize != 0) {
		CHECK(*buf.front() == expectedElements.front());
		CHECK(*buf.back()  == expectedElements.back());
	}

	for (auto i : xrange(expectedSize)) {
		CHECK(*buf[i] == expectedElements[i]);
	}

	auto it1 = expectedElements.begin();
	for (auto& i : buf) {
		CHECK(*i == *it1++);
	}

	auto rit = expectedElements.rbegin();
	for (auto it = buf.rbegin(); it != buf.rend(); ++it) {
		CHECK(**it == *rit++);
	}
}
TEST_CASE("circular_buffer, move-only") {
	circular_buffer<unique_ptr<int>> buf1; check_buf(buf1, 0, {});
	buf1.set_capacity(3);                  check_buf(buf1, 3, {});
	buf1.push_back (make_unique<int>(1));  check_buf(buf1, 3, {1});
	buf1.push_back (make_unique<int>(2));  check_buf(buf1, 3, {1,2});
	buf1.push_front(make_unique<int>(3));  check_buf(buf1, 3, {3,1,2});
	buf1.pop_front();                      check_buf(buf1, 3, {1,2});
	buf1.pop_back();                       check_buf(buf1, 3, {1});
	buf1.clear();                          check_buf(buf1, 3, {});

	// doesn't work, see
	//    http://stackoverflow.com/questions/8193102/initializer-list-and-move-semantics
	//buf1.push_back({make_unique<int>(4),
	//                make_unique<int>(5)}); check_buf(buf1, 3, {4,5});
}

static void check_queue(
	const cb_queue<int>& q, int expectedCapacity,
	const vector<int>& expectedElements)
{
	check_buf(q.getBuffer(), expectedCapacity, expectedElements);
}
TEST_CASE("cb_queue") {
	cb_queue<int> q;           check_queue(q, 0, {});
	q.push_back(1);            check_queue(q, 4, {1});
	q.push_back(2);            check_queue(q, 4, {1,2});
	CHECK(q.pop_front() == 1); check_queue(q, 4, {2});
	q.push_back({4,5,6,7});    check_queue(q, 8, {2,4,5,6,7});
	CHECK(q.pop_front() == 2); check_queue(q, 8, {4,5,6,7});
	CHECK(q.pop_front() == 4); check_queue(q, 8, {5,6,7});
	q.clear();                 check_queue(q, 8, {});
}

static void check_queue(
	const cb_queue<unique_ptr<int>>& q, int expectedCapacity,
	const vector<int>& expectedElements)
{
	check_buf(q.getBuffer(), expectedCapacity, expectedElements);
}
TEST_CASE("cb_queue, move-only") {
	cb_queue<unique_ptr<int>> q;      check_queue(q, 0, {});
	q.push_back(make_unique<int>(1)); check_queue(q, 4, {1});
	q.push_back(make_unique<int>(2)); check_queue(q, 4, {1,2});
	CHECK(*q.pop_front() == 1);       check_queue(q, 4, {2});
	q.clear();                        check_queue(q, 4, {});
}
