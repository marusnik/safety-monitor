/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <atomic>
#include <thread>
#include <opencv2/opencv.hpp> 
#include <iostream>
#include <chrono>
#include <cstdint>
#include "frame.hpp"
#include "thread_safe_queue.hpp"


/*************************************************************************************
                                      CLASSES
*************************************************************************************/
using FrameQueue = ThreadSafeQueue<Frame, 4>;     

struct CaptureStats { std::uint64_t captured = 0;           // written ONLY by the camera thread
                      std::uint64_t failed   = 0;
                      std::uint64_t dropped  = 0;
    };

struct ProcessStats { std::uint64_t processed      = 0;     // written ONLY by the processing thread
                      std::uint64_t timeouts       = 0;
                      double        latency_sum_ms = 0.0;
                      double        latency_max_ms = 0.0;
    };


/*************************************************************************************
                                    FUNCTIONS
*************************************************************************************/

void camera_loop( cv::VideoCapture &         cap, 
                  FrameQueue &               queue,
                  const std::atomic<bool> &  running, 
                  CaptureStats &             stats )
    {

    std::uint64_t    sequence = 0;

    while ( running )                              // checks the flag every iteration
        {                              
        Frame f;                                   // fresh Frame → fresh pixel buffer

        /************************************************************
         Mark as failed if capture can't read or image is empty
        ************************************************************/
        if ( !cap.read( f.image ) 
          || f.image.empty() ) 
            {
            ++stats.failed;
            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) ); // in case camera is unplugged no need to burn the CPU
            continue;
            }

        /************************************************************
         Fill the rest of the Frame
        ************************************************************/            
        f.captured_at = std::chrono::steady_clock::now();
        f.sequence    = sequence++;

        /************************************************************
         Push into the queue and count drops
        ************************************************************/        
        if ( queue.push( std::move( f ) ) == PushResult::ADDED_DROPPED_OLDEST )
            {
            ++stats.dropped;
            }

        ++stats.captured;

        } /* while ( running )*/

    /************************************************************
     Camera finished: let the consumer exit
    ************************************************************/        
    queue.close();

    } /* camera_loop() */




void processing_loop( FrameQueue&   queue, 
                      ProcessStats& stats )
    {

    Frame f;

    while ( true ) 
        {

        /************************************************************
         Pop the frame
        ************************************************************/
        PopResult r = queue.pop_wait( f, std::chrono::milliseconds( 500 ) );

        /************************************************************
         Normal shutdown
        ************************************************************/
        if ( r == PopResult::CLOSED ) 
            {                   
            break;
            }

        /************************************************************
         If camera timed out, go to Stop (continue for now)
        ************************************************************/
        if ( r == PopResult::TIMEOUT ) 
            {
            ++stats.timeouts;
            std::cerr << "No frame for 500 ms: camera silent\n";
            continue;
            }

        /************************************************************
         Grayscale and blurre the image
        ************************************************************/
        cv::Mat    gray;
        cv::Mat    blurred;

        cv::cvtColor( f.image, gray, cv::COLOR_BGR2GRAY );
        cv::GaussianBlur( gray, blurred, cv::Size( 9, 9 ), 0 );
        
        /************************************************************
         Calculate latency, between when the image was captured to now
        ************************************************************/
        double milli_sec = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - f.captured_at).count();

        stats.latency_sum_ms = stats.latency_sum_ms + milli_sec;

        if ( milli_sec > stats.latency_max_ms )
            {
            stats.latency_max_ms = milli_sec;
            }

        ++stats.processed;

        } /* while( true ) */

    } /* processing_loop() */



int main()
    {
    /*************************************************************************************
                                        CONSTANTS
    *************************************************************************************/
    constexpr std::uint32_t     WARMUP_READ_FRAME_NUM =  10;
    constexpr std::uint32_t     CAM_RUN_PERIOD_SEC    =  10;

    /*************************************************************************************
                                        VARIABLES
    *************************************************************************************/
    std::atomic<bool> running{ true };
    FrameQueue        queue;
    CaptureStats      cstats;
    ProcessStats      pstats;

    /************************************************************
    Open video capture; if not successfully, throw an error
    ************************************************************/
    cv::VideoCapture cap( 0, cv::CAP_V4L2 );   /* 0 = /dev/video0; V4L2 = the Linux video system */

    if( !cap.isOpened() )
        {
        std::cerr << "Camera couldn't be opened\n";
        return( 1 );
        }

    /************************************************************
    Waste a few reads until camera stabilize
    ************************************************************/ 
    cv::Mat            frame;

    for( std::uint32_t idx = 0; idx < WARMUP_READ_FRAME_NUM; idx++ )
        {
        cap.read( frame );
        }

    /************************************************************
     Spin up the capture and processing threads
    ************************************************************/
    std::thread camera( [ & ] { camera_loop( cap, queue, running, cstats ); } );
    std::thread worker( [ & ] { processing_loop( queue, pstats ); } );

    /************************************************************
     Run for a while and then ask the camera thread to stop
    ************************************************************/
    std::this_thread::sleep_for( std::chrono::seconds( CAM_RUN_PERIOD_SEC ) );
    running = false;

    camera.join();
    worker.join();

    /************************************************************
     Print capture and processing statistics
    ************************************************************/    
    std::cout << "---------------------------------\n"
              << "Frames Per Second [ms]: " << static_cast<double>( cstats.captured ) / static_cast<double>( CAM_RUN_PERIOD_SEC ) << "\n"
              << "Processed FSP [ms]: "     << static_cast<double>( pstats.processed ) / static_cast<double>( CAM_RUN_PERIOD_SEC ) << "\n"
              << "Dropped Frames: "         << cstats.dropped << "\n"
              << "Failed to Capture: "      << cstats.failed << "\n"
              << "Number of Timeouts: "     << pstats.timeouts << "\n"
              << "Processed Frames: "       << pstats.processed << "\n"
              << "Average Latency [ms]: "   << pstats.latency_sum_ms / static_cast<double>( pstats.processed ) << "\n"
              << "Max Latency [ms]: "       << pstats.latency_max_ms << "\n"
              << "Accounting " << ( cstats.captured == pstats.processed + cstats.dropped ? "OK" : "MISMATCH" ) << "\n"
              << "---------------------------------\n";

    return( 0 );

    } /* main() */