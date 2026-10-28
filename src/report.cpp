#include <iostream>
#include <string>
#include <cstdint>
#include <format>
#include "report.hpp"

/*****************************************************************************
*
*   PROCEDURE NAME:
*       print_row
*
*   DESCRIPTION:
*       Print measurements to standard output in a specified format for 
*       readability
*
*****************************************************************************/

void print_row( const std::string& label, 
                double             value, 
                int                decimals )
    {
    std::cout << std::format( "{:<40}{:>10.{}f}\n", label, value, decimals );
    }


/*****************************************************************************
*
*   PROCEDURE NAME:
*       print_row
*
*   DESCRIPTION:
*       Print counts to standard output in a specified format for readability
*
*****************************************************************************/

void print_row( const std::string& label, 
                std::uint64_t      value )
    {
    std::cout << std::format( "{:<40}{:>10}\n", label, value );
    }


/*****************************************************************************
*
*   PROCEDURE NAME:
*       print_row
*
*   DESCRIPTION:
*       Print text to standard output in a specified format for readability
*
*****************************************************************************/

void print_row( const std::string& label, 
                const std::string& value )
    {
    std::cout << std::format( "{:<40}{:>10}\n", label, value );
    }


/*****************************************************************************
*
*   PROCEDURE NAME:
*       print_statistics
*
*   DESCRIPTION:
*       Prints capture and processing statistics of interes
*
*****************************************************************************/

void print_statistics( const CaptureStats  & cstats,
                       const ProcessStats  & pstats,
                       const std::uint32_t & cam_run_period )
    {
    /************************************************************
     Print capture and processing statistics
    ************************************************************/
    std::cout << "--------------------------------------------------\n";
    print_row( "Captured FPS: ",                   static_cast<double>( cstats.captured )  / static_cast<double>( cam_run_period ) );
    print_row( "Processed FPS: ",                  static_cast<double>( pstats.processed ) / static_cast<double>( cam_run_period ) );
    print_row( "Dropped frames: ",                 cstats.dropped );
    print_row( "Failed captures: ",                cstats.failed );
    print_row( "Timeouts: ",                       pstats.timeouts );
    print_row( "Processed frames: ",               pstats.processed );
    print_row( "Frames with detection: ",          pstats.num_of_proc_frames_with_detection );
    print_row( "Capture Frame Interval (1/FPS): ", 1000.0 * static_cast<double>( cam_run_period )  / static_cast<double>( cstats.captured ) );
    if ( pstats.processed > 0 )
        {
        print_row( "Average Pipeline Latency [ms]: ",         pstats.latency_sum_ms        / static_cast<double>( pstats.processed ) );
        print_row( "Average Detection Time [ms]: ",           pstats.detection_time_sum_ms / static_cast<double>( pstats.processed ) );
        print_row( "Average Frame Wait Time in Queue [ms]: ", pstats.frame_age_sum_ms      / static_cast<double>( pstats.processed ) );
        }
    else
        {
        std::cout << "Average Latency [ms]: n/a (no processed frames)\n";
        }
    print_row( "Max Latency [ms]: ", pstats.latency_max_ms );
    print_row( "Accounting: ",      ( cstats.captured == pstats.processed + cstats.dropped ? "OK" : "MISMATCH" ) );
    std::cout << "--------------------------------------------------\n";

    } /* print_statistics */