
#include <ctime>
#include <iostream>
#include <string>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

std::string make_daytime_string()
{
    using namespace std; // For time_t, time and ctime;
    time_t now = time(0);
    return ctime(&now);
}

// tcp_connection 개체를 참조하는 작업이 있는 동안, 
// tcp_connection 개체가 활성(active) 상태를 유지하도록 
// share_ptr 및 enable_shared_from_this를 사용할 수 있다.
class tcp_connection
    : public boost::enable_shared_from_this<tcp_connection>
{
public:
    typedef boost::shared_ptr<tcp_connection> pointer;

    static pointer create(boost::asio::io_context& io_context)
    {
        return pointer(new tcp_connection(io_context));
    }

    tcp::socket& socket()
    {
        return _socket;
    }

    // start() 함수에서, boost::asio::async_write()를 호출하여 클라이언트에 데이터를 제공한다. 
    // 전체 데이터 블록이 전송되도록 ip::tcp::socket::async_write_some() 대신에 boost::asio::async_write()를 사용한다.
    void start()
    {
        // 비동기 작업이 완료될 때까지 데이터가 유효한 상태를 유지할 수 있도록, 보낼 데이터는 message_ 클래스 멤버에 저장된다.
        _message = make_daytime_string();

        /*
            비동기 작업을 시작할 때 boost::bind()를 사용하려면, 
            핸들러의 파라미터 목록과 일치하는 인수만 지정해야 한다. 
            이 프로그램에서 두 개의 인수 대체자(placeholder)는
            (boost::asio::placeholders::error 및 boost::asio::placeholders::bytes_transferred) handle_write()에서 
            사용되지 않기 때문에 잠재적으로 제거될 수 있다.
        */
        boost::asio::async_write(_socket, boost::asio::buffer(_message),
            boost::bind(&tcp_connection::handle_write, shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

private:
    tcp_connection(boost::asio::io_context& io_context)
        : _socket(io_context)
    {
    }

    /*
        error 및 bytes_transferred 파라미터가 handler_write() 함수의 본문에서 사용되지 않는다는 것을 알 수 있을 것이다. 
        파라미터가 필요하지 않은 경우, 다음과 같이 함수에서 파라미터를 제거할 수 있다.
    */
    void handle_write(const boost::system::error_code& /*error*/,
        size_t /*bytes_transferred*/)
    {
    }

    tcp::socket _socket;
    std::string _message;
};

class tcp_server
{
public:
    //생성자는 TCP 포트 13번에서 수신기(acceptor)를 수신하도록 초기화한다.
    tcp_server(boost::asio::io_context& io_context)
        : io_context_(io_context),
        acceptor_(io_context, tcp::endpoint(tcp::v4(), 13))
    {
        start_accept();
    }

private:
    //start_accept() 함수는 소켓을 생성하고 비동기 승인 작업을 시작하여 새로운 연결을 기다린다.
    void start_accept()
    {
        tcp_connection::pointer new_connection =
            tcp_connection::create(io_context_);

        acceptor_.async_accept(new_connection->socket(),
            boost::bind(&tcp_server::handle_accept, this, new_connection,
                boost::asio::placeholders::error));
    }
    // start_accept()에서 시작된 비동기 승인 작업이 완료되면 handle_accept() 함수가 호출된다. 
    // 클라이언트 요청을 처리한 다음 start_accept()를 호출하여 다음 승인 작업을 시작한다.
    void handle_accept(tcp_connection::pointer new_connection,
        const boost::system::error_code& error)
    {
        if (!error)
        {
            new_connection->start();
        }

        start_accept();
    }

    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
};

int main()
{
    try
    {
        // 들어오는 클라이언트 연결을 승인하려면 서버 개체를 생성해야 한다. 
        // io_context 개체는 서버 개체가 사용할 소켓과 같은 I/O 서비스를 제공한다.
        boost::asio::io_context io_context;
        tcp_server server(io_context);
        //io_context 개체를 실행하여 사용자를 대신하여 비동기 작업을 수행한다.
        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

}