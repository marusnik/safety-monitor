#include <cstdint>
#include <cstddef>
#include <cassert>

#include "frame_queue.hpp"




int main()
    {
    RingBuffer<std::int32_t, 8>    circular_buffer;
    std::int32_t                   out;

    assert( ( circular_buffer.full() == false ) && "Buffer should be empty at this point" );
    assert( circular_buffer.empty() == true );
    assert( ( circular_buffer.pop( out ) == false ) && "There should be nothing to pop at this point" );
    assert( circular_buffer.push( 5 ) == PushResult::ADDED );
    assert( circular_buffer.size() == 1 );
    assert( ( circular_buffer.pop( out ) == true ) && ( out == 5 ) );
    assert( circular_buffer.size() == 0 );
    assert( ( circular_buffer.full() == false ) && "Buffer should be empty at this point" );
    assert( circular_buffer.empty() == true );

    assert( circular_buffer.push( 0 )  == PushResult::ADDED );
    assert( circular_buffer.push( 1 )  == PushResult::ADDED );
    assert( circular_buffer.push( 2 )  == PushResult::ADDED );
    assert( circular_buffer.push( 3 )  == PushResult::ADDED );
    assert( circular_buffer.push( 4 )  == PushResult::ADDED );
    assert( circular_buffer.push( 5 )  == PushResult::ADDED );
    assert( circular_buffer.push( 6 )  == PushResult::ADDED );
    assert( circular_buffer.push( 7 )  == PushResult::ADDED );
    assert( circular_buffer.size() == 8 );
    assert( circular_buffer.push( 8 )  == PushResult::ADDED_DROPPED_OLDEST );
    assert( circular_buffer.size() == 8 );
    assert( circular_buffer.push( 9 )  == PushResult::ADDED_DROPPED_OLDEST );
    assert( circular_buffer.size() == 8 );
    assert( circular_buffer.push( 10 ) == PushResult::ADDED_DROPPED_OLDEST );

    assert( circular_buffer.full() == true );
    assert( circular_buffer.size() == 8 );

    assert( ( circular_buffer.pop( out ) == true ) && ( out == 3 ) );
    assert( ( circular_buffer.pop( out ) == true ) && ( out == 4 ) );
    assert( ( circular_buffer.pop( out ) == true ) && ( out == 5 ) );
    assert( circular_buffer.push( 3 ) == PushResult::ADDED );
    assert( circular_buffer.push( 4 ) == PushResult::ADDED );
    assert( circular_buffer.push( 5 ) == PushResult::ADDED );
    assert( ( circular_buffer.pop( out ) == true ) && ( out == 6  ) );
    assert( ( circular_buffer.pop( out ) == true ) && ( out == 7  ) );
    assert( ( circular_buffer.pop( out ) == true ) && ( out == 8  ) );
    assert( ( circular_buffer.pop( out ) == true ) && ( out == 9  ) );
    assert( circular_buffer.full() == false );
    assert( ( circular_buffer.pop( out ) == true ) && ( out == 10 ) );
    assert( ( circular_buffer.pop( out ) == true ) && ( out == 3  ) );
    assert( ( circular_buffer.pop( out ) == true ) && ( out == 4  ) );
    assert( ( circular_buffer.pop( out ) == true ) && ( out == 5  ) );
    assert( circular_buffer.empty() == true );

    return( 0 );

    }      