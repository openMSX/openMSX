#include "Thread.hh"
#include "CondVar.hh"
#include <unistd.h>
#include <iostream>

namespace openmsx {

CondVar v1;

class R1 : public Runnable
{
	public:
		void run()
		{
			for (int i=0; i<5; i++) {
				v1.wait();
				cout << "a\n";
			}
		}
};

class R2 : public Runnable
{
	public:
		void run()
		{
			sleep(1);
			for (int i=0; i<5; i++) {
				cout << "b\n";
				v1.signal();
				sleep(1);
			}
		}
};


int main() {
	Runnable *r1 = new R1();
	Runnable *r2 = new R2();
	Thread *t1 = new Thread(r1);
	Thread *t2 = new Thread(r2);
	
	t1->start();
	t2->start();

	sleep(15);

	t1->stop();
	t2->stop();
}

} // namespace openmsx
