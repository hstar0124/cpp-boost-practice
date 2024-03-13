#define _CRT_SECURE_NO_WARNINGS
#include <ctime>
#include <iostream>
#include <string>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

/*
    클라이언트로 다시 보낼 문자열을 생성하는 make_daytime_string() 함수를 정의한다. 
    이 함수는 모든 daytime 서버 응용프로그램에서 재사용될 것이다.
*/
std::string make_daytime_string()
{
    using namespace std; // For time_t, time and ctime;
    time_t now = time(0);
    return ctime(&now);
}

int main()
{
    try
    {
        boost::asio::io_context io_context;

        /*
            새로운 연결을 수신하려면 ip::tcp::acceptor 객체를 생성해야 한다. 
            IP 버전 4에 대해 TCP 포트 13번에서 수신하도록 초기화되었다.
        */
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 13));

        for (;;)
        {
            /*
                이는 처리를 순차적으로 반복하는 서버의 형태로, 한 번에 하나의 연결만 처리를 한다. 
                클라이언트의 연결을 나타내는 소켓을 생성한 다음 연결을 기다린다.
            */
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            // 클라이언트가 서비스에 접근하고 있으면, 현재 시간을 확인하여 클라이언트로 정보를 전송한다.
            std::string message = make_daytime_string();

            boost::system::error_code ignored_error;
            boost::asio::write(socket, boost::asio::buffer(message), ignored_error);
        }
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}