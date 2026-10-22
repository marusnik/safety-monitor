#include <iostream>
#include <opencv2/opencv.hpp>
#include <chrono>

int main()
    {


    constexpr std::uint32_t     WARMUP_READ_FRAME_NUM =  10;
    constexpr std::uint32_t     FPS_READ_FRAME_NUM    = 100;
    std::uint32_t               idx;    // loop variable
    cv::Mat                     frame;

    cv::VideoCapture cap(0, cv::CAP_V4L2);   // 0 = /dev/video0; V4L2 = the Linux video system

    if( !cap.isOpened() )
        {
        std::cout << "Camera couldn't be open\n";
        return( 0 );
        }

    for( idx = 0; idx < WARMUP_READ_FRAME_NUM; idx++ )
        {
        cap.read( frame );
        }

    auto t0 = std::chrono::steady_clock::now();

    for( idx = 0; idx < FPS_READ_FRAME_NUM; idx++ )
        {
        cap.read( frame );
        }

    auto t1 = std::chrono::steady_clock::now();

    double seconds = std::chrono::duration<double>(t1 - t0).count();


    if ( !cap.read( frame ) )
        {
        std::cout << "Camera couldn't read the frame\n";
        return( 0 );
        }

    if ( frame.empty() )
        {
        std::cout << "Camera couldn't read the frame\n";
        return( 0 );
        }

    std::uint32_t    pic_width  = frame.cols;
    std::uint32_t    pic_height = frame.rows;

    std::cout << "Captured pic width is "  << pic_width                    << "\n";
    std::cout << "Captured pic height is " << pic_height                   << "\n";
    std::cout << "Frames per second is "   << FPS_READ_FRAME_NUM / seconds << "\n";

    cv::imwrite("frame.jpg", frame);         // save as JPEG

    }