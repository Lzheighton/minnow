#include "address.hh"
#include "debug.hh"
#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>

using namespace std;

namespace {
void get_URL( const string& host, const string& path )
{
  debug( R"(Function called: get_URL( "{}", "{}" ))", host, path );
  // debug( "get_URL() function not yet implemented" );

  // 建立TCPSocket连接
  Address target( host, "http" );
  TCPSocket http_tcp;
  http_tcp.connect( target );

  // 跟telnet一样，向TCPSocket直接写入HTTP报文
  http_tcp.write( "GET " + path + " HTTP/1.1\r\n" + "HOST: " + host + "\r\n" + "Connection: close\r\n" + "\r\n" );

  // 读取返回内容
  while ( !http_tcp.eof() ) {
    string s;
    http_tcp.read( s );
    cout << s;
  }

  http_tcp.close();
}
} // namespace

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
