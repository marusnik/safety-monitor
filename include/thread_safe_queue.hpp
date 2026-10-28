#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <chrono>
#include <condition_variable>
#include <mutex>
#include "frame_queue.hpp"


/*************************************************************************************
                                      CLASSES
*************************************************************************************/
enum class PopResult { POPPED, 
                       TIMEOUT, 
                       CLOSED };


template <typename T, std::size_t Capacity>
class ThreadSafeQueue {

    public:
        PushResult                 push( T value );                                // camera thread
        PopResult                  pop_wait( T&                        out, 
                                             std::chrono::milliseconds timeout ); // processing thread: waits for data
        void                       close();                                       // shutdown: wake up anyone waiting

    private:
        RingBuffer<T, Capacity>    buffer_;                                       // the existing circular buffer
        std::mutex                 mutex_;                                        // protects buffer_ and closed_
        std::condition_variable    not_empty_;
        bool                       closed_ = false;
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
*       Thread safe push function
*
*****************************************************************************/

template <typename T, std::size_t Capacity>
PushResult ThreadSafeQueue<T, Capacity>::push( T value )
    {
    
    PushResult    ret_val;

        {
        std::lock_guard<std::mutex> lock( mutex_ );
        ret_val = buffer_.push( std::move( value ) );
        }

    not_empty_.notify_one();

    return( ret_val );

    }  /* push() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       close
*
*   DESCRIPTION:
*       Thread safe close function
*
*****************************************************************************/

template <typename T, std::size_t Capacity>
void ThreadSafeQueue<T, Capacity>::close()
    {
        {
        std::lock_guard<std::mutex> lock( mutex_ );
        closed_ = true;
        }
    
    not_empty_.notify_all();

    } /* close() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       close
*
*   DESCRIPTION:
*       Thread safe pop function
*
*****************************************************************************/

template <typename T, std::size_t Capacity>
PopResult ThreadSafeQueue<T, Capacity>::pop_wait( T&                        out, 
                                                  std::chrono::milliseconds timeout )
    {

    std::unique_lock<std::mutex> lock( mutex_ );

    not_empty_.wait_for( lock, timeout, [ this ] { return !buffer_.empty() || closed_; } );

    /************************************************************
     Pop even if closed_ is true. No captured frame is silently 
     lost at shutdown (for Capacity of one)
    ************************************************************/
    if ( buffer_.pop( out ) )
        {
        return( PopResult::POPPED );
        }

    /************************************************************
     Return closed only when frames are not in the queue any more
     (for Capacity of one)
    ************************************************************/
    if ( closed_ )
        {
        return( PopResult::CLOSED );
        }

    return( PopResult::TIMEOUT );
     
    } /* pop_wait() */