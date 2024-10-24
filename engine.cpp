#include <iostream>
#include <thread>
#include <string>
#include <unordered_map>
#include <queue>
#include <mutex>

#include "io.hpp"
#include "engine.hpp"


struct Order 
{
	uint32_t order_id;
	uint32_t price;
	uint32_t count;
	int64_t timestamp;
	bool is_cancelled;
	bool is_buy;
	uint32_t execution_id;
	std::string instrument_name;

	Order(uint32_t order_id, uint32_t price, uint32_t count, int64_t timestamp, bool is_buy, uint32_t execution_id, std::string instrument_name)
		: order_id(order_id), price(price), count(count), timestamp(timestamp), is_cancelled(false), is_buy(is_buy), execution_id(execution_id), instrument_name(instrument_name) {}

	/* Override operator< for sorting
	 * 
	 * @param other: the other order to compare to
	 * if is_cancelled: return false
	 * if buy: highest price first, then earliest timestamp
	 * else (sell): lowest price first, then earliest timestamp
	 */ 
	bool operator<(const Order& other) const 
	{
		if (is_cancelled) 
		{
			return false;
		}
		if (price != other.price) {
			if (is_buy) {
				return price < other.price;
			} else {
				return price > other.price;
			}
		} else {
			return timestamp > other.timestamp;
		}
	}
};

struct InstrumentBook {
    std::string instrumentName; 
    std::priority_queue<Order> buyOrders;
    std::priority_queue<Order> sellOrders;

    mutable std::mutex instrumentMutex;

    InstrumentBook(std::string instrumentName, std::priority_queue<Order> buyOrders, std::priority_queue<Order> sellOrders)
		: instrumentName(instrumentName), buyOrders(buyOrders), sellOrders(sellOrders) {}

	InstrumentBook(std::string instrumentName)
		: instrumentName(instrumentName), buyOrders(std::priority_queue<Order>()), sellOrders(std::priority_queue<Order>()) {}

	InstrumentBook (const InstrumentBook& other)
	{
		std::lock_guard<std::mutex> lock{other.instrumentMutex};
		instrumentName = other.instrumentName;
		buyOrders = other.buyOrders;
		sellOrders = other.sellOrders;
	}
};

std::unordered_map<std::string, InstrumentBook> instrumentLookup;
std::unordered_map<uint32_t, std::pair<std::string, bool>> orderLookup; // order_id -> (instrument_name, is_buy)

std::mutex instrumentLookupMutex;
std::mutex orderLookupMutex;

void Engine::accept(ClientConnection connection)
{
	auto thread = std::thread(&Engine::connection_thread, this, std::move(connection));
	thread.detach();
}

void cancelOrder(uint32_t order_id)
{
    // check if order exists
	orderLookupMutex.lock();
    auto orderIt = orderLookup.find(order_id);
    if (orderIt == orderLookup.end()) 
    {
		Output::OrderDeleted(order_id, false, getCurrentTimestamp());

        orderLookupMutex.unlock();
        return;
    }

	std::string instrument = orderIt->second.first;
	bool is_buy = orderIt->second.second;
	orderLookupMutex.unlock();

	// find the instrument book corresponding to the order's instrument
	instrumentLookupMutex.lock();
	auto instrumentIt = instrumentLookup.find(instrument);
	if (instrumentIt == instrumentLookup.end()) 
	{
		orderLookupMutex.lock();
		auto orderIt = orderLookup.find(order_id);
		if (orderIt != orderLookup.end()) 
		{
			orderLookup.erase(order_id);
		}
		Output::OrderDeleted(order_id, false, getCurrentTimestamp());

		orderLookupMutex.unlock();
		instrumentLookupMutex.unlock();
		return;
	}


	// remove the order from the instrument
	instrumentIt->second.instrumentMutex.lock();

	instrumentLookupMutex.unlock();

	bool found = false;

	if (is_buy)
	{
		std::priority_queue<Order> newBuyOrders;
		while (!instrumentIt->second.buyOrders.empty()) 
		{
			Order buyOrder = instrumentIt->second.buyOrders.top();
			instrumentIt->second.buyOrders.pop();
			if (buyOrder.order_id == order_id) 
			{
				found = true;
				buyOrder.is_cancelled = true;
				Output::OrderDeleted(order_id, true, getCurrentTimestamp());

				orderLookupMutex.lock();
				auto orderIt = orderLookup.find(order_id);
				if (orderIt != orderLookup.end()) 
				{
					orderLookup.erase(order_id);
				}
				orderLookupMutex.unlock();
				continue;
			}
			newBuyOrders.push(buyOrder);
		}
		instrumentIt->second.buyOrders = newBuyOrders;
	}
	else 
	{
		std::priority_queue<Order> newSellOrders;
		while (!instrumentIt->second.sellOrders.empty()) 
		{
			Order sellOrder = instrumentIt->second.sellOrders.top();
			instrumentIt->second.sellOrders.pop();
			if (sellOrder.order_id == order_id) 
			{
				found = true;
				sellOrder.is_cancelled = true;
				Output::OrderDeleted(order_id, true, getCurrentTimestamp());

				orderLookupMutex.lock();
				auto orderIt = orderLookup.find(order_id);
				if (orderIt != orderLookup.end()) 
				{
					orderLookup.erase(order_id);
				}
				orderLookupMutex.unlock();

				continue;
			}
			newSellOrders.push(sellOrder);
		}
		instrumentIt->second.sellOrders = newSellOrders;
	}

	if (!found)
		Output::OrderDeleted(order_id, false, getCurrentTimestamp());

	instrumentIt->second.instrumentMutex.unlock();
}


void addBuyOrder(uint32_t order_id, uint32_t price, uint32_t count, char* instrument)
{
	// find the instrument book corresponding to the new buy order's instrument
	instrumentLookupMutex.lock();
	auto instrumentIt = instrumentLookup.find(instrument);
	if (instrumentIt == instrumentLookup.end()) 
	{
		Order newOrder = Order(order_id, price, count, getCurrentTimestamp(), true, 0, instrument);
		std::priority_queue<Order> newBuyOrders;
		newBuyOrders.push(newOrder);

		instrumentLookup.emplace(instrument, InstrumentBook(instrument, newBuyOrders, std::priority_queue<Order>()));

		orderLookupMutex.lock();
		orderLookup.emplace(order_id, std::make_pair(instrument, true));
		
		Output::OrderAdded(order_id, instrument, price, count, false, newOrder.timestamp);

		orderLookupMutex.unlock();

		instrumentLookupMutex.unlock();
		
		return;
	}

	instrumentIt->second.instrumentMutex.lock();

	instrumentLookupMutex.unlock();


	// while there is a matching sell order and count > 0
	while (!instrumentIt->second.sellOrders.empty() 
		&& !instrumentIt->second.sellOrders.top().is_cancelled
		&& instrumentIt->second.sellOrders.top().price <= price 
		&& count > 0) 
	{
		Order sellOrder = instrumentIt->second.sellOrders.top();
		instrumentIt->second.sellOrders.pop();


		uint32_t executionCount = std::min(count, sellOrder.count);
		count -= executionCount;
		sellOrder.count -= executionCount;
		sellOrder.execution_id += 1;

		Output::OrderExecuted(sellOrder.order_id, order_id, sellOrder.execution_id, sellOrder.price, executionCount, getCurrentTimestamp());

		if (sellOrder.count > 0) {
			instrumentIt->second.sellOrders.push(sellOrder);
			instrumentIt->second.instrumentMutex.unlock();
			return;
		}
		else 
		{
			orderLookupMutex.lock();
			auto orderIt = orderLookup.find(sellOrder.order_id);
			if (orderIt != orderLookup.end()) 
			{
				orderLookup.erase(sellOrder.order_id);
			}	
			orderLookupMutex.unlock();
		}
	}

	if (count > 0) 
	{
		Order newOrder = Order(order_id, price, count, getCurrentTimestamp(), true, 0, instrument);

		Output::OrderAdded(order_id, instrument, price, count, false, newOrder.timestamp);
		
		orderLookupMutex.lock();
		orderLookup.emplace(order_id, std::make_pair(instrument, true));
		orderLookupMutex.unlock();

		instrumentIt->second.buyOrders.push(newOrder);
	}
	instrumentIt->second.instrumentMutex.unlock();
}

void addSellOrder(uint32_t order_id, uint32_t price, uint32_t count, char* instrument)
{
	// lock the instrument 
	instrumentLookupMutex.lock();
	auto instrumentIt = instrumentLookup.find(instrument);
	if (instrumentIt == instrumentLookup.end()) 
	{
		Order newOrder = Order(order_id, price, count, getCurrentTimestamp(), false, 0, instrument);
		std::priority_queue<Order> newSellOrders;
		newSellOrders.push(newOrder);

		instrumentLookup.emplace(instrument, InstrumentBook(instrument, std::priority_queue<Order>(), newSellOrders));

		orderLookupMutex.lock();
		orderLookup.emplace(order_id, std::make_pair(instrument, false));
		
		Output::OrderAdded(order_id, instrument, price, count, true, newOrder.timestamp);

		orderLookupMutex.unlock();
		instrumentLookupMutex.unlock();

		return;
	}

	instrumentIt->second.instrumentMutex.lock();
	instrumentLookupMutex.unlock();

	// while there is a matching buy order and count > 0
	while (!instrumentIt->second.buyOrders.empty() 
		&& !instrumentIt->second.buyOrders.top().is_cancelled 
		&& instrumentIt->second.buyOrders.top().price >= price 
		&& count > 0) 
	{
		Order buyOrder = instrumentIt->second.buyOrders.top();
		instrumentIt->second.buyOrders.pop();

		uint32_t executionCount = std::min(count, buyOrder.count);
		count -= executionCount;
		buyOrder.count -= executionCount;
		buyOrder.execution_id += 1;

		Output::OrderExecuted(buyOrder.order_id, order_id, buyOrder.execution_id, buyOrder.price, executionCount, getCurrentTimestamp());

		if (buyOrder.count > 0) 
		{
			instrumentIt->second.buyOrders.push(buyOrder);
			instrumentIt->second.instrumentMutex.unlock();
			return;
		}
		else 
		{
			orderLookupMutex.lock();
			auto orderIt = orderLookup.find(buyOrder.order_id);
			if (orderIt != orderLookup.end()) 
			{
				orderLookup.erase(buyOrder.order_id);
			}
			orderLookupMutex.unlock();
		}

	}

	if (count > 0) 
	{
		Order newOrder = Order(order_id, price, count, getCurrentTimestamp(), false, 0, instrument);

		Output::OrderAdded(order_id, instrument, price, count, true, newOrder.timestamp);
		

		orderLookupMutex.lock();
		orderLookup.emplace(order_id, std::make_pair(instrument, false));
		orderLookupMutex.unlock();

		instrumentIt->second.sellOrders.push(newOrder);
	}
	instrumentIt->second.instrumentMutex.unlock();
}

void Engine::connection_thread(ClientConnection connection)
{
	while(true)
	{
		ClientCommand input {};
		switch(connection.readInput(input))
		{
			case ReadResult::Error: SyncCerr {} << "Error reading input" << std::endl;
			case ReadResult::EndOfFile: return;
			case ReadResult::Success: break;
		}

		switch(input.type)
		{
			case input_cancel:
			{
				cancelOrder(input.order_id);
				break;
			}
			default:
			{
				if (input.price < 0 || input.count < 0) 
				{
					SyncCerr {} << "Invalid input" << std::endl;
					return;
				}
				if (input.type == input_buy)
					addBuyOrder(input.order_id, input.price, input.count, input.instrument);
				else if (input.type == input_sell)
					addSellOrder(input.order_id, input.price, input.count, input.instrument);
			}
		}	
	}
}
