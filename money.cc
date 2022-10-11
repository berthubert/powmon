#include "minicurl.hh"
#include <map>
#include <mutex>
#include <iostream>
#include <unistd.h>
#include <stdexcept>
#include "nlohmann/json.hpp"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "powmon.hh"
using namespace std;

std::mutex g_prices_mutex;  
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
      
      std::string url=fmt::sprintf("https://api.energyzero.nl/v1/energyprices?fromDate=%04d-%02d-%02dT22%%3A00%%3A00.000Z&tillDate=%04d-%02d-%02dT21%%3A59%%3A59.999Z&interval=4&usageType=1&inclBtw=false",
                                   fromtm.tm_year+1900, 1+fromtm.tm_mon, fromtm.tm_mday,
                                   tilltm.tm_year+1900, 1+tilltm.tm_mon, tilltm.tm_mday);
      cout<<url<<endl;

      MiniCurl mc;
      string res=mc.getURL(url);
      //cout<<res<<endl;
      using namespace nlohmann;
      json ex1 = json::parse(res);
      auto prices=ex1["Prices"];
      std::map<time_t, double> pricemap;
      for(const auto& p : prices) {
        struct tm tm;
        // 2022-10-10T22:00:00Z
        char* ptr=strptime(p["readingDate"].get<string>().c_str(), "%Y-%m-%dT%H:%M:%S%z", & tm);
        if(!ptr || *ptr) {
          throw std::runtime_error("Could not parse the whole date: "+p["readingDate"].get<string>());
        }
        time_t t = timegm(&tm);
        struct tm localtm={};
        localtime_r(&t, &localtm);
        pricemap[t] = p["price"].get<double>();
        cout<<p["price"].get<double>()<<", "<<p["readingDate"]<<" "<<fmt::sprintf("%04d-%02d-%02d %02d:%02d", 1900+localtm.tm_year, 1+localtm.tm_mon, localtm.tm_mday, localtm.tm_hour, localtm.tm_min)<<endl;
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
