# Exchange Matching Engine in C++

## Contributors

- Nguyen Quy Duc
- Nguyen Van Binh 

## Overview

This project showcases a robust exchange matching engine implemented in C++. Designed with efficiency and concurrency in mind, our engine effectively handles asynchronous BUY and SELL requests while ensuring data integrity and quick access to order information.

## Key Features

### Data Structures

- **Order Lookup (`unordered_map`)**: Maps `order_id` to `pair<instrument, is_buy>`, allowing for rapid existence checks and retrieval of order details.
  
- **Instrument Lookup (`unordered_map`)**: Links instruments to their respective `InstrumentBook` objects for quick access and validation.

- **InstrumentBook Struct**: Holds all `Order` objects for an instrument, utilizing `priority_queues` for both buy and sell orders, ensuring sorted access by price and time.

- **Order Struct**: Captures essential order details, including `order_id`, `price`, `count`, `timestamp`, and more, facilitating easy management and processing.

### Synchronization Primitives

We implement mutexes and manual locks to safeguard our global data structures, preventing race conditions and ensuring reliable operations:

- **Order Lookup Mutex**: Protects the `orderLookup` map.
  
- **Instrument Lookup Mutex**: Safeguards the `instrumentLookup` map.

- **InstrumentBook Mutexes**: Each `InstrumentBook` has its own mutex to manage concurrent access to buy and sell orders.

### Concurrency Model

Our engine employs **instrument-level concurrency**, enabling simultaneous processing of orders across different instruments. This is achieved by locking each `InstrumentBook` individually, allowing for efficient operations without unnecessary delays.

For example, when processing orders for `AAPL` and `GOOG`, our system locks only the necessary resources, significantly enhancing throughput and reducing bottlenecks.

### Testing Methodology

To ensure robustness, we employed:

- **ThreadSanitizer (TSan)**: To detect and resolve potential data races and threading issues during compilation.

- **AddressSanitizer (ASan)**: To identify memory-related errors like out-of-bounds accesses.

- **Test Case Generation**: Using a custom Python script, we generated diverse test scenarios, scaling from single-client tests to multi-client simulations with varying instrument and order counts.

This approach guarantees our matching engine is not only efficient but also reliable and scalable, making it an excellent fit for real-world trading environments.