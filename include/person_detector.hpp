#pragma once

/*************************************************************************************
                                    INCLUDES
*************************************************************************************/
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>


/*************************************************************************************
                                      CLASSES
*************************************************************************************/

struct Detection { 
                   cv::Rect    box;
                   float       confidence;
                 };

class PersonDetector {
                       public:
                           PersonDetector( const std::string&    model_path, 
                                           const std::string&    config_path,
                                           float                 min_confidence );
                           std::vector<Detection> detect( const cv::Mat& image );

                       private:
                           cv::dnn::Net     net_;
                           float            min_confidence_;
                     };