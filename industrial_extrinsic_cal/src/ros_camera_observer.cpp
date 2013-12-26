/*
 * ros_camera_observer.cpp
 *
 *  Created on: Dec 18, 2013
 *
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2011, Southwest Research Institute
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of the Southwest Research Institute, nor the names
 * of its contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <industrial_extrinsic_cal/ros_camera_observer.h>

namespace industrial_extrinsic_cal {


ROSCameraObserver::ROSCameraObserver(std::string camera_topic):
		sym_circle_(true),	pattern_(Chessboard), pattern_rows_(0), pattern_cols_(0)
{
	image_topic_=camera_topic;
	results_pub_ = nh_.advertise<sensor_msgs::Image>("observer_results_image", 100);
}


void ROSCameraObserver::addTarget(boost::shared_ptr<Target> targ, Roi roi)
{
	//set pattern based on target
	ROS_INFO_STREAM("Target type: "<<targ->target_type);
	instance_target_=targ;
	if (targ->target_type==0)
	{
		pattern_=Chessboard;
	}
	else if (targ->target_type==1)
	{
		pattern_=CircleGrid;
	}
	else if (targ->target_type==2)
	{
		pattern_=ARtag;
	}
	else
		ROS_ERROR_STREAM("Unknown target_type");

	if (pattern_ !=0 && pattern_ !=1 && pattern_!= 2)
	{
		ROS_ERROR_STREAM("Unknown pattern, based on target_type");
	}

	//set pattern rows/cols based on target
	switch (pattern_)
	{
	case Chessboard:
		pattern_rows_=targ->checker_board_parameters.pattern_rows;
		pattern_cols_=targ->checker_board_parameters.pattern_cols;
		break;
	case CircleGrid:
		pattern_rows_=targ->circle_grid_parameters.pattern_rows;
		pattern_cols_=targ->circle_grid_parameters.pattern_cols;
		sym_circle_=targ->circle_grid_parameters.is_symmetric;
		break;
	case ARtag:

		break;
	}

	sensor_msgs::ImageConstPtr recent_image = ros::topic::waitForMessage<sensor_msgs::Image>(image_topic_);
	try
	{
		input_bridge_ = cv_bridge::toCvCopy(recent_image, "mono8");
		output_bridge_ = cv_bridge::toCvCopy(recent_image, "bgr8");
		out_bridge_ = cv_bridge::toCvCopy(recent_image, "mono8");
		ROS_INFO_STREAM("cv image created based on ros image");
	}
	catch (cv_bridge::Exception& ex)
	{
		ROS_ERROR("Failed to convert image");
		return;
	}

	cv::Rect input_ROI(roi.x_min, roi.y_min, roi.x_max-roi.x_min, roi.y_max-roi.y_min);//Rect takes in x,y,width,height
	//ROS_INFO_STREAM("image ROI region created");
	if (input_bridge_->image.cols < input_ROI.width || input_bridge_->image.rows < input_ROI.height)
	{
		ROS_ERROR_STREAM("ROI too big for image size");
	}

	image_roi_ = input_bridge_->image(input_ROI);


	output_bridge_->image=output_bridge_->image(input_ROI);
	out_bridge_->image=image_roi_;
	ROS_INFO_STREAM("output image size: " <<output_bridge_->image.rows<<" x "<<output_bridge_->image.cols);
	results_pub_.publish(out_bridge_->toImageMsg());
}

void ROSCameraObserver::clearTargets()
{
	//instance_target_.reset();
}

void ROSCameraObserver::clearObservations()
{
	//camera_obs_
}

int ROSCameraObserver::getObservations(CameraObservations &cam_obs)
{
	bool successful_find =false;

	switch (pattern_)
	{
	case Chessboard:
		ROS_INFO_STREAM("Finding Chessboard Corners...");
		successful_find = cv::findChessboardCorners(image_roi_, cv::Size(pattern_rows_,pattern_cols_), observation_pts_, cv::CALIB_CB_ADAPTIVE_THRESH);
		break;
	case CircleGrid:
		if (sym_circle_)
		{
			ROS_INFO_STREAM("Finding Circles in grid, symmetric...");
		successful_find = cv::findCirclesGrid(image_roi_, cv::Size(pattern_rows_,pattern_cols_), observation_pts_, cv::CALIB_CB_SYMMETRIC_GRID);
		}
		else
		{
			ROS_INFO_STREAM("Finding Circles in grid, asymmetric...");
		successful_find= cv::findCirclesGrid(image_roi_, cv::Size(pattern_rows_,pattern_cols_), observation_pts_,
				cv::CALIB_CB_ASYMMETRIC_GRID | cv::CALIB_CB_CLUSTERING);
		}
		break;
	}
	if (!successful_find)
		{
			ROS_WARN_STREAM("Pattern not found for pattern: "<<pattern_ <<" with symmetry: "<< sym_circle_);
		}

	ROS_INFO_STREAM("Number of points found on board: "<<observation_pts_.size());
	camera_obs_.observation.resize(observation_pts_.size());
	for (int i = 0; i<observation_pts_.size() ; i++)
	{
		camera_obs_.observation.at(i).target=instance_target_;
		camera_obs_.observation.at(i).point_id=i;
		camera_obs_.observation.at(i).image_loc_x=observation_pts_.at(i).x;
		camera_obs_.observation.at(i).image_loc_y=observation_pts_.at(i).y;
	}

	cam_obs=camera_obs_;
	if (successful_find)
	{
		return 1;
	}
}

}//industrial_extrinsic_cal
