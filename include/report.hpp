#pragma once

/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <string>
#include <cstdint>


/*************************************************************************************
                                      CLASSES
*************************************************************************************/
/************************************************************
 Written ONLY by the camera thread
************************************************************/
struct CaptureStats { std::uint64_t captured = 0;
                      std::uint64_t failed   = 0;
                      std::uint64_t dropped  = 0;
    };
/************************************************************
 Written ONLY by the processing thread
************************************************************/
struct ProcessStats { std::uint64_t processed                         = 0;
                      std::uint64_t timeouts                          = 0;
                      std::uint64_t num_of_proc_frames_with_detection = 0;
                      double        latency_sum_ms                    = 0.0;
                      double        latency_max_ms                    = 0.0;
                      double        detection_time_sum_ms             = 0.0;
                      double        frame_age_sum_ms                  = 0.0;
    };

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
                int                decimals = 2 );


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
                std::uint64_t      value );


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
                const std::string& value );


/*****************************************************************************
*
*   PROCEDURE NAME:
*       print_statistics
*
*   DESCRIPTION:
*       Prints capture and processing statistics of interest
*
*****************************************************************************/

void print_statistics( const CaptureStats  & cstats,
                       const ProcessStats  & pstats,
                       const std::uint32_t & cam_run_period );