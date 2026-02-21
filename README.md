# Cache & Memory Subsystem Simulator (C++)

A configurable C++ simulator for:

- Direct Mapped Cache
- Fully Associative Cache
- Set Associative Cache

## Features

- Write-back and Write-through policies
- Replacement policies:
  - Random
  - LRU
  - LIFO
- Miss rate calculation
- AMAT computation
- File-based workload input

## Project Structure

cache-memory-simulator/
│
├── src/
│ └── cache_simulator.cpp
│
├── examples/
│ └── sample_input.txt
│
└── README.md

## How to Run

Compile:
g++ src/cache_simulator.cpp -o cache_sim

Run:
./cache_sim

Follow prompts to configure cache.

## Example Output

Hits: 6
Misses: 4
Miss Rate: 0.4
AMAT: 5