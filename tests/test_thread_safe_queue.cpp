/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include "thread_safe_queue.hpp"




void test_producer_consumer_keeps_order_and_accounts_for_drops();   // the main two-thread test
void test_pop_wait_returns_timeout_when_no_data();                  // nothing pushed → Timeout
//void test_close_wakes_waiting_consumer();                           // consumer asleep, close() → Closed
//void test_items_pushed_before_close_are_still_delivered();          // drain-before-Closed design choice



/*************************************************************************************
                                  FUNCTIONS
*************************************************************************************/

void test_producer_consumer_keeps_order_and_accounts_for_drops()
    {
    constexpr std::int32_t           kCount = 10'000;
    ThreadSafeQueue<std::int32_t, 8> queue;

    std::int32_t dropped = 0;
    std::thread producer([&] {                       // starts running immediately
        for (std::int32_t v = 0; v < kCount; ++v) {
            if (queue.push(v) == PushResult::ADDED_DROPPED_OLDEST) {
                ++dropped;
            }
        }
        queue.close();                               // tell the consumer: no more data
    });

    std::int32_t received = 0;
    std::int32_t last     = -1;
    bool         order_ok = true;
    std::thread consumer([&] {
        std::int32_t value;
        while (queue.pop_wait(value, std::chrono::milliseconds(500)) == PopResult::POPPED) 
            {
            /************************************************************
            Check the values strictly increase, using 'last'
            ************************************************************/
            if ( value <= last )
                {
                std::cerr << "values not strictly increaseing: " << value << "after " << last << "\n";
                order_ok = false;
                }

            last = value;

            received++;
            }
    });

    /************************************************************
     Wait for both threads to finish
    ************************************************************/
    producer.join();  
    consumer.join();

    /************************************************************
     Assert if popped values are not strictly increasing, or
     nuber of popping plus number of dropps is not equal to kCount
    ************************************************************/
    assert( order_ok  && "values not strictly increasing" );

    std::cout << "received is " << received << " and dropped is " << dropped 
              << ", that is " << ( static_cast<double>( dropped ) / static_cast<double>( kCount ) ) * 100.0f 
              << "% of dropped reads" << "\n";

    assert( ( received + dropped == kCount ) && "received + dropped not equal to kCount" );

    }


void test_pop_wait_returns_timeout_when_no_data()
    {
    ThreadSafeQueue<std::int32_t, 8> queue;
    std::int32_t value = 42;

    auto t0 = std::chrono::steady_clock::now();
    PopResult r = queue.pop_wait(value, std::chrono::milliseconds(100));
    auto waited = std::chrono::steady_clock::now() - t0;

    assert(r == PopResult::TIMEOUT);
    assert(value == 42);                                   // untouched
    assert(waited >= std::chrono::milliseconds(100));      // really waited
    }


int main()
    {
    test_producer_consumer_keeps_order_and_accounts_for_drops();
    test_pop_wait_returns_timeout_when_no_data();

    return( 0 );

    }