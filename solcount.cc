#include <gpiod.hpp>
#include <thread>
#include <atomic>
#include <iostream>
#include <chrono>
#include <signal.h>
#define CPPHTTPLIB_USE_POLL
#define CPPHTTPLIB_THREAD_POOL_COUNT 32

#include "httplib.h"


using Clock = std::chrono::steady_clock; 

static double passedMsec(const Clock::time_point& then, const Clock::time_point& now)
{
  return std::chrono::duration_cast<std::chrono::microseconds>(now - then).count()/1000.0;
}


static double passedMsec(const Clock::time_point& then)
{
  return passedMsec(then, Clock::now());
}


 
using namespace std;		// No need to keep using

std::atomic<uint64_t> g_pulses;
#if 0
void print_event(const ::gpiod::line_event& event)
{
	
	if (event.event_type == ::gpiod::line_event::RISING_EDGE)
		::std::cout << " RISING EDGE";
	else if (event.event_type == ::gpiod::line_event::FALLING_EDGE)
		::std::cout << "FALLING EDGE";
	else
		throw ::std::logic_error("invalid event type");

	::std::cout << " ";

	::std::cout << ::std::chrono::duration_cast<::std::chrono::seconds>(event.timestamp).count();
	::std::cout << ".";
	::std::cout << event.timestamp.count() % 1000000000;

	::std::cout << " line: " << event.source.offset();

	::std::cout << ::std::endl;
}
#endif

int main(int argc, char **argv)
{
  signal(SIGPIPE, SIG_IGN); // every TCP application needs this

  if (argc < 3) {
    cout << "usage: " << argv[0] << " <chip> <offset0> ..." << endl;
    return EXIT_FAILURE;
  }

  vector<unsigned int> offsets;
  offsets.reserve(argc);
  for (int i = 2; i < argc; i++)
    offsets.push_back(stoul(argv[i]));


  auto request =
    ::gpiod::chip(argv[1])
    .prepare_request()
    .set_consumer("watch-line-value")
    .add_line_settings(
		       atoi(argv[2]),
		       ::gpiod::line_settings()
		       .set_direction(
				      ::gpiod::line::direction::INPUT)
		       .set_edge_detection(
					   ::gpiod::line::edge::FALLING)
		       )
    .do_request();

	
	
  /*
    # HELP http_requests_total The total number of HTTP requests.
    # TYPE http_requests_total counter
    http_requests_total{method="post",code="200"} 1027 1395066363000
    http_requests_total{method="post",code="400"}    3 1395066363000
  */

  httplib::Server svr;
  svr.Get(R"(/metrics)", [](const auto &req, auto&res) {
    ostringstream ret;
    ret << "# HELP power_pulses The total number of power pulses"<<endl;	
    ret << "# TYPE power_pulses counter"<<endl;
    ret<<"power_pulses "<<g_pulses<<endl;
    res.set_content(ret.str(), "text/plain");	  
  });
	
  std::thread ws([&svr]() {
    svr.set_socket_options([](socket_t sock) {
      int yes = 1;
      setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		 reinterpret_cast<const void *>(&yes), sizeof(yes));
    });
	  
    cout<<"Going live on http://0.0.0.0:10000/\n";
    svr.listen("0.0.0.0", 10000);
  });
  ws.detach();

  //  auto prev = Clock::now();

  ::gpiod::edge_event_buffer buffer(1);

  for (;;) {
    request.read_edge_events(buffer);
    
    for (const auto &event : buffer) {
      ::std::cout << "line: " << event.line_offset()
		  << "  event #" << event.line_seqno()
		  << ::std::endl;
      
      g_pulses++;
    }
  }
}


