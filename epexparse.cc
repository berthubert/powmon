#include <regex>
#include <iostream>
#include <fstream>
#include <cctype>
#include <fmt/chrono.h>
#include <fmt/os.h>
#include <fmt/printf.h>
#include <nlohmann/json.hpp>

using namespace std;

int main(int argc, char** argv)
{
  struct Entry
  {
    time_t start;
    double eurPerMWh;
  };
  vector<Entry> entries;

  for(int n = 1; n < argc ; ++n) {
    ifstream ifs(argv[n]);
    if(!ifs) {
      cerr<<"Could not open file "<<argv[n]<<endl;
      exit(1);
    }
    string line;
    string html;
    while(getline(ifs, line)) {
      for(auto& c : line) {
	if(isspace(c))
	  c=' ';
      }
      html += line;
    }
    
    /* All on one line
       <tr class="child ">
       <td>901.2</td>
       <td>1,922.5</td>
       <td>1,922.5</td>
       <td>187.90</td>
       </tr>
    */
    

    //                 data-head="31.08.26"
    std::regex date(R"(data-head="(\d\d)\.(\d\d)\.(\d\d))");
    std::smatch m;
    
    if(!std::regex_search(html, m, date) || m.size() < 4) {
      cerr<<"Could not find date"<<endl;
      exit(1);
    }
    struct tm tm={.tm_isdst = -1};
    tm.tm_mday = atoi(m[1].str().c_str());
    tm.tm_mon =  atoi(m[2].str().c_str()) - 1;
    tm.tm_year = atoi(m[3].str().c_str()) + 100; 
    
    time_t daystart = mktime(&tm);
    
    fmt::print("Start of day in local timezone: {:%Y-%m-%d %H:%M}\n",
	       fmt::localtime(daystart));
    
    std::regex generic(R"(<tr class="child[^>]*>\s*<td>([0-9,.-]*)</td>\s*<td>([0-9,.-]*)</td>\s*<td>([0-9,.-]*)</td>\s*<td>([0-9,.-]*)</td>\s*</tr>)");
    
    auto gen_begin = std::sregex_iterator(html.begin(), html.end(), generic);
    auto gen_end = std::sregex_iterator();
    
    // buy volume 1
    // sell volume 2
    // volume 3
    // price 4
    time_t start = daystart;
    
    for(auto iter = gen_begin; iter != gen_end; ++iter) {
      m = *iter;
      string val = m[4];
      string res;
      res.reserve(val.size());
      for(const auto& c : val)
	if(c!=',')
	  res.append(1, c);

      double price = atof(res.c_str());
      
      entries.emplace_back(start, price);
      
      start += 900;
    }
  }
  auto out = fmt::output_file("nlprices-epex.csv");
  out.print(",price\n");

  sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
    return a.start < b.start;
  });
  
  for(const auto& e : entries) {
    struct tm tm={};
    localtime_r(&e.start, &tm);
    string tzoff = fmt::sprintf("%c%02d:%02d",
			      tm.tm_gmtoff > 0 ? '+' : '-',
			      tm.tm_gmtoff / 3600,
			      tm.tm_gmtoff % 3600
			      );
    
    out.print("{:%Y-%m-%d %H:%M:%S}{},{}\n",
	      fmt::localtime(e.start), tzoff, e.eurPerMWh);
  }

  // {"price":{"1787470200000":0.06,"1787471100000":-0.12, "1787472000000":0.06

  nlohmann::json data;
  for(const auto& e : entries) {
    data[to_string(e.start*1000)] = e.eurPerMWh;
  }
  nlohmann::json o;
  o["price"]=data;
  auto outj = fmt::output_file("nlprices-epex.json");
  outj.print("{}", o.dump());
}
