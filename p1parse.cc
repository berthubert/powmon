#include <iostream>
#include <fcntl.h>                                                        
#include <termios.h>                                                      
#include <stdio.h>                                                        
#include <unistd.h>
#include <strings.h>
#include <stdlib.h>
#include <string>
#include <algorithm>
#include <random>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <map>
#include <sstream>
#include <thread>
#include "ext/powerblog/h2o-pp.hh"
#include <mutex>

int g_baudval{B115200};

std::string string_replace(const std::string& str, const std::string& match, 
        const std::string& replacement)
{
    size_t pos = 0;
    std::string newstr = str;

    while ((pos = newstr.find(match, pos)) != std::string::npos
            )
    {
         newstr.replace(pos, match.length(), replacement);
         pos += replacement.length();
    }
    return newstr;
}



static void doTermios(int fd)
{
  struct termios newtio;

  bzero(&newtio, sizeof(newtio));                                         
  newtio.c_cflag = CS8 | CLOCAL | CREAD;             
  newtio.c_iflag = IGNPAR;                                                
  newtio.c_oflag = 0;                                                     
  
  /* set input mode (non-canonical, no echo,...) */                       
  newtio.c_lflag = 0;                                                     
    
  newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */         
  newtio.c_cc[VMIN]     = 4;   /* blocking read until 5 chars received */ 
  
  cfsetspeed(&newtio, g_baudval);
  if(tcsetattr(fd, TCSANOW, &newtio)) {
    perror("tcsetattr");
    exit(-1);
  }

}

using namespace std;


map<string, double> parseDSMR(const std::string& in)
{
  istringstream inp(in);
  string line;
  map<string, double> ret;
  
  while(getline(inp, line)) {
    //    if(line.find("kW")== string::npos)
    //  continue;

    /*
    1-0:1.8.1(011295.029*kWh)
    1-0:1.7.0(02.677*kW)
    1-0:21.7.0(00.469*kW)
    1-0:32.32.0(00000)
    */

    if(auto pos1 = line.find(':'); pos1 != string::npos)
      if(auto pos2 = line.find('(', pos1); pos2 != string::npos)
	if(auto pos3 = line.find(')', pos2); pos3 != string::npos) {
	  ret[line.substr(pos1+1, pos2-pos1-1)]=atof(&line[pos2+1]);
	}
    
    
  }
  return ret;
}

std::mutex g_metrix_mutex;  
std::shared_ptr<map<string, double>> g_metrics;

static void addMetric(ostringstream& ret, std::string_view key, std::string_view desc, std::string_view kind, double factor=1.0)
{

  map<string, double> metrics;
  {
    std::lock_guard<std::mutex> lock(g_metrix_mutex);
    metrics = *g_metrics;
  }
    

  string rkey = string_replace((string)key, ".", "_");
  ret << "# HELP dsmr_" << rkey << " " <<desc <<endl;
  ret << "# TYPE dsmr_"<< rkey << " " << kind <<endl;
  ret<<"dsmr_"<<rkey<<" "<< std::fixed<< factor*metrics[(string)key] <<endl;
}

int main()
{
  int fd = open("/dev/ttyUSB0", O_RDONLY);
  if(fd < 0)
    throw runtime_error("Unable to open serial port: "+string(strerror(errno)));
  doTermios(fd);

  H2OWebserver h2s("p1mon");
  
  h2s.addHandler("/metrics", [](auto handler, auto req) {
			       ostringstream ret;

			       addMetric(ret, "1.8.1", "Total energy IN low tariff (J)", "counter", 3600*1000);
			       addMetric(ret, "1.8.2", "Total energy IN high tariff (J)", "counter", 3600*1000);
			       addMetric(ret, "2.8.1", "Total energy OUT low tariff (J)", "counter", 3600*1000);
			       addMetric(ret, "2.8.2", "Total energy OUT high tariff (J)", "counter", 3600*1000);

			       addMetric(ret, "1.7.0", "Total power IN (kW)", "gauge");
			       addMetric(ret, "2.7.0", "Total power OUT (kW)", "gauge");

			       addMetric(ret, "21.7.0", "Total power IN (kW) phase 1", "gauge");
			       addMetric(ret, "41.7.0", "Total power IN (kW) phase 2", "gauge");
			       addMetric(ret, "61.7.0", "Total power IN (kW) phase 3", "gauge");

			       addMetric(ret, "22.7.0", "Total power OUT (kW) phase 1", "gauge");
			       addMetric(ret, "42.7.0", "Total power OUT (kW) phase 2", "gauge");
			       addMetric(ret, "62.7.0", "Total power OUT (kW) phase 3", "gauge");

			       addMetric(ret, "96.14.0", "Tariff indicator", "gauge");
			       addMetric(ret, "96.7.21", "Power failires in any phase", "counter");
			       addMetric(ret, "96.7.9", "Long power failires in any phase", "counter");

			       addMetric(ret, "32.32.0", "L1 sags", "counter");
			       addMetric(ret, "52.32.0", "L2 sags", "counter");
			       addMetric(ret, "72.32.0", "L3 sags", "counter");

			       addMetric(ret, "32.36.0", "L1 swells", "counter");
			       addMetric(ret, "52.36.0", "L2 swells", "counter");
			       addMetric(ret, "72.36.0", "L3 swells", "counter");

			       
			       return pair<string,string>("text/plain", ret.str());
			     });
      

  
  bool first = true;
  
  for(;;) {
    string msg;
    char c;
    for(;;) {
      int res = read(fd, &c, 1);
      if(res != 1)
	throw runtime_error("Serial error");
      if(c=='/')
	break;
    }
    for(;;) {
      int res = read(fd, &c, 1);
      if(res != 1)
	throw runtime_error("Serial error");
      if(c=='!')
	break;
      msg.append(1, c);
    }
    //    cerr<<"Got a DSMR message: "<<msg<<endl;
    auto res=parseDSMR(msg);
    auto metrics = make_shared<map<string,double>>();
    for(const auto& r: res) {
      cerr<< r.first <<" = " <<r.second<<endl;
      (*metrics)[r.first] = r.second;
    }
    {
      std::lock_guard<std::mutex> lock(g_metrix_mutex);
      g_metrics = metrics;
    }
    if(first) {
      std::thread ws([&h2s]() {
		       auto actx = h2s.addContext();
		       ComboAddress listenOn("0.0.0.0:10000");
		       h2s.addListener(listenOn, actx);
		       cout<<"Listening on "<< listenOn.toStringWithPort() <<endl;
		       h2s.runLoop();
		     });
      ws.detach();
      first = false;
    }
  }
}
