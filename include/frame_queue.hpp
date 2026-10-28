#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <cstdint>
#include <cstddef>
#include <cassert>




/*************************************************************************************
                                      CLASSES
*************************************************************************************/

enum class PushResult { ADDED, 
                        ADDED_DROPPED_OLDEST };

template < typename T, std::size_t Capacity>
class RingBuffer {

    static_assert( Capacity > 0, "RingBuffer Capacity must be at least 1" );

    public:
        PushResult push(T value);   // add a value; returns weater the frame was dropped or not
        bool pop(T& out);           // remove the oldest value into 'out'; returns false if empty
        bool empty() const;
        bool full() const;
        std::size_t size() const;   // how many values are currently stored

    private:
        // your design
        T           buffer[ Capacity ]{};
        std::size_t read_idx     = 0;
        std::size_t write_idx    = 0;
        std::size_t size_in_buff = 0;
    };




/*************************************************************************************
                                    FUNCTIONS
*************************************************************************************/

/*****************************************************************************
*
*   PROCEDURE NAME:
*       push
*
*   DESCRIPTION:
*       Push onto the buffer. Start dropping the oldest value if not enough
*       space
*
*****************************************************************************/


template <typename T, std::size_t Capacity>
PushResult RingBuffer<T, Capacity>::push(T value)
    {

    buffer[ write_idx ] = value;
    write_idx = ( write_idx + 1 ) % Capacity;        

    if ( size_in_buff < Capacity )
        {
        size_in_buff++;
        return( PushResult::ADDED );
        }
    else
        {
        read_idx = ( read_idx + 1 ) % Capacity;
        return( PushResult::ADDED_DROPPED_OLDEST );
        }

    }  /* push() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       pop
*
*   DESCRIPTION:
*       Pop the oldest kept value from the buffer, if there is anything to pop
*
*****************************************************************************/

template <typename T, std::size_t Capacity>
bool RingBuffer<T, Capacity>::pop(T& out)
    {

    if( size_in_buff != 0 )
        {
        out = buffer[ read_idx ];
        read_idx = ( read_idx + 1 ) % Capacity;
        size_in_buff--;
        return( true );
        }
    else
        {
        return( false );
        }

    }  /* pop() */    


/*****************************************************************************
*
*   PROCEDURE NAME:
*       empty
*
*   DESCRIPTION:
*       Is buffer empty?
*
*****************************************************************************/

template <typename T, std::size_t Capacity>
bool RingBuffer<T, Capacity>::empty() const
    {

    if( size_in_buff == 0 )
        {
        return( true );
        }
    else
        {
        return( false );
        }

    }  /* empty() */    


/*****************************************************************************
*
*   PROCEDURE NAME:
*       full
*
*   DESCRIPTION:
*       Is buffer full?
*
*****************************************************************************/

template <typename T, std::size_t Capacity>
bool RingBuffer<T, Capacity>::full() const
    {

    if ( size_in_buff == Capacity )
        {
        return( true );
        }
    else
        {
        return( false );
        }

    }  /* full() */      



template <typename T, std::size_t Capacity>
std::size_t RingBuffer<T, Capacity>::size() const
    {

    return( size_in_buff );

    }  /* full() */