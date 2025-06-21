# Tachyon-Tick: A Distributed Time-Series Compressor & Query Engine

[![Language](https://img.shields.io/badge/language-C%2B%2B-blue.svg)](https://isocpp.org/)
[![Build](https://img.shields.io/badge/build-CMake-green.svg)](https://cmake.org/)
[![Parallelism](https://img.shields.io/badge/parallelism-OpenMPI-orange.svg)](https://www.open-mpi.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Tachyon-Tick is a high-performance, distributed system designed to tackle the immense volume of financial tick data. It features a custom, block-based compression algorithm tailored for time-series data and a query engine capable of executing analytical queries directly on the compressed format, avoiding costly full decompression.

## Table of Contents

- [Tachyon-Tick: A Distributed Time-Series Compressor \& Query Engine](#tachyon-tick-a-distributed-time-series-compressor--query-engine)
  - [Table of Contents](#table-of-contents)
  - [Core Concept](#core-concept)
  - [Why It's a "Killer" Showcase](#why-its-a-killer-showcase)
  - [Technology Stack](#technology-stack)
  - [System Architecture](#system-architecture)
  - [The Tachyon-Tick Compressed Format](#the-tachyon-tick-compressed-format)
    - [Header (Fixed-size for fast skipping)](#header-fixed-size-for-fast-skipping)
    - [Data Sections (Variable-size, tightly packed bits)](#data-sections-variable-size-tightly-packed-bits)
  - [Getting Started](#getting-started)
    - [Prerequisites](#prerequisites)
    - [Building](#building)
  - [Usage](#usage)
  - [Project Roadmap](#project-roadmap)
  - [License](#license)

## Core Concept

Financial tick data, while voluminous, is highly structured and compressible. Timestamps increment predictably, and prices often remain unchanged or change by a small delta. Tachyon-Tick exploits these properties using a distributed model:

1.  **Distributed Compression**: A live "firehose" of financial data is distributed across multiple `MPI` processes. These worker processes compress batches of data in parallel into a custom binary format.
2.  **Querying on Compressed Data**: The real power lies in the query engine. It can process analytical queries (e.g., "find the average spread for AAPL in a 50ms window") by scanning the compressed blocks and performing on-the-fly micro-decompression of only the necessary fields, bypassing the need to decompress entire datasets.

## Why It's a "Killer" Showcase

This project is a graduate-level computer science endeavor that demonstrates:

- **Mastery of low-level data manipulation**: Designing a custom binary format requires a deep understanding of bit-packing, endianness, and data alignment.
- **Compression Theory**: It applies well-known techniques like **Delta-of-Delta encoding**, **Run-Length Encoding (RLE)**, and **XOR-based compression** (inspired by Facebook's Gorilla) in a novel, combined approach.
- **Parallel Algorithm Design**: The system's architecture is built on the Message Passing Interface (MPI), showcasing skills in designing and implementing distributed algorithms for both data processing and query execution.
- **Systems Programming in C++**: Building a robust, high-performance backend from scratch.

## Technology Stack

- **Language**: C++17
- **Parallel Computing**: OpenMPI
- **Build System**: CMake

## System Architecture

The system uses a Coordinator-Worker model orchestrated by MPI.

- **Coordinator (Rank 0)**:

  - Acts as the entry point for the data stream and user queries.
  - Distributes raw data batches to available workers (e.g., using a round-robin or symbol-based hashing strategy).
  - Broadcasts queries to all workers.
  - Receives and aggregates partial results from workers to compute the final answer.

- **Workers (Ranks 1..N)**:
  - Receive raw tick data from the Coordinator.
  - Compress the data into `CompressedBlock`s and store them locally in memory.
  - Listen for queries, scan their local compressed data, and execute calculations.
  - Send their partial results back to the Coordinator.

```
                  +------------------------+
Data Firehose --> |                        | --(Distributes Batches)--> +----------+
                  |  Coordinator (Rank 0)  |                           | Worker 1 |
User Query ---->  |                        | --(Broadcasts Query)----> +----------+
                  +-----------+------------+                           +----------+
                              ^                                        | Worker 2 |
                              | (Aggregates)                           +----------+
                              |                                        +----------+
                              +---(Sends Partial Result)-------------- | Worker N |
                                                                       +----------+
```

## The Tachyon-Tick Compressed Format

The core innovation is the `CompressedBlock`. Each block stores a fixed number of ticks (e.g., 1024) for a single financial symbol.

A `CompressedBlock` consists of a header and three data sections:

#### Header (Fixed-size for fast skipping)

- `start_timestamp`, `end_timestamp`: The time range of the block. Allows for rapid discarding of irrelevant blocks during a query.
- `symbol_id`: An integer mapping to the symbol string (e.g., "AAPL" -> 0).
- `num_ticks`: Number of ticks in the block.
- `data_section_offsets`: Pointers to the start of each data section.
- `first_bid_price`, `first_ask_price`: The full, uncompressed starting prices.

#### Data Sections (Variable-size, tightly packed bits)

- **Timestamps**: Compressed using **Delta-of-Delta encoding**. We store the delta between consecutive deltas, which are often very small numbers that can be encoded in just a few bits.
- **Prices**: Compressed using a hybrid **RLE/XOR** scheme.
  - A control bit `0` indicates the price is unchanged (Run-Length Encoding).
  - A control bit `1` indicates a change. We then store the `XOR` of the current and previous price's floating-point representation. This result has many leading/trailing zeros and can be stored compactly.
- **Sizes**: Compressed using **Variable-Length Integer (VarInt)** encoding, where smaller, more common integer sizes use fewer bytes.

## Getting Started

### Prerequisites

- A C++17 compliant compiler (e.g., g++)
- CMake (version 3.10 or higher)
- An OpenMPI implementation

On Debian/Ubuntu, you can install these with:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake openmpi-bin libopenmpi-dev
```

### Building

1.  Clone the repository:
    ```bash
    git clone https://github.com/your-username/tachyon-tick.git
    cd tachyon-tick
    ```
2.  Create a build directory:
    ```bash
    mkdir build && cd build
    ```
3.  Run CMake and make:
    ```bash
    cmake ..
    make
    ```

## Usage

The executable is created in the `build` directory. To run the simulation with `N` processes, use `mpirun`. For example, to run with 1 Coordinator and 3 Workers (4 processes total):

```bash
mpirun -np 4 ./tachyon_tick
```

The program will start, the Coordinator will generate and distribute mock tick data, the Workers will compress it, and finally, a sample query will be executed and the result printed to the console.

## Project Roadmap

- [ ] **Full Compression Implementation**: Complete the bit-level implementation of the delta-of-delta, XOR, and VarInt encoding/decoding in the `CompressedBlock` and `CompressedBlockScanner` classes.
- [ ] **Advanced Queries**: Add support for more complex query types, such as calculating VWAP (Volume-Weighted Average Price) and OHLC (Open, High, Low, Close) bars.
- [ ] **Asynchronous MPI**: Convert blocking MPI calls (`MPI_Send`, `MPI_Recv`) to their asynchronous counterparts (`MPI_Isend`, `MPI_Irecv`) to improve pipeline throughput.
- [ ] **Data Persistence**: Add functionality to save compressed blocks to disk and memory-map them for querying, allowing for analysis of datasets larger than available RAM.
- [ ] **Robust Benchmarking Suite**: Develop a comprehensive suite to measure compression ratios, compression speed, and query latency as a function of data volume and the number of worker processes.
- [ ] **Symbol-based Data Distribution**: Implement a hashing mechanism in the Coordinator to ensure all ticks for a given symbol are sent to the same Worker, improving data locality and query efficiency.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE.md) file for details.
