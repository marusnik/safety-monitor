/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <stdexcept>
#include <chrono>
#include <iostream>
#include "person_detector.hpp"


/*************************************************************************************
                                    FUNCTIONS
*************************************************************************************/

/*****************************************************************************
*
*   PROCEDURE NAME:
*       PersonDetector
*
*   DESCRIPTION:
*       Create PersonDetector object and throw an error if not able to read
*       network model
*
*****************************************************************************/

PersonDetector::PersonDetector( const std::string&    model_path,
                                const std::string&    config_path,
                                float                 min_confidence )
    : net_( cv::dnn::readNetFromTensorflow( model_path, config_path ) ),
      min_confidence_( min_confidence )
    {
    if ( net_.empty() )
        {
        throw std::runtime_error( "Model could NOT be loaded: " + model_path );
        }

    } /* PersonDetector::PersonDetector() */


/*****************************************************************************
*
*   PROCEDURE NAME:
*       detect
*
*   DESCRIPTION:
*       Return detection object (box and confidence) for provided image
*
*****************************************************************************/

std::vector<Detection> PersonDetector::detect( const cv::Mat& image )
    {
    constexpr int      PERSON_CLASS_ID = 1;       /* COCO class 1 = person      */

    std::vector<Detection>    detections;

    /************************************************************
     Create a blob out of image
     SSD MobileNet v2 expects a 300x300 RGB input
    ************************************************************/   
    cv::Mat    blob = cv::dnn::blobFromImage( image, 
                                              1.0,                  
                                              cv::Size( 300, 300 ), 
                                              cv::Scalar(), 
                                              true, 
                                              false );

    /************************************************************
     Set the blob as a new input value for the network
    ************************************************************/
    net_.setInput( blob );

    /************************************************************
     Process the image
    ************************************************************/
    cv::Mat    output = net_.forward();

    /************************************************************
     output shape: [1, 1, N, 7] (first two can be ignored, anways
     ones)
     Last member in [1, 1, N, 7] contains seven floats
     [_, class, conf, x1, y1, x2, y2]
     So, treat output as N rows of 7 floats (two dimensional 
     array)
    ************************************************************/    
    cv::Mat    rows( output.size[2], output.size[3], CV_32F, output.ptr<float>() );

    for ( int idx = 0; idx < rows.rows; ++idx )
        {
        /************************************************************
         For each detection make sure it's a pearson with a minimum 
         specified confidence
        ************************************************************/  
        const int      class_id   = static_cast<int>( rows.at<float>( idx, 1 ) );
        const float    confidence = rows.at<float>( idx, 2 );

        if ( class_id != PERSON_CLASS_ID 
          || confidence < min_confidence_ )
            {
            continue;
            }

        /************************************************************
         Coordinates are normalised 0..1 -> Scale to image size
        ************************************************************/          
        const int    x1 = static_cast<int>( rows.at<float>( idx, 3 ) * static_cast<float>( image.cols ) );
        const int    y1 = static_cast<int>( rows.at<float>( idx, 4 ) * static_cast<float>( image.rows ) );
        const int    x2 = static_cast<int>( rows.at<float>( idx, 5 ) * static_cast<float>( image.cols ) );
        const int    y2 = static_cast<int>( rows.at<float>( idx, 6 ) * static_cast<float>( image.rows ) );

        /************************************************************
         Append the detection after clipping
        ************************************************************/  
        detections.push_back( { cv::Rect( cv::Point( x1, y1 ), cv::Point( x2, y2 ) )
                              & cv::Rect( 0, 0, image.cols, image.rows ), 
                              confidence } );
        }

    return( detections );

    } /* PersonDetector::detect() */