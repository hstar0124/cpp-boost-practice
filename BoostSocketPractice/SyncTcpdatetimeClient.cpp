#include <ctime>
#include <iostream>
#include <string>
#include <boost/asio.hpp>

/*
    응용프로그램의 목적은 daytime 서비스에 접근하는 것으로, 접속하려는 서버 지정이 필요하다.
*/
using boost::asio::ip::tcp;

int main(int argc, char* argv[])
{

    /////////////////////////////////////////////////////////////////////
    //Daytime.1 - 동기식 TCP daytime 클라이언트 (A synchronous TCP daytime client)
    /////////////////////////////////////////////////////////////////////
    {
        try
        {
            if (argc != 2)
            {
                std::cerr << "Usage: client <host>" << std::endl;
                return 1;
            }

            // asio를 사용하는 모든 프로그램은 io_context 객체와 같은 I/O 실행 컨텍스트가 하나 이상 있어야 한다.
            boost::asio::io_context io_context;

            // 응용프로그램의 파라미터로 지정된 서버 이름을 TCP 엔드포인트(endpoint)로 변경해야 한다. 
            // ip::tcp::resolver 개체를 사용하여 변환 작업을 할 수 있다.
            tcp::resolver resolver(io_context);

            /*
                리졸버(resolver)는 호스트 이름과 서비스 이름을 가져와서 엔드포인트(endpoint)의 목록으로 변경한다.
                argv[1]에 지정된 서버 이름과 서비스 이름(이 경우 "daytime")을 사용하여 resolve를 호출하여 작업을 수행한다.
                엔드포인트(endpoint) 목록은 ip::tcp::resolver::result_type 유형의 객체를 사용하여 반환된다.
                이 개체는 결과로 반복자(iterator)의 실행에 사용할 수 있는 begin()과 end() 멤버 함수의 범위를 가지고 있다.
            */
            tcp::resolver::results_type endpoints = resolver.resolve(argv[1], "daytime");

            /*
                이제 소켓을 생성하고 연결한다. 위에서 얻은 엔드포인트(endpoint) 목록에는 IPv4와 IPv6 엔드포인트(endpoint) 모두 포함되어 있을 수 있다.
                그래서 작업에 사용할 엔드포인트(endpoint)를 찾을 때까지 각각에 대해 시도할 필요가 있다.
                이는 특정 IP 버전과 독립적으로 클라이언트 프로그램을 유지되도록 한다. boost::asio::connect() 함수는 이를 자동으로 수행한다.
            */
            tcp::socket socket(io_context);
            boost::asio::connect(socket, endpoints);

            /*
                수신된 데이터를 보관하기 위해 boost::array를 사용한다.
                boost::asio:;buffer() 함수는 버퍼 오버런을 방지하기 위해 배열의 크기를 자동으로 결정한다.
                boost::array 대신에, char [] 또는 std::vector를 사용할 수 있다.
            */
            for (;;)
            {
                std::array<char, 128> buf;
                boost::system::error_code error;

                size_t len = socket.read_some(boost::asio::buffer(buf), error);

                /*
                    서버가 연결을 해제할 때, ip::tcp::socket::read_some() 함수가
                    boost::asio::error::eof 오류와 함께 종료되고, 이것이 루프를 종료하는 방법이다.
                */
                if (error == boost::asio::error::eof)
                    break; // Connection closed cleanly by peer.
                else if (error)
                    throw boost::system::system_error(error); // Some other error.

                std::cout.write(buf.data(), len);
            }
        }
        catch (std::exception& e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
}