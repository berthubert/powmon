#include <gpiod.hpp>
#include "ext/powerblog/h2o-pp.hh"
#include <thread>
#include <atomic>
#include <iostream>
#include <chrono>
#include <signal.h>

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

	::gpiod::chip chip(argv[1]);
	auto lines = chip.get_lines(offsets);

	lines.request({
		argv[0],
		::gpiod::line_request::EVENT_FALLING_EDGE,
		0,
	});

	H2OWebserver h2s("solcount");
	
	/*
	# HELP http_requests_total The total number of HTTP requests.
	# TYPE http_requests_total counter
	http_requests_total{method="post",code="200"} 1027 1395066363000
	http_requests_total{method="post",code="400"}    3 1395066363000
	*/
	
	h2s.addHandler("/metrics",[](auto handler, auto req) 
        {
        	ostringstream ret;
        	ret << "# HELP power_pulses The total number of power pulses"<<endl;	
        	ret << "# TYPE power_pulses counter"<<endl;
        	ret<<"power_pulses "<<g_pulses<<endl;
                   return pair<string,string>("text/plain", ret.str());
                 });


	std::thread ws([&h2s]() {
	      auto actx = h2s.addContext();
	      ComboAddress listenOn("0.0.0.0:10000");
	      h2s.addListener(listenOn, actx);
	      cout<<"Listening on "<< listenOn.toStringWithPort() <<endl;
	      h2s.runLoop();
	    });
	ws.detach();


	for (;;) {
		auto events = lines.event_wait(chrono::seconds(1));
		if (events) {
			for (auto& it: events) {
				it.event_read();
				g_pulses++;
				cout<<g_pulses<<endl;
//				print_event(it.event_read());
			}
		}
	}

}
