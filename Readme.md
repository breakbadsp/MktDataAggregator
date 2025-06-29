# Market Data File Multiplexer

## Problem Statement

This project addresses the challenge of efficiently merging market data from multiple equity symbols into a single time-ordered file for backtesting purposes. 

### Requirements

- Process market data from ~10,000 individual symbol files into a single consolidated file
- Input files are sorted by timestamp
- Output file must maintain timestamp ordering
- For same timestamps, entries must be sorted by symbol name
- Handle file handle limitations (Windows max 2048, optimal < 100)
- Process 100+ GB of data efficiently
- Support for multithreading (optional)

### Constraints

- Cannot open all files simultaneously (file handle limits)
- Cannot load all data into memory at once
- Must handle billions of entries efficiently

## Solution Approach

### Core Algorithm: K-way Merge

1. **Chunked File Processing**
   - Process files in batches (50-100 files at a time)
   - Use memory-mapped files for efficient I/O
   - Read data in configurable time chunks

2. **Data Management**
   - Sort data within time windows
   - Merge sorted chunks maintaining timestamp order
   - Write to output file with symbol column added

3. **Performance Optimizations**
   - Memory-mapped file I/O for fast access
   - Multi-threaded processing where applicable
   - Efficient buffering strategy

## Building the Project

### Prerequisites
- CMake 3.x
- Modern C++ compiler (C++17 or later)
- Google Test (for running tests)

### Build Instructions
```bash
mkdir build && cd build
cmake ..
make
```

## Building and Running

### Building from Source
```bash
# Create and enter build directory
mkdir -p build && cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)

# Run tests (optional but recommended)
cd gtest && make && ./mmf_tests
```

### Usage

```bash
./bestex [options] <input_directory> <output_file>
```

### Options
- `--buffer-size`: Size of read buffer in MB (default: 64)
- `--max-files`: Maximum number of simultaneously open files (default: 50)
- `--threads`: Number of worker threads (default: hardware concurrency)

### Usage Examples

1. Basic usage with default settings:
```bash
./bestex /path/to/market/data/files output.csv
```

2. Specify buffer size and max open files:
```bash
./bestex --buffer-size 128 --max-files 75 /path/to/market/data/files output.csv
```

3. Multithreaded processing with 8 threads:
```bash
./bestex --threads 8 /path/to/market/data/files output.csv
```

### Input Directory Structure
The input directory should contain market data files named after their symbols:
```
/path/to/market/data/files/
├── AAPL.txt
├── CSCO.txt
├── MSFT.txt
└── ...
```

### Input File Format
Each input file is named after its symbol (e.g., MSFT.txt, AAPL.txt) and contains CSV data with the following columns:
```
Timestamp, Price, Size, Exchange, Type
2021-03-05 10:00:00.123, 228.5, 120, NYSE, Ask
2021-03-05 10:00:00.123, 228.4, 110, NASDAQ, Bid
2021-03-05 10:00:00.133, 228.5, 120, NYSE, TRADE
2021-03-05 10:00:00.134, 228.5, 120, NYSE_ARCA, Ask
```

Format Details:
- Timestamp: YYYY-MM-DD HH:MM:SS.mmm format
- Price: Decimal number
- Size: Integer representing quantity
- Exchange: Trading venue identifier
- Type: Quote type (Ask, Bid, TRADE)

### Output File Format
The output file will contain the following columns in order:
```
Symbol, Timestamp, Price, Size, Exchange, Type

CSCO, 2021-03-05 10:00:00.123, 46.14, 120, NYSE_ARCA, Ask
CSCO, 2021-03-05 10:00:00.123, 46.13, 110, NSX, Bid
MSFT, 2021-03-05 10:00:00.123, 228.5, 120, NYSE, Ask
MSFT, 2021-03-05 10:00:00.123, 228.4, 110, NASDAQ, Bid
CSCO, 2021-03-05 10:00:00.130, 46.13, 120, NYSE, TRADE
CSCO, 2021-03-05 10:00:00.131, 46.14, 120, NYSE_ARCA, Ask
MSFT, 2021-03-05 10:00:00.133, 228.5, 120, NYSE, TRADE
MSFT, 2021-03-05 10:00:00.134, 228.5, 120, NYSE_ARCA, Ask
```

Note: The output maintains strict ordering:
1. Primary sort by Timestamp
2. For entries with the same timestamp, secondary sort by Symbol alphabetically
3. Symbol is placed as the first column for easier readability and sorting

## Performance Considerations

- Uses memory-mapped files for efficient I/O
- Implements buffered reading to minimize system calls
- Employs sorting of time-windowed chunks
- Optimal file handle management
- Thread-safe data structures for parallel processing

## Error Handling
- Invalid timestamps will be logged with errors
- Files with incorrect formats will be skipped with warnings
- Missing input directory will result in error
- Insufficient permissions will be reported with details

## Performance Tips
1. Choose buffer size based on available RAM:
   - 64MB default works well for 16GB RAM
   - Increase for systems with more RAM
   
2. Optimal number of open files:
   - Default 50 is safe for most systems
   - Can increase up to system limit (ulimit -n)
   
3. Thread count considerations:
   - Default uses hardware concurrency
   - For I/O bound systems, try threads = cores * 2
   - For memory bound systems, try threads = cores
