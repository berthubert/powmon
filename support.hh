#pragma once
#include "peglib.h"

class PrometheusParser
{
public:
  PrometheusParser();
  void parse(const std::string& cont);

  typedef std::map<std::string, std::map<std::map<std::string,std::string>, double>> prom_t;
  prom_t d_prom;
private:
  peg::parser d_parser;
};
