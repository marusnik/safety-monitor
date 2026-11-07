/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <atomic>
#include <thread>
#include <opencv2/opencv.hpp> 
#include <iostream>
#include <chrono>
#include <cstdint>
#include <optional>
#include <cmath>

#include "frame.hpp"
#include "thread_safe_queue.hpp"
#include "person_detector.hpp"
#include "report.hpp"
#include "vl53l1x_sensor.hpp"
#include "zone_monitor.hpp"
#include "zone_sample.hpp"


/*************************************************************************************
                                      CLASSES
*************************************************************************************/
/************************************************************
 In normal light detector is slower than camera. Choosing 
 Capacity = 1 means less latency since frames get dropped
************************************************************/
using FrameQueue = ThreadSafeQueue< Frame, 1 >;


/*************************************************************************************
                                    FUNCTIONS
*************************************************************************************/

/*****************************************************************************
*
*   PROCEDURE NAME:
*       camera_loop
*
*   DESCRIPTION:
*       Camera thread: captures timestamped frames into the queue until
*       stopped, then closes the queue.
*
*****************************************************************************/

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
         Mark as failed if camera can't read or image is empty
        ************************************************************/
        if ( !cap.read( f.image ) 
          || f.image.empty() ) 
            {
            ++stats.failed;
            std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) ); // in case camera is unplugged no need to burn the CPU
            continue;
            }

        /************************************************************
         Fill the rest of the Frame, timestamp and order number
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


/*****************************************************************************
*
*   PROCEDURE NAME:
*       processing_loop
*
*   DESCRIPTION:
*       Processing thread: runs person detection on the newest frame and
*       records timing statistics; a timeout means the camera went silent.
*
*****************************************************************************/

void processing_loop( FrameQueue&     queue, 
                      ProcessStats&   stats,
                      PersonDetector& detector )
    {
    /************************************************************
                            CONSTANTS
    ************************************************************/
    constexpr int64_t    WAIT_FOR_NEW_FRAME_MS = 500;

    /************************************************************
                            VARIABLES
    ************************************************************/
    Frame    f;

    /************************************************************
     Loop until shutdown
    ************************************************************/
    while ( true ) 
        {

        /************************************************************
         Pop the frame
        ************************************************************/
        PopResult r = queue.pop_wait( f, std::chrono::milliseconds( WAIT_FOR_NEW_FRAME_MS ) );

        /************************************************************
         Normal shutdown
        ************************************************************/
        if ( r == PopResult::CLOSED ) 
            {                   
            break;
            }

        /************************************************************
         If camera timed out, go to Stop (continue for now, TODO)
        ************************************************************/
        if ( r == PopResult::TIMEOUT ) 
            {
            ++stats.timeouts;
            std::cerr << "No frame for " << WAIT_FOR_NEW_FRAME_MS << " ms: camera silent\n";
            continue;
            }

        /************************************************************
         Calculate average frame age for statistics
        ************************************************************/
        if ( r == PopResult::POPPED ) 
            {
            stats.frame_age_sum_ms = stats.frame_age_sum_ms + std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - f.captured_at ).count();
            }

        /************************************************************
         Detect people in the image; time it for statistics
        ************************************************************/
        const auto    t0 = std::chrono::steady_clock::now();
        const std::vector<Detection>    detections = detector.detect( f.image );
        const auto    t1 = std::chrono::steady_clock::now();

        stats.detection_time_sum_ms = stats.detection_time_sum_ms + std::chrono::duration<double, std::milli>( t1 - t0 ).count();

        /************************************************************
         Detect people in the image
        ************************************************************/
        if ( !detections.empty() )
            {
            ++stats.num_of_proc_frames_with_detection;
            }
        
        /************************************************************
         Calculate latency, between when the image was pushed onto 
         the queue to after pop and detection (now)

         NOTE: This time does NOT include time it takes light to hit
         the sensor (negligable), the camera's own exposure (longer in 
         dim conditions), internal processing, and USB transfer. 
         Latency time is in reality larger and must be budgeted 
         separately.
        ************************************************************/
        double milli_sec = 
        std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - f.captured_at ).count();

        stats.latency_sum_ms = stats.latency_sum_ms + milli_sec;

        if ( milli_sec > stats.latency_max_ms )
            {
            stats.latency_max_ms = milli_sec;
            }

        ++stats.processed;

        } /* while( true ) */

    } /* processing_loop() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       sensor_loop()
*
*   DESCRIPTION:
*       Sensor thread: the only owner of the ToF driver. Turns each reading
*       into a zone and publishes it with a timestamp, so that readers can
*       detect a hung or dead thread. Any failure publishes STOP.
*
*****************************************************************************/

void sensor_loop( Vl53l1xSensor&               sensor,
                  std::atomic<ZoneSample>&     latest,
                  const std::atomic<bool>&     running )
    {
    /************************************************************
     Short pause after a failed read, so that a dead bus can't
     spin this thread at 100% CPU
    ************************************************************/
    constexpr auto  RETRY_DELAY = std::chrono::milliseconds( 10 );

    ZoneMonitor     monitor;    // starts in STOP

    try
        {
        while ( running )
            {
            /************************************************************
             read() blocks until data is ready (up to 
             ~NUM_OF_POLL_ATTEMPTS ms), so it sets the loop rate; 
             no sleep needed on success
            ************************************************************/
            const std::optional<RangeReading> reading = sensor.read();

            /************************************************************
             Only a status-0 reading gives a distance; anything else
             goes to the monitor as nullopt, which means STOP
            ************************************************************/
            std::optional<std::uint16_t> distance_mm;

            if ( reading
              && reading->valid() )
                {
                distance_mm = reading->distance_mm;
                }

            /************************************************************
             Publish even on failure, because "sensor not working, so
             STOP" is fresh information too
            ************************************************************/
            const Zone zone = monitor.update( distance_mm );
            latest.store( ZoneSample{ zone, now_ms() } );

            if ( !reading )
                {
                std::this_thread::sleep_for( RETRY_DELAY );
                }
            }
        }
    catch ( const std::exception& e )
        {
        /************************************************************
         Uncaught exception in a std::thread calls std::terminate.
         Publish STOP and exit; staleness keeps readers in STOP
        ************************************************************/
        latest.store( ZoneSample{ Zone::STOP, now_ms() } );
        std::cerr << "Sensor thread failed: " << e.what() << '\n';
        }
    catch ( ... )
        {
        latest.store( ZoneSample{ Zone::STOP, now_ms() } );
        std::cerr << "Sensor thread failed: unknown exception\n";
        }

    } /* sensor_loop() */
    

/*****************************************************************************
*
*   PROCEDURE NAME:
*       make_detector
*
*   DESCRIPTION:
*       Creates the person detector; returns std::nullopt if the model
*       cannot be loaded.
*
*****************************************************************************/

std::optional<PersonDetector> make_detector( float min_confidence )
    {
    try
        {
        /************************************************************
         Consturct a detector object with specified minimum confidence
        ************************************************************/
        return PersonDetector( "models/frozen_inference_graph.pb",
                               "models/ssd_mobilenet_v2_coco_2018_03_29.pbtxt",
                               min_confidence );
        }
    catch ( const std::exception& e )
        {
        std::cerr << "Detector setup failed: " << e.what() << '\n';
        return std::nullopt;
        }

    } /* make_detector */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       zone_name()
*
*   DESCRIPTION:
*       Returns a printable name for a zone
*
*****************************************************************************/

const char* zone_name( Zone zone )
    {
    switch ( zone )
        {
        case Zone::CLEAR: return "CLEAR";
        case Zone::SLOW:  return "SLOW";
        case Zone::STOP:  return "STOP";
        }

    return "UNKNOWN";   // unreachable, but makes -Wall/-Wextra happy

    } /* zone_name() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       main
*
*   DESCRIPTION:
*       Runs camera, detector and sensor threads for a fixed
*       period and prints statistics.
*
*****************************************************************************/

int main()
    {
    /*************************************************************************************
                                        CONSTANTS
    *************************************************************************************/
    constexpr std::uint32_t     WARMUP_READ_FRAME_NUM =  10;
    constexpr std::uint32_t     CAM_RUN_PERIOD_SEC    =  60;    /* has to be different than zero */
    constexpr float             MIN_CONFIDENCE        =  0.3f;

    static_assert( CAM_RUN_PERIOD_SEC > 0, "Run period must be positive" );

    /*************************************************************************************
                                        VARIABLES
    *************************************************************************************/
    std::atomic<bool>       running{ true };
    FrameQueue              queue;
    CaptureStats            cstats;
    ProcessStats            pstats;
    ZoneSample              initial;
    std::atomic<ZoneSample> latest_zone;
    

    /************************************************************
     Open video capture; if not successfully, throw an error
     0 = /dev/video0
     V4L2 = the Linux video system
    ************************************************************/
    cv::VideoCapture cap( 0, cv::CAP_V4L2 );  

    if( !cap.isOpened() )
        {
        std::cerr << "Camera couldn't be opened\n";
        return( 1 );
        }

    /************************************************************
     Make sure autofocus is off and set to proper distance 
     (don't assume camera is set the way we need). Experiment 
     showed the value of 11 is appropriate to focus at 1-2m where 
     the detection is most important (LifeCam HD-5001).
    ************************************************************/
    constexpr long FOCUS_ABSOLUTE = 11;

    if ( !cap.set( cv::CAP_PROP_AUTOFOCUS, 0.0 ) 
      || !cap.set( cv::CAP_PROP_FOCUS, static_cast<double>( FOCUS_ABSOLUTE ) ) )
        {
        std::cerr << "Warning: could not set fixed focus\n";
        }

    /************************************************************
     Get the set values back and make sure they are set.
     It's safe to compare doubles with the chosen values here 
     (since round), but they might change in the future so 
     converting to integers first
    ************************************************************/
    if ( std::lround( cap.get( cv::CAP_PROP_AUTOFOCUS ) ) != 0
      || std::lround( cap.get( cv::CAP_PROP_FOCUS     ) ) != FOCUS_ABSOLUTE )
        {
        std::cerr << "Error: camera did not apply the requested focus settings\n";
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

    std::optional<PersonDetector> detector = make_detector( MIN_CONFIDENCE );

    /************************************************************
     Missing model files, exit before any thread exists
    ************************************************************/ 
    if ( !detector )
        {
        return( 1 );
        }

    /************************************************************
     Create the sensor before any thread starts, so that a
     startup failure exits cleanly instead of calling
     std::terminate on joinable threads
    ************************************************************/
    std::optional<Vl53l1xSensor> sensor;
    try
        {
        sensor.emplace( "/dev/i2c-1", ToF_SENSOR_ADDR );
        }
    catch ( const std::exception& e )
        {
        std::cerr << "Sensor setup failed: " << e.what() << '\n';
        return( 1 );
        }

    /************************************************************
     Run one warm-up detection on a frame before starting the 
     processing thread. The first detection takes longer, and 
     we don't want it to throw off latency statistics
    ************************************************************/ 
    if( !frame.empty() )
        {
        detector->detect( frame );
        }
    else
        {
        std::cerr << "No frame from camera during warm-up\n";
        return( 1 );
        }

    /************************************************************
     Spin up the capture and processing threads
    ************************************************************/
    std::thread camera( [ & ] { camera_loop( cap, queue, running, cstats ); } );
    std::thread worker( [ & ] { processing_loop( queue, pstats, *detector ); } );

    /************************************************************
     Start with a stale initial zone
    ************************************************************/
    initial.zone    = Zone::STOP;
    initial.time_ms = now_ms() - STALE_ZONE_LIMIT_MS - 1;

    latest_zone.store( initial );

    /************************************************************
     Spin up the sensor thread
    ************************************************************/
    std::thread sensor_thread( [ & ] { sensor_loop( *sensor, 
                                                     latest_zone,
                                                     running ); } );

    /************************************************************
     Run for CAM_RUN_PERIOD_SEC, checking the zone every 100 ms.
     Print only changes, so that a zone you walk through is easy
     to see, and the console isn't flooded
    ************************************************************/
    constexpr auto ZONE_CHECK_PERIOD = std::chrono::milliseconds( 100 );

    const auto run_until = std::chrono::steady_clock::now()
                         + std::chrono::seconds( CAM_RUN_PERIOD_SEC );

    std::optional<Zone> last_printed;   // empty, so the first zone always prints

    while ( std::chrono::steady_clock::now() < run_until )
        {
        const Zone zone = current_zone( latest_zone );

        if ( !last_printed 
          || *last_printed != zone )
            {
            std::cout << std::format( "[{:>6} ms] zone: {}\n", now_ms() % 1000000, zone_name( zone ) );
            last_printed = zone;
            }

        std::this_thread::sleep_for( ZONE_CHECK_PERIOD );
        }

    running = false;

    camera.join();
    worker.join();
    sensor_thread.join();

    /************************************************************
     Print capture and processing statistics
    ************************************************************/
    print_statistics( cstats, 
                      pstats, 
                      CAM_RUN_PERIOD_SEC );

    return( 0 );

    } /* main() */