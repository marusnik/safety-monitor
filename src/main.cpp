
#include <iostream>
#include <cstdint>
#include <opencv2/opencv.hpp>
#include <chrono>

#include "frame.hpp"
#include "frame_queue.hpp"




int measure_capture_fps( cv::VideoCapture& cap )
    {
    /*************************************************************************************
                                        CONSTANTS
    *************************************************************************************/
    constexpr std::uint32_t     FPS_READ_FRAME_NUM    = 100;

    /************************************************************
    Make number of reads and time it for Frame Per Second (FPS)
    calculation. Use just sucessful reads for calculation, and 
    report failed ones
    ************************************************************/
    cv::Mat            last_good_frame;
    std::uint32_t      num_failed_frame_reads = 0;
    cv::Mat            frame;

    auto time_0 = std::chrono::steady_clock::now();
    
    for( std::uint32_t idx = 0; idx < FPS_READ_FRAME_NUM; idx++ )
        {
        if ( !cap.read( frame ) || frame.empty() )
            {
            num_failed_frame_reads++;
            }
        else
            {
            frame.copyTo( last_good_frame );
            }        
        }

    auto time_1 = std::chrono::steady_clock::now();

    if ( num_failed_frame_reads != 0 )
        {
        std::cerr << "Number of failed frame reads is " << num_failed_frame_reads << "\n";
        }

    /************************************************************
    Report last suceffully captured frame's size and FPS
    ************************************************************/
    if ( last_good_frame.empty() )
        {
        std::cerr << "No frames captured\n";
        return( 1 );
        }
    else
        {
        const double seconds = std::chrono::duration<double>( time_1 - time_0 ).count();

        std::cout << "Captured pic width is "  << last_good_frame.cols << "\n";
        std::cout << "Captured pic height is " << last_good_frame.rows << "\n";
        std::cout << "Frames per second is "   << ( ( FPS_READ_FRAME_NUM - num_failed_frame_reads ) / seconds ) << "\n";

        /************************************************************
        Save last frame as jpg image
        ************************************************************/
        if ( !cv::imwrite( "frame.jpg", last_good_frame ) )
            {
            std::cerr << "Last good frame NOT saved\n";
            return( 1 );
            }

        return( 0 );

        }

    } /* measure_capture_fps() */


int frame_copy_experiment( cv::VideoCapture& cap )
    {

    RingBuffer< Frame, 4 >    buffer;

    Frame                     test_frame;
    Frame                     shared_frame;
    cv::Mat                   shared_image;


    for( std::uint32_t idx=0; idx < 4; idx++ )
        {
        cap.read( shared_image );
        shared_frame.image = shared_image.clone();
        shared_frame.captured_at = std::chrono::steady_clock::now();
        shared_frame.sequence = idx;

        buffer.push( shared_frame );
        }

    for( std::uint32_t idx=0; idx < 4; idx++ )
        {
        buffer.pop( test_frame );
        std::cout << static_cast<void*>( test_frame.image.data ) << "\n";
        cv::imwrite( "frame_" + std::to_string( idx ) +".jpg", test_frame.image );
        }

    return( 0 );

    } /* frame_copy_experiment() */


int main()
    {
    /*************************************************************************************
                                        CONSTANTS
    *************************************************************************************/
    constexpr std::uint32_t     WARMUP_READ_FRAME_NUM =  10;

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
    Report Frames per Second, and save last sucessful image
    ************************************************************/ 
    if ( measure_capture_fps( cap ) != 0 )
        {
        return( 1 );
        }

    /************************************************************
    Copy experiment to make sure images are different
    ************************************************************/ 
    if ( frame_copy_experiment( cap ) != 0 )
        {
        return( 1 );
        }

    return( 0 );    

    }