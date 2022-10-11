#pragma once
#include <time.h>
#include <optional>

void pricingThread();
std::optional<double> getPrice(time_t now);
