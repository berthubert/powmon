Two tools to measure power usage using external meters, and publish this in
Prometheus format.

Both tools require libh2o-evloop. Compiling requires cmake.

Clone this repo like this: `git clone --recursive
https://github.com/berthubert/powmon.git` 

If you forgot the --recursive, it won't work.

To compile:

```
mkdir build
cd build
cmake ..
make
```

powmon
------
Initializes /dev/ttyUSB0 to 115200 and expects to find DSMR coded data
there. Published on 0.0.0.0:10000/metrics/

solcount
--------
Requires libgpiod-dev

This tool counts transitions on a selected GPIO chip and pin.

`solcount ./gpiochip0 3` works for me.

Publishes on 0.0.0.0:10000/metrics/

