# CryptoTradingInfra

A reasonably low-latency and high-throughput demo for the real trading engine. It maintains an order book capable of storing up to 100 levels of both ask and bid sides. And is also capable of simulating trades happen in real market.

## Build

### Prerequisites

- The compiler should support C++17 or newer standard.
- If built on Apple, ensure that homebrew clang is correctly installed and configured. No Xcode clang is used in this project. Toolchain file is provided for MacOS.
- The environment has Python3 support.
-  `numpy` is installed. If not, try run `pip3 install numpy`.
- The trading engine itself is currently free of other third-party libraries, while running benchmark would require google's benchmark (it's in git submodule) and boost (need to install yourself based on the package management tool you use).

### Supported Platforms

Currently build is tested on following systems (results of `uname -a`):

- `Darwin Somebody'sMacBook-Pro.local 23.5.0 Darwin Kernel Version 23.5.0: Wed May  1 20:14:38 PDT 2024; root:xnu-10063.121.3~5/RELEASE_ARM64_T6020 arm64`

- `Linux pick3t-desktop 5.15.167.4-microsoft-standard-WSL2 #1 SMP Tue Nov 5 00:21:55 UTC 2024 x86_64 x86_64 x86_64 GNU/Linux`

### Core Engine

To build the project, symply run:

`bash build.sh`

The generated binary executable is `./build/trading_engine`.

You can choose a port between 49152 and 65535 for the engine to listen to, for example:

`./build/trading_engine 56789`

If you don't specify the port, it will listen to 49152 by default.

Once you bring up the engine, inject udp packets containing `MarketUpdate`s to the port you specified. You can use the [python script](###MarketUpdate Packet Generation Script) provided.

Press Ctrl+C to stop the engine anytime you feel necessary to, and statistics will be printed once the job is done.

```bash
➜  CryptoTradingInfra git:(master) ✗ ./build/trading_engine
Engine running. Press Ctrl+C to stop...
Port 49152 is listening
^C
Signal (2) received, shutting down.
Total packets received: 122593
Total packets enqued: 122593
Total packets Discarded: 2490373
Total MarketUpdates processed: 122593
Total Trades processed:        122593
====OrderBook====
Asks:
100.002 @75
100.008 @18
100.014 @67
100.014 @57
100.016 @62
Bids:
199.999 @71
199.999 @52
199.995 @64
199.994 @28
199.994 @46
===TradingEngine===
Asks:
168.174 @2
175.852 @72
175.991 @6
176.236 @31
176.292 @17
Bids:
160.205 @1
143.256 @9
130.77 @1
129.98 @42
129.543 @52
```

### Test

If you want to compile the test at the same time:

`bash build.sh -t`

Execute testcases:

`./build/tests/test`

### Benchmark

Download google's benchmark first, it's already delivered as a submodule:

`git submodule update --init`

Make sure you have boost installed, it's used to implement a traditional MPMC ring buffer using locks as the benchmark for comparison with our lock-free MPMC ring buffer.

```bash
# Depends on your system's package manager
# For example:
# MacOS, homebrew as package manager
brew install boost
# Linux, distro Ubuntu
sudo apt-get install libboost-all-dev
```

Build the benchmark test for ring buffer:

`bash build.sh -t -b`

If successful, the generated binary executable called `test_benchmark_ring_buffer` will appear under `build/tests`.

Run the test:

`./build/tests/test_benchmark_ring_buffer`

Example results (varies on systems):

```bash
➜  CryptoTradingInfra git:(master) ✗ uname -a
Darwin pick3tdeMacBook-Pro.local 23.5.0 Darwin Kernel Version 23.5.0: Wed May  1 20:14:38 PDT 2024; root:xnu-10063.121.3~5/RELEASE_ARM64_T6020 arm64
➜  CryptoTradingInfra git:(master) ✗ ./build/tests/test_benchmark_ring_buffer
Unable to determine clock rate from sysctl: hw.cpufrequency: No such file or directory
This does not affect benchmark measurements, only the metadata output.
***WARNING*** Failed to set thread affinity. Estimated CPU frequency may be incorrect.
2025-08-07T10:28:44+08:00
Running ./build/tests/test_benchmark_ring_buffer
Run on (12 X 24 MHz CPU s)
CPU Caches:
  L1 Data 64 KiB
  L1 Instruction 128 KiB
  L2 Unified 4096 KiB (x12)
Load Average: 1.90, 2.32, 2.40
------------------------------------------------------------------------
Benchmark                              Time             CPU   Iterations
------------------------------------------------------------------------
BenchMarkConcurrentRingBuffer   14031437 ns      1109100 ns          637
BenchMarkBoostRingBuffer        15502141 ns      1122256 ns          625
```

It seems like our lock-free design is working ^^.

## Structure

```bash
➜  CryptoTradingInfra git:(master) ✗ tree --gitfile .
.
├── CMakeLists.txt
├── README.md
├── app
│   ├── CMakeLists.txt
│   ├── execution_engine.cpp
│   ├── execution_engine.hpp
│   └── main.cpp
├── build.sh
├── data
│   ├── CMakeLists.txt
│   ├── market_update.hpp
│   ├── order_book.cpp
│   └── order_book.hpp
├── tests
│   ├── CMakeLists.txt
│   ├── test_benchmark_ring_buffer.cpp
│   ├── test_entries.hpp
│   ├── test_execution_engine.cpp
│   ├── test_main.cpp
│   ├── test_market_updates_recv.cpp
│   ├── test_order_book.cpp
│   ├── test_ring_buffer.cpp
│   └── udp_market_client.py
├── toolchains
│   └── homebrew-llvm-toolchain.cmake
└── utils
    ├── CMakeLists.txt
    ├── Lock-Free MPMC Ring Buffer Design.md
    ├── math.hpp
    ├── network.hpp
    └── ring_buffer.hpp

6 directories, 26 files
```

- **app/**
    Contains the main application logic, including the execution engine and program entry point (`main.cpp`). This is where the system is assembled and run.
- **data/**
    Houses fundamental data structures and business logic for market updates and order book management. Key classes for order handling reside here.
- **tests/**
    Includes all unit and integration tests for the trading infrastructure, including Python-based UDP client for integration testing.
- **toolchains/**
    Contains CMake toolchain files to support specific build environments, such as custom compilers or Homebrew LLVM on macOS.
- **utils/**
    Provides supporting utilities, including mathematical helpers, networking components, and advanced data structures like lock-free ring buffers. Documentation for the MPMC ring buffer designs is also included.

## TestCases

You can find entires for unit tests under `tests/test_main.cpp`.

```cpp
int main()
{
    CryptoTradingInfra::Test::TestRingBuffer();
    CryptoTradingInfra::Test::TestOrderBookBasic();
    CryptoTradingInfra::Test::TestExecutionEngineBasic();
    CryptoTradingInfra::Test::TestExecutionEngineCrossTrades();

    ... ...
}
```

### Current Unit Tests

- TestRingBuffer

    Test basic functionalities of `ConcurrentRingBuffer`. Data is generated and inserted to the buffer by multiple producers, and fetched by multiple consumers concurrently. All consumed data is verified against produced data so that its integrity and correctness is guaranteed.

- TestOrderBook

    Only aims to test basic functionalities of `OrderBook`. Multiple producers will randomly generate `MarketUpdate`s and publish them to the book. Multiple consumers call `bestBid()` and `bestAsk()` to fetch best bid/ask. In the end, 10 levels of both bid and ask from the book is printed.

- TestExecutionEngineBasic

    Several `MaketUpdate`s from both sides are published to the engine, no trades will happen in this case. Results are verified against expectations.

- TestExecutionEngineCrossTrades

    Several `MaketUpdate`s from both sides are published to the engine, trades will happen in this case. Results are verified against expectations after each trade happens.


### MarketUpdate Packet Generation Script

The script named `udp_market_client.py` is a Python-based UDP client which essentially injects udp packets containing 1 or more `MarketUpdate` data to a chosen host and port.

You can print help information using `python3 udp_market_client.py --help`:

```bash
usage: udp_market_client.py [-h] [--host HOST] [--port PORT] [--count COUNT] [--pps PPS] [--batch BATCH] [--rnum-updates RNUM_UPDATES]

Send MarketUpdate UDP packets.

options:
  -h, --help            show this help message and exit
  --host HOST           Receiver IP address
  --port PORT           Receiver UDP port
  --count COUNT         Number of packets to send
  --pps PPS             Number of packets to send per second
  --batch BATCH         If turned on, number of packets specified by pps will be sent immediately instead of being sent 1 by 1 based on calculatedsending rate(1.0s / pps)
  --rnum-updates RNUM_UPDATES
                        If turned on, random number of MarketUpdates(1 - 20) will be packed into a single packet
```

Example execution:

```bash
python3 udp_market_client.py --host 127.0.0.1 --port 49152 --count 20 --pps 20
```

You can also add `--rnum-updates=True` to make the script generate packets containing random number of `MarketUpdate`s in each of them:

```bash
python3 udp_market_client.py --host 127.0.0.1 --port 49152 --count 20 --pps 20 --rnum-updates=True
```

#### Pitfalls

Usually above is enough to test if the program is bahaving correctly according to our expectations, but sometimes we want to test the system's throughput and latency, requiring for large input.

In this case, if you try to execute the script and bring up the trading engine on the same machine, things get worse on both sides. For example, if you try to make the script send 100000 packets per second:

```bash
python3 udp_market_client.py --host 127.0.0.1 --port 49152 --count 500000 --pps 100000
```

At the same time, the trading engine is also listening on localhost:49152. Then the rate is no longer guaranteed to "true" (at least on my machine):

 ```bash
 Start sending packets to 127.0.0.1 on port 49152
 Sent 100000 packets in 1.4629696250194684
 Sent 200000 packets in 2.912073167040944
 Sent 300000 packets in 4.358706124941818
 Sent 400000 packets in 5.804974291939288
 Sent 500000 packets in 7.252274124999531
 ```

As you can see, it takes more than 7 secs to send all 500000 packets, which is much slower than what we specified.

To get a slightly better result, turn on the option `--batch`:

```bash
python3 udp_market_client.py --host 127.0.0.1 --port 49152 --count 500000 --pps 100000 --batch=True
Start sending packets to 127.0.0.1 on port 49152
Sent 100000 packets in 1.0049835829995573
Sent 200000 packets in 2.0083054999122396
Sent 300000 packets in 3.0134907079627737
Sent 400000 packets in 4.016816165996715
Sent 500000 packets in 5.021984290913679
```

What this option does under the hood, is to make sure the script will try its best to send number of packets specified by `pps` within 1 second, but not evenly distributed across the second. Therefore in this case, the real send rate is much higher than specified `pps`.

## Todo

- An executable to generate test packets written in C++, much more precisely than the python script
- Support order cancellations

- Introduce spdlog to record ecents
