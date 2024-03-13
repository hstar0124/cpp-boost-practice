#include <iostream>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind/bind.hpp>

void printA(const boost::system::error_code& /*e*/)
{
	std::cout << "Hello, boost async World!" << std::endl;
}

void printB(const boost::system::error_code& /*e*/,	boost::asio::steady_timer* t, int* count)
{
	/*
		타이머가 여섯 번째 발생될 때 실행을 중지하기 위해 카운터를 사용한다. 
		그러나 io_context를 중지하도록 요청하는 명시적인 호출이 없다는 것을 알게 될 것이다. 
		튜토리얼 Timer.2에서 io_context::run() 함수가 더 이상 "work(작업)"이 없을 때 완료된다는 것을 기억할 것이다. 
		count가 5에 도달하면 타이머에서 더 이상 신규 비동기 대기를 시작하지 않으면, 
		io_context는 작업이 부족해지고 실행을 중지할 것이다.
	*/
	if (*count < 5)
	{
		std::cout << *count << std::endl;
		++(*count);
		/*
			다음으로 타이머의 만료 시간을 이전 만료 시간에서 일초만큼 이동한다. (이전 만료 시간에서 일초를 더한다.) 
			이전과 비교하여 새로운 만료 시간을 계산함으로써, 
			핸들러 처리 지연으로 인해 타이머가 전체-초 표시에서 이탈하지 않도록 할 수 있다.
		*/
		t->expires_at(t->expiry() + boost::asio::chrono::seconds(1));
		t->async_wait(boost::bind(printB, boost::asio::placeholders::error, t, count));
	}
}

class PrinterForSingleThread
{
public:
	// 이 클래스의 생성자는 io_context의 객체에 대한 참조를 가져와서 _timer 멤버를 초기화할 때 사용한다. 
	// 프로그램을 종료하는 데 사용되는 카운터 역시 이제는 클래스 멤버이다.
	PrinterForSingleThread(boost::asio::io_context& io)
		:_timer(io, boost::asio::chrono::seconds(1)), _count(0)
	{
		_timer.async_wait(boost::bind(&PrinterForSingleThread::print, this));
	}
	~PrinterForSingleThread()
	{
		std::cout << "Final count is " << _count << std::endl;
	}
	void print()
	{
		if (_count < 5)
		{
			std::cout << _count << std::endl;
			++_count;

			_timer.expires_at(_timer.expiry() + boost::asio::chrono::seconds(1));
			_timer.async_wait(boost::bind(&PrinterForSingleThread::print, this));
		}
	}
private:
	boost::asio::steady_timer _timer;
	int _count;
};

class PrinterForMultiThread
{
public:
	/*
		스트랜드(strand) 클래스 템플릿은, 이 템플릿을 통해 전달된(dispatched) 실행 중인 핸들러가 
		다음 핸들러가 시작되기 전에 완료될 수 있도록 보장하는 실행기(executor) 어댑터(adapter)이다. 
		이것은 io_context::run()을 호출하는 스레드의 수에 관계없이 보장된다. 
		물론, 핸들러는 스트랜드(strand)를 통해 전달되지(dispatched) 않거나 
		다른 스트랜드(strand) 개체를 통해 전달된(dispatched) 다른 핸들러와 여전히 동시에 실행할 수 있다.
	*/
	PrinterForMultiThread(boost::asio::io_context& io)
		: _strand(boost::asio::make_strand(io)),
		_timer1(io, boost::asio::chrono::seconds(1)),
		_timer2(io, boost::asio::chrono::seconds(1)),
		_count(0)
	{
		/*
			비동기 작업을 시작할 때, 각 콜백 핸들러는 boost::asio::strand<boost::asio::io_context::executor_type> 개체에 "bound(바인드)" 된다. 
			boost::asio::bind_executor() 함수는, 함수에 포함된 핸들러를 스트랜드(strand) 개체를 통해 자동으로 전달하는(dispatches) 새로운 핸들러를 반환한다. 
			핸들러를 같은 스트랜드(strand)에 바인딩하여, 동시에 실행할 수 없도록 한다.
		*/
		_timer1.async_wait(boost::asio::bind_executor(_strand, boost::bind(&PrinterForMultiThread::print1, this)));
		_timer2.async_wait(boost::asio::bind_executor(_strand, boost::bind(&PrinterForMultiThread::print2, this)));
	}

	~PrinterForMultiThread()
	{
		std::cout << "Final count is " << _count << std::endl;
	}

	void print1()
	{
		if (_count < 10)
		{
			std::cout << "Timer 1: " << _count << std::endl;
			++_count;

			_timer1.expires_at(_timer1.expiry() + boost::asio::chrono::seconds(1));

			_timer1.async_wait(boost::asio::bind_executor(_strand,
				boost::bind(&PrinterForMultiThread::print1, this)));
		}
	}

	void print2()
	{
		if (_count < 10)
		{
			std::cout << "Timer 2: " << _count << std::endl;
			++_count;

			_timer2.expires_at(_timer2.expiry() + boost::asio::chrono::seconds(1));

			_timer2.async_wait(boost::asio::bind_executor(_strand,
				boost::bind(&PrinterForMultiThread::print2, this)));
		}
	}

private:
	boost::asio::strand<boost::asio::io_context::executor_type> _strand;
	boost::asio::steady_timer _timer1;
	boost::asio::steady_timer _timer2;
	int _count;
};


int main()
{
	/////////////////////////////////////////////////////////////////////
	// Timer.1 - 타이머를 동기식으로 사용 (Using a timer synchronously)
	/////////////////////////////////////////////////////////////////////
	{

		/*
			asio를 사용하는 모든 프로그램에는 io_context 또는 thread_pool 개체와 같은 I/O 실행 컨텍스트가 하나 이상 있어야 한다.
			I/O 실행 컨텍스트는 I/O 기능에 대한 접근을 제공한다.
			main 함수에서 io_context 유형의 개체를 먼저 선언한다.
		*/
		boost::asio::io_context io;

		/*
			다음으로 boost::asio::steady_timer 유형의 개체를 선언한다.
			I/O 기능(또는 이 경우 타이머 기능)을 제공하는 핵심 asio 클래스의 생성자는 io_context를 첫 번째 인수로 사용한다.
			생성자에 대한 두 번째 인수는 타이머가 지금부터 5초 후에 만료되도록 설정한다.
		*/
		boost::asio::steady_timer t(io, boost::asio::chrono::seconds(5));

		/*
			여기서 블록된 상태로 대기를 수행하는데, steady_time::wait()에 대한 호출은 타이머가 생성된 후
			5초가 지나 만료될 때까지 반환되지 않는다.
		*/
		t.wait();

		std::cout << "Hello, boost sync World!" << std::endl;
	}


	/////////////////////////////////////////////////////////////////////
	//Timer.2 타이머를 비동기식으로 사용 (Using a timer asynchronously)
	/////////////////////////////////////////////////////////////////////
	{
		boost::asio::io_context io;
		boost::asio::steady_timer t(io, boost::asio::chrono::seconds(5));
		/*
			asio의 비동기 기능을 사용한다는 것은 비동기 작업 완료 후 호출되는 콜백 함수를 갖는다는 것을 의미한다.
			이 프로그램에서는 비동기 대기가 끝나면 호출되는 print라는 함수를 정의한다.
			비동기 대기를 수행하기 위해 steady_timer::async_wait() 함수를 호출한다.
			이 함수를 호출할 때 위에서 정의한 print 콜백 핸들러를 인수로 전달한다.
		*/
		t.async_wait(&printA);

		/*
			마지막으로 io_context 개체에서 io_context::run() 멤버 함수를 호출해야 한다.
			asio 라이브러리는 현재 io_context::run()을 호출한 스레드에서만 콜백 핸들러가 호출되도록 보장한다.
			따라서 io_context::run() 함수는 비동기 대기 완료를 위한 콜백이 호출되지 않는 한 결코 호출되지 않는다.
			io_context::run() 함수는 "work(작업)"이 있는 동안 계속 실행된다.
			이 예제에서, 작업은 타이머에서 비동기 대기이므로 타이머가 만료되거나 콜백이 완료될 때까지 반환되지 않는다.
			io_context::run() 호출 전, io_context에 해야할 일을 제공하는 것이 중요하다.
			예를 들어, 위에서 steady_timer::async_wait()에 대한 호출을 생략한다면 io_context는 할 일이 없었을 것이고,
			결과적으로 io_context::run()은 즉시 반환할 것이다.
		*/
		io.run();
	}

	/////////////////////////////////////////////////////////////////////
	//Timer.3 - 핸들러에 대한 바인딩 인수 (Binding arguments to a handler)
	/////////////////////////////////////////////////////////////////////
	{
		/*
			asio를 사용하여 반복 타이머를 구현하려면, 
			콜백 함수에서 타이머의 만료 시간을 변경한 다음에 신규 비동기 대기를 시작해야 한다. 
			분명히 이것은 콜백 함수가 타이머 개체에 접근할 수 있어야 한다는 것을 의미한다. 
			이를 위해 print 함수에 두 개의 신규 파라미터를 추가한다.

			- 타이머 개체에 대한 포인터
			- 타이머가 여섯 번째 발생될 때 프로그램을 중지할 수 있는 카운터
		*/
		boost::asio::io_context io;
		int count = 0;
		boost::asio::steady_timer t(io, boost::asio::chrono::seconds(1));

		t.async_wait(boost::bind(printB, boost::asio::placeholders::error, &t, &count));

		io.run();

		std::cout << "Final count is " << count << std::endl;
	}


	/////////////////////////////////////////////////////////////////////
	//Timer.4 - 핸들러로 멤버 함수를 사용 (Using a member function as a handler)
	/////////////////////////////////////////////////////////////////////
	{
		/*
			print를 콜백 핸들러로 정의하는 대신에 이제는 Printer라는 클래스를 정의한다.
			
		*/
		boost::asio::io_context io;
		PrinterForSingleThread p(io);
		io.run();
	}


	/////////////////////////////////////////////////////////////////////
	//Timer.5 - 멀티스레드 프로그램에서 핸들러 동기화 (Synchronising handlers in multithreaded programs)
	/////////////////////////////////////////////////////////////////////
	{
		/*
			단일 스레드 접근 방식은 asio를 사용하여 응용프로그램을 개발할 때 일반적으로 시작하기에 좋은 방법이다. 
			단점은 프로그램, 특히 서버에 미치는 다음과 같은 제한 사항이다:

			- 핸들러를 완료하는데 오랜 시간이 걸릴 수 있는 경우 응답성이 떨어진다.
			- 멀티 프로세서 시스템에서 확장할 수 없다.
			
			이러한 제한 사항에 직면한 경우, 다른 방법은 io_context::run()을 호출하는 스레드 풀을 갖는 것이다. 
			그러나 이렇게 하면 핸들러가 동시에 실행될 수 있으므로 핸들러가 공유된 리소스 또는 스레드에 
			안전하지 않은 리소스에 접근할 수 있을 때 동기화 방법이 필요하다.
		*/
		boost::asio::io_context io;
		PrinterForMultiThread p(io);
		boost::thread t(boost::bind(&boost::asio::io_context::run, &io));
		io.run();
		t.join();
	}
}
