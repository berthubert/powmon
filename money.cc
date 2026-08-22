#include <map>
#include <mutex>
#include <iostream>
#include <unistd.h>
#include <stdexcept>
#include "nlohmann/json.hpp"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "fmt/chrono.h"
#include "httplib.h"
#include "powmon.hh"
using namespace std;

std::mutex g_prices_mutex;
// euros per kWh
std::map<time_t, double> g_prices;

void pricingThread()
{
  for(;;) {
    try {
      time_t now = time(0);
      time_t from = now - 86400;
      time_t till = now + 86400;
      struct tm fromtm={}, tilltm={};
      gmtime_r(&from, &fromtm);
      gmtime_r(&till, &tilltm);

      
      std::string url="https://berthub.eu/nlelec/nlprices.json";
      cout<<url<<endl;

      httplib::Client cli("https://berthub.eu");
      
      auto cres = cli.Get("/nlelec/nlprices.json");
      if(!cres || cres->status != 200) {
	throw runtime_error("Problem retrieving pricing from berthub");
      }
      string res=cres->body;

      using namespace nlohmann;
      json ex1 = json::parse(res);
      auto prices=ex1["price"];
      std::map<time_t, double> pricemap;

      // {"1786796100000":4.82,"1786797000000":5.23,"1786797900000":7.96 ..
      
      for(const auto& p : prices.items()) {
        struct tm tm;
	
        time_t t = (atol(p.key().c_str()))/1000;
        pricemap[t] = (double)p.value() / 1000.0;
	// price is euros/mWh, we need per kWh
	fmt::print("Date: {:%a, %d %b %Y %H:%M:%S %z (%Z)} -> {} euro/kWh", fmt::localtime(t), ((double)p.value())/1000.0);
      }
      {
        std::lock_guard<std::mutex> lock(g_prices_mutex);
        g_prices = pricemap;
      }

      sleep(3600);
    }
    catch(std::exception& e) {
      cerr<<"Error: "<<e.what()<<endl;
      sleep(300);
    }
  }
}

std::optional<double> getPrice(time_t now)
{
  decltype(g_prices) prices;
  {
    std::lock_guard<std::mutex> lock(g_prices_mutex);
    prices = g_prices;
  }
  std::optional<double> ret;
  if(prices.empty()) // no pricing, sorry
    return ret;
  // if now is at the beginning of our map, we also have no applicable pricing
  if(auto iter = prices.lower_bound(now); iter != prices.begin()) {
    --iter;
    if(now - iter->first > 7200) // too old
      return ret;
    return iter->second;
  }
  return ret;
}
