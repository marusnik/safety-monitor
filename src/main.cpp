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
#include <poll.h>
#include <unistd.h>
#include <cerrno>

#include "frame.hpp"
#include "thread_safe_queue.hpp"
#include "person_detector.hpp"
#include "report.hpp"
#include "vl53l1x_sensor.hpp"
#include "zone_monitor.hpp"
#include "zone_sample.hpp"
#include "camera_sample.hpp"
#include "safety_output.hpp"
#include "safety_latch.hpp"


/*************************************************************************************
                                     CONSTANTS
*************************************************************************************/

inline constexpr const char* COLOR_RESET = "\033[0m";


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

void processing_loop( FrameQueue&                    queue, 
                      ProcessStats&                  stats,
                      PersonDetector&                detector,
                      std::atomic<CameraSample>&     latest_cam_sample )
    {
    /************************************************************
                            CONSTANTS
    ************************************************************/
    constexpr int64_t    WAIT_FOR_NEW_FRAME_MS = 500;

    /************************************************************
                            VARIABLES
    ************************************************************/
    Frame            f;
    std::uint32_t    timeouts_in_row = 0; // how many timeout happened in a row 

    /************************************************************
     Loop until shutdown
    ************************************************************/
    while ( true ) 
        {
        /************************************************************
         Pop the frame from the queue
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

            /************************************************************
             Log only when the camera goes silent and when it comes
             back, because one line per timeout hides the lines
             that matter
            ************************************************************/
            if ( 0 == timeouts_in_row )
                {
                std::cerr << std::format( "[{} ms] camera silent: no frame for {} ms\n", now_ms(), WAIT_FOR_NEW_FRAME_MS );
                }

            ++timeouts_in_row;
            continue;
            }

        if ( timeouts_in_row > 0 )
            {
            std::cerr << std::format( "[{} ms] camera back after {} timeouts\n", now_ms(), timeouts_in_row );
            timeouts_in_row = 0;
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

        const bool person = !detections.empty();
       
        /************************************************************
         Publish every processed frame; timestamp is the camera's 
         proof of life. Staleness should include time since the image
         was taken (so use captured_at, instead of now_ms)
        ************************************************************/
        latest_cam_sample.store( CameraSample{ person, to_ms( f.captured_at ) } );

        if ( person )
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
                  const std::atomic<bool>&     running 
#ifdef FAULT_INJECTION
                  , std::atomic<bool>&         inject_fault
#endif
                                                       )
    {
    /************************************************************
     Short pause after a failed read, so that a dead bus can't
     spin this thread at 100% CPU
    ************************************************************/
    constexpr auto  RETRY_DELAY = std::chrono::milliseconds( 10 );

    ZoneMonitor     monitor;    
    std::uint32_t   failed_reads       = 0;
    std::uint32_t   invalid_readings   = 0;
    std::uint8_t    last_logged_status = 0;
    std::string     last_logged_error;

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

#ifdef FAULT_INJECTION
            /************************************************************
             Drop the distance as if the reading were invalid
            ************************************************************/
            if ( inject_fault )
                {
                distance_mm.reset();   
                }
#endif

            /************************************************************
             Publish even on failure, because "sensor not working, so
             STOP" is fresh information too
            ************************************************************/
            const Zone zone = monitor.update( distance_mm );
            const bool fault = !distance_mm;
            latest.store( ZoneSample{ zone, fault, now_ms() } );


            /************************************************************
             Log only on changes (first failure, different error,
             recovery), because one line per failed read hides the
             line that matters, and writing to cerr that often can
             affect timing
            ************************************************************/
            if ( !reading )
                {
                if ( 0 == failed_reads 
                  || sensor.last_error() != last_logged_error )
                    {
                    std::cerr << std::format( "[{} ms] sensor read failed: {}\n", now_ms(), sensor.last_error() );
                    last_logged_error = sensor.last_error();
                    }

                ++failed_reads;
                std::this_thread::sleep_for( RETRY_DELAY );
                }

            else if ( failed_reads > 0 )
                {
                std::cerr << std::format( "[{} ms] sensor recovered after {} failed reads\n", now_ms(), failed_reads );
                failed_reads = 0;
                last_logged_error.clear();
                }

            /************************************************************
             The read worked, but the sensor says the distance can't be
             trusted (non-zero range status)
            ************************************************************/
            if ( reading )
                {
                if ( !reading->valid() )
                    {
                    /************************************************************
                     Log on change only: first invalid status, a different status, 
                     and back to valid
                    ************************************************************/
                    if ( 0 == invalid_readings
                      || reading->status != last_logged_status )
                        {
                        std::cerr << std::format( "[{} ms] sensor range status {} (distance {} mm)\n",
                                                  now_ms(), reading->status, reading->distance_mm );
                        last_logged_status = reading->status;
                        }
                
                    ++invalid_readings;
                    }
                
                /************************************************************
                 Back to valid readings
                ************************************************************/
                else if ( invalid_readings > 0 )
                    {
                    std::cerr << std::format( "[{} ms] sensor status valid again after {} invalid readings\n",
                                              now_ms(), invalid_readings );
                    invalid_readings   = 0;
                    last_logged_status = 0;
                    }

                } /* if ( reading ) */

            } /*  while ( running ) */

        } /* try */

    catch ( const std::exception& e )
        {
        /************************************************************
         Uncaught exception in a std::thread calls std::terminate.
         Publish STOP and exit; staleness keeps readers in STOP
        ************************************************************/
        latest.store( ZoneSample{ Zone::STOP, true, now_ms() } );
        std::cerr << "Sensor thread failed: " << e.what() << '\n';
        }
    catch ( ... )
        {
        latest.store( ZoneSample{ Zone::STOP, true, now_ms() } );
        std::cerr << "Sensor thread failed: unknown exception\n";
        }

    } /* sensor_loop() */
    

/*****************************************************************************
*
*   PROCEDURE NAME:
*       keyboard_loop()
*
*   DESCRIPTION:
*       Keyboard thread: 'r' + Enter requests a reset ('f' toggles an
*       injected fault in test builds, FAULT_INJECTION needed). Polls with a 
*       timeout so it can stop at shutdown.
*
*****************************************************************************/

void keyboard_loop( std::atomic<bool>&          reset_requested,
                    const std::atomic<bool>&    running 
#ifdef FAULT_INJECTION
                  , std::atomic<bool>&          inject_fault
#endif
                                                                )
   {

    while ( running )
        {
        /************************************************************
         Wake every 100 ms to check if the program is still running
        ************************************************************/
        pollfd pfd{ STDIN_FILENO, POLLIN, 0 };
        const int ready = poll( &pfd, 1, 100 ); 

        if ( ready < 0 )
            {
            /************************************************************
             Interrupted by a signal: just retry 
            ************************************************************/
            if ( errno == EINTR ) 
                {
                continue;
                }

            std::cerr << "keyboard: poll failed, reset unavailable\n";
            return;
            }

        /************************************************************
         Timeout: no key, check running again
        ************************************************************/
        if ( ready == 0 ) 
            {
            continue;
            }

        /************************************************************
         We have a key press, read it
        ************************************************************/
        char buf[ 64 ];
        const ssize_t n = read( STDIN_FILENO, buf, sizeof( buf ) );

        if ( n <= 0 )                              // EOF or error: input is gone
            {
            std::cerr << "keyboard: input closed, reset unavailable\n";
            return;
            }

        /************************************************************
         If any of the pressed keys is 'r' we have a reset
        ************************************************************/
        for ( ssize_t i = 0; i < n; ++i )
            {
            if ( buf[ i ] == 'r' ) 
               { 
               reset_requested = true;
               }

#ifdef FAULT_INJECTION
            /************************************************************
             Inject/clear a fault with 'f'
            ************************************************************/
            if ( buf[ i ] == 'f' )
                {
                const bool on = !inject_fault.load();
                inject_fault  = on;
                std::cerr << std::format( "[{} ms] fault injection {}\n", now_ms(), on ? "ON" : "OFF" );
                }
#endif

            } /* for() */

        } /* while() */

   } /* keyboard_loop() */  


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
*       TODO: move all the *_name() and *_color() into a separate file
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
*       zone_color()
*
*   DESCRIPTION:
*       ANSI terminal colors, so zone changes stand out in the log
*
*****************************************************************************/

inline const char* zone_color( Zone z )
    {
    switch ( z )
        {
        case Zone::CLEAR: return "\033[32m";   // green
        case Zone::SLOW:  return "\033[33m";   // yellow
        case Zone::STOP:  return "\033[31m";   // red
        }

    return "\033[0m";

    } /* zone_color() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       cam_name()
*
*   DESCRIPTION:
*       Returns a printable name for a person present or not
*
*****************************************************************************/

const char* cam_name( bool person_present )
    {
    switch ( person_present )
        {
        case true:  return "Person Present";
        case false: return "Person NOT Present";
        }

    return "UNKNOWN";   // unreachable, but makes -Wall/-Wextra happy

    } /* cam_name() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       cam_color()
*
*   DESCRIPTION:
*       ANSI terminal colors, so person detection stand out in the log
*
*****************************************************************************/

inline const char* cam_color( bool person_present )
    {
    switch ( person_present )
        {
        case true:  return "\033[32m";   // green
        case false: return "\033[31m";   // red
        }

    return "\033[0m";

    } /* cam_color() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       latch_name()
*
*   DESCRIPTION:
*       Returns a printable name for the current latch state
*
*****************************************************************************/

const char* latch_name( LatchState s )
    {
    switch ( s )
        {
        case LatchState::OK:            return "OK";
        case LatchState::FAULT_PENDING: return "FAULT_PENDING";
        case LatchState::LATCHED:       return "LATCHED";
        }

    return "UNKNOWN";   // unreachable, but makes -Wall/-Wextra happy

    } /* latch_name() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       latch_color()
*
*   DESCRIPTION:
*       ANSI terminal colors, so latch state changes stand out in the log
*
*****************************************************************************/

inline const char* latch_color( LatchState latch_state )
    {
    switch ( latch_state )
        {
        case LatchState::OK:             return "\033[32m";   // green
        case LatchState::FAULT_PENDING:  return "\033[33m";   // yellow
        case LatchState::LATCHED:        return "\033[31m";   // red
        }

    return "\033[0m";

    } /* latch_color() */


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
    constexpr std::uint32_t     CAM_RUN_PERIOD_SEC    =  120;    /* !!! has to be different than zero */
    constexpr float             MIN_CONFIDENCE        =   0.3f;

    static_assert( CAM_RUN_PERIOD_SEC > 0, "Run period must be positive" );

    /*************************************************************************************
                                        VARIABLES
    *************************************************************************************/
    std::atomic<bool>         running{ true };
    FrameQueue                queue;
    CaptureStats              cstats;
    ProcessStats              pstats;
    ZoneSample                zone_initial;
    CameraSample              camera_initial;
    std::atomic<ZoneSample>   latest_zone_sample;
    std::atomic<CameraSample> latest_cam_sample;
    std::atomic<bool>         reset_requested{ false };
#ifdef FAULT_INJECTION
    std::atomic<bool>         inject_fault{ false }; 
    std::cerr << "\033[31mWARNING: fault injection enabled (test build), press 'f' to toggle a sensor fault\033[0m\n";
 #endif

    /************************************************************
     Open video capture; if not successfully, throw an error
     0 = /dev/video0 (or open by the stable name, 
                      /def/video0 sometimes changes to video1 after replugs)
     V4L2 = the Linux video system
    ************************************************************/
    //cv::VideoCapture cap( 0, cv::CAP_V4L2 );  
    cv::VideoCapture cap( "/dev/v4l/by-id/usb-Microsoft_Microsoft®_LifeCam_HD-5001-video-index0", cv::CAP_V4L2 );

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
     Start with a stale assumption that a person is present
    ************************************************************/
    camera_initial.person = true;
    camera_initial.time_ms = now_ms() - STALE_CAMERA_LIMIT_MS - 1;

    latest_cam_sample.store( camera_initial );

    /************************************************************
     Spin up the capture and processing threads
    ************************************************************/
    std::thread camera( [ & ] { camera_loop( cap, 
                                             queue, 
                                             running, 
                                             cstats ); } );

    std::thread worker( [ & ] { processing_loop( queue, 
                                                 pstats, 
                                                *detector, 
                                                 latest_cam_sample ); } );

    /************************************************************
     Spin up the keyboard thread monitoring the reset button 'r'
    ************************************************************/
    std::thread keyboard_thread( [ & ] { keyboard_loop( reset_requested, 
                                                        running
#ifdef FAULT_INJECTION
                                                      , inject_fault
#endif
                                                                         ); } );

    /************************************************************
     Start with a faulty stale initial zone
    ************************************************************/
    zone_initial.zone    = Zone::STOP;
    zone_initial.fault   = true;
    zone_initial.time_ms = now_ms() - STALE_ZONE_LIMIT_MS - 1;

    latest_zone_sample.store( zone_initial );

    /************************************************************
     Spin up the sensor thread
    ************************************************************/
    std::thread sensor_thread( [ & ] { sensor_loop( *sensor, 
                                                     latest_zone_sample,
                                                     running
#ifdef FAULT_INJECTION
                                                   , inject_fault
#endif
                                                             ); } );

    /************************************************************
     Run for CAM_RUN_PERIOD_SEC, checking the zone every 100 ms.
     Print only changes, so that a zone you walk through is easy
     to see, and the console isn't flooded
    ************************************************************/
    constexpr auto ZONE_CHECK_PERIOD = std::chrono::milliseconds( 100 );

    const auto run_until = std::chrono::steady_clock::now()
                         + std::chrono::seconds( CAM_RUN_PERIOD_SEC );

    SafetyLatch               latch;
    std::optional<Zone>       last_printed_zone;
    std::optional<LatchState> last_printed_latch_state;

    while ( std::chrono::steady_clock::now() < run_until )
        {
        const ZoneSample    sensor_sample = latest_zone_sample.load();
        const CameraSample  camera_sample = latest_cam_sample.load();
        const std::uint32_t now           = now_ms();

        /************************************************************
         Are there any faults detected?
        ************************************************************/
        const bool fault = is_stale( sensor_sample.time_ms, now, STALE_ZONE_LIMIT_MS   )
                        || is_stale( camera_sample.time_ms, now, STALE_CAMERA_LIMIT_MS )
                        || sensor_sample.fault;

        /************************************************************
         Is the reset button pressed?
        ************************************************************/
        const bool reset = reset_requested.exchange( false );

        /************************************************************
         Decide what the zone should be based on sensors regardless
         if they are faulting
        ************************************************************/
        const Zone normal = safety_output( sensor_sample, 
                                           camera_sample, 
                                           now );

        /************************************************************
         Latch if there are significant faults, and request human 
         interaction (button press) to restart
        ************************************************************/
        const LatchState before_latch_state = latch.state();
        const Zone zone = latch.update_latch( fault,
                                              reset,
                                              now,
                                              normal );
        const LatchState state = latch.state();
        
        /************************************************************
         Report each reset press, so the log says whether it was
         accepted or why it was ignored
        ************************************************************/
        if ( reset )
            {
            const char* result = "ignored: not latched";

            if ( before_latch_state == LatchState::LATCHED )
                {
                /************************************************************
                 Report sucessful reset if transitioning from LATCHED to OK
                ************************************************************/
                if ( state == LatchState::OK )
                    {
                    result = "accepted";
                    }
                /************************************************************
                 Otherwise fault is still present, reset ignored
                ************************************************************/
                else
                    {
                    result = "ignored: fault present";
                    }
                }

            std::cout << std::format( "[{:>6} ms] reset {}\n", now, result );               
            }
        
        /************************************************************
         Print on a zone change or a latch state change (and always
         the first time). Check for empty before dereferencing
        ************************************************************/
        if ( !last_printed_zone  
          || *last_printed_zone  != zone
          || !last_printed_latch_state 
          || *last_printed_latch_state != state )
            {
            /************************************************************
             Show "stale" instead of the last value, so the log says
             why the output is STOP
            ************************************************************/
            const bool sensor_stale = is_stale( sensor_sample.time_ms, now, STALE_ZONE_LIMIT_MS );
            const bool camera_stale = is_stale( camera_sample.time_ms, now, STALE_CAMERA_LIMIT_MS );

            const char* sensor_text  = sensor_stale ? "STALE"    : zone_name( sensor_sample.zone );
            const char* sensor_color = sensor_stale ? "\033[31m" : zone_color( sensor_sample.zone );

            const char* camera_text  = camera_stale ? "STALE"    : cam_name( camera_sample.person );
            const char* camera_color = camera_stale ? "\033[31m" : cam_color( camera_sample.person );
        
            std::cout << std::format( "[{:>6} ms] zone: {}{}{} latch: {}{}{} (sensor: {}{}{}, camera: {}{}{})\n",
                                      now,
                                      zone_color( zone ),   zone_name( zone ),   COLOR_RESET,
                                      latch_color( state ), latch_name( state ), COLOR_RESET,
                                      sensor_color,         sensor_text,         COLOR_RESET,
                                      camera_color,         camera_text,         COLOR_RESET );
        
            last_printed_zone        = zone;
            last_printed_latch_state = state;
            }

        std::this_thread::sleep_for( ZONE_CHECK_PERIOD );
        }

    running = false;

    camera.join();
    worker.join();
    keyboard_thread.join();
    sensor_thread.join();

    /************************************************************
     Print capture and processing statistics
    ************************************************************/
    print_statistics( cstats, 
                      pstats, 
                      CAM_RUN_PERIOD_SEC );

    return( 0 );

    } /* main() */