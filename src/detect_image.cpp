/*************************************************************************************
                                      INCLUDES
*************************************************************************************/
#include <iostream>
#include <stdexcept>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include "person_detector.hpp"


/*************************************************************************************
                                      FUNCTIONS
*************************************************************************************/

int main( int argc, char* argv[] )
    {
    if ( argc < 2 )
        {
        std::cerr << "Usage: " << argv[0] << " <image>\n";
        return( 1 );
        }

    try
        {
        PersonDetector    detector( "models/frozen_inference_graph.pb",
                                    "models/ssd_mobilenet_v2_coco_2018_03_29.pbtxt",
                                    0.3f );

        cv::Mat           image = cv::imread( argv[1] );

        if ( image.empty() )
            {
            std::cerr << "Could NOT read image: " << argv[1] << '\n';
            return( 1 );
            }

        const std::vector<Detection>    detections = detector.detect( image );
        std::cout << "Persons found: " << detections.size() << '\n';
        
        for ( int idx = 0; idx < detections.size(); ++idx )
            {
            const Detection& d = detections[ idx ];
            std::cout << "  box " << d.box << "  conf " << d.confidence << '\n';
            cv::rectangle( image, d.box, cv::Scalar( 0, 255, 0 ), 2 );
            }

        cv::imwrite( "result.jpg", image );
        std::cout << "Saved result.jpg\n";

        }
    catch ( const std::exception& e )
        {
        std::cerr << e.what() << '\n';
        return( 1 );
        }

    return( 0 );

    } /* main() */