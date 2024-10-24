# Assignment 1: Exchange matching engine in C++

## Team members

1. Nguyen Van Binh (e0550453)
2. Nguyen Quy Duc (e0851456)

## I. Data structures used

### 1. `unordered_map orderLookup`

- This unordered_map is used to map `order_id` to `pair<instrument, is_buy>`. This is used to quickly check if an order exists and get the instrument and side of the order.

### 2. `unordered_map instrumentLookup`

- This unordered_map is used to map `instrument` to `InstrumentBook` object. This is used to quickly get the `InstrumentBook` object of an instrument as well as checking the existence of an instrument.

### 3. struct `InstrumentBook`

- This struct is used to store the all the `Order` objects corresponding to an instrument. It contains `string` instrument name and 2 `priority_queue` for buy and sell orders (sorted by price and time).
- For each existing instrument, we create and maintian an `InstrumentBook` object.

### 4. struct `Order`

- This struct is used to store the information of an order. It contains `order_id`, `price`, `count`, `timestamp`, `is_buy`, `execution-id` and `instrument_name` of a given order.

## II. Synchronisation primitives used

We use mutexes and manual locks to protect the global data structures to prevent data race. This if not done right can lead to undefined behavior and race condition. However, with proper use, we think manually locking can provide more flexibility and more fine-grained control over the synchronization.

### 1. `std::mutex orderLookupMutex`

- We use one mutex to protect the `orderLookup` map.

### 2. `std::mutex instrumentLookupMutex`

- We use one mutex protect the `instrumentLookup` map.

### 3. `std::mutex instrumentBookMutex`

- For each `InstrumentBook` object, we use a mutex to protect both `priority_queue` of buy and sell orders.

## III. Level of concurrency

### Our implementation is **Instrument-level concurrency**.

- As we maintain each lock for each `InstrumentBook` object, orders of different instruments can be processed concurrently.

- Apart from the fact that when the order first enter the systems, we do need to lock the `instrumentLookup` to check if the instrument exists and create a new `InstrumentBook` object if needed, other operations on different instruments can be done concurrently.

- For example, given 2 commands either buy/sell that have the instruments `AAPL` and `GOOG` respectively. At the first step of `addBuy/SellOrder`, the function first lock `instrumentLookupMutex` in order to find the corresponding `instrumentBook`. Then, releases `instrumentLookupMutex` and acquire the lock of its `instrumentBook` (`APPL`) to do the matching and updating the book. This behavior allows the second operation (`GOOG`) to happen in parallel (except a short wait to acquire `instrumentLookupMutex`).
  - We can reduce this wait time for `instrumentLookupMutex` by doing a pre-processing step to "srad" different instruments to fixed number of buckets so that we only need to fix the corresponding bucket when searching for targeted `instrumentBook` (the bucket can be identified by hashing the instrument and take the modulo of the number of buckets).

## IV. Testing methodology

- First, we use ThreadSanitizer (TSan) for compilation to check for possible data races and threading bugs in the code and try to fix them.

- When running the test cases, we run the program with AddressSanitizer (ASan) to check for memory-relate issues, like out-of-bounds accesses, use-after-free, etc.

- Test cases are generated using the provided `generate_test.py` python script. The provided script can generates general test cases with scattered sync and wait on various points based on given number of clients, instruments and orders.

- Test case generate strategy:
  - First, we generated 1 client test cases with increasing number of instruments and orders
  - Then, we generated multi-client (2, 4, 10, 20, 40 clients) test cases with increasing number of instruments and orders
