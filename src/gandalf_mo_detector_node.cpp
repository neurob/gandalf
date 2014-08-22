/*
 * Copyright (C) 2014 by
 *   Neuroinformatics and Cognitive Robotics Labs (NICR) at TU Ilmenau, GERMANY
 * All rights reserved.
 *
 * Contact: christoph.weinrich@tu-ilmenau.de,
 *          tim.wengefeld@tu-ilmenau.de
 *
 * GNU General Public License Usage:
 *   This file may be used under the terms of the GNU General Public License
 *   version 3.0 as published by the Free Software Foundation and appearing in
 *   the file LICENSE.GPL3 included in the packaging of this file. Please review
 *   the following information to ensure the GNU General Public License
 *   version 3.0 requirements will be met: http://www.gnu.org/copyleft/gpl.html.
 *   Alternatively you may (at your option) use any later version of the GNU
 *   General Public License if such license has been publicly approved by NICR.
 *
 * IN NO EVENT SHALL "NICR" BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
 * SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS
 * SOFTWARE AND ITS DOCUMENTATION, EVEN IF "NICR" HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * "NICR" SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND "NICR"
 * HAVE NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS OR
 * MODIFICATIONS.
 */

/*
 * When using this software for your own research, please acknowledge the effort
 * that went into its construction by citing the corresponding paper:
 *
 *   C. Weinrich, T. Wengefeld, C. Schröter and H.-M. Gross
 *   People Detection and Distinction of their Walking Aids in 2D Laser Range
 *   Data based on Generic Distance-Invariant Features.
 *   In Proceedings of the IEEE International Symposium on Robot and Human
 *   Interactive Communication (RO-MAN), 2014, Edinburgh (UK)
 */

#include <ros/ros.h>
#include <std_msgs/String.h>
#include <sensor_msgs/LaserScan.h>
#include <geometry_msgs/PoseArray.h>
#include <visualization_msgs/Marker.h>

#include <robot/RangeScan.h>
#include <geometry/Point.h>

// replace this
#include <GDIFDetector.h>

#include <AdaboostClassifierNodeParams.h>
#include <boost/filesystem.hpp>

using namespace mira::laserbasedobjectdetection;

class GDIFMultiObjectDetectorNode{
public:

	GDIFMultiObjectDetectorNode() : mNodeHandle("~"){
		mAdaBoostParams = boost::shared_ptr<AdaboostClassifierParams>(new AdaboostClassifierParams());
	}

    void init(){
		mHypothesesPoseArrayTopic = mNodeHandle.advertise<geometry_msgs::PoseArray>("hypotheses", 1000);
		mHypothesesMarkerTopic = mNodeHandle.advertise<visualization_msgs::Marker>("hypotheses2", 1000);
		mLaserSub = mNodeHandle.subscribe<sensor_msgs::LaserScan>("laser", 1000, &GDIFMultiObjectDetectorNode::laserCallback, this);

		int tInt;
		double tDouble;

		int tFeatureVectorSize;
		mNodeHandle.param("FeatureVectorSize", tFeatureVectorSize, 45);

		std::vector<boost::shared_ptr<AdaboostClassifierNodeParams>> tAdaboostClassifierNodeParams;

		std::vector<double> tThresholds;
		std::vector<string> tClassifierFiles;
		std::vector<int> tPosLabels;
		std::vector<int> tNegLabels;
		std::vector<int> tPosChilds;
		std::vector<int> tNegChilds;

		if(!mNodeHandle.hasParam("Thresholds")){
			ROS_ERROR("no param thresholds");
		}

		if(!mNodeHandle.getParam("Thresholds", tThresholds)){
			ROS_ERROR("could not get param thresholds");
		}

		mNodeHandle.getParam("ClassifierFiles", tClassifierFiles);
		mNodeHandle.getParam("PosLabels", tPosLabels);
		mNodeHandle.getParam("NegLabels", tNegLabels);
		mNodeHandle.getParam("PosChilds", tPosChilds);
		mNodeHandle.getParam("NegChilds", tNegChilds);

		if(tThresholds.size() != tClassifierFiles.size()){
			ROS_ERROR("tThresholds.size() [%d] != tClassifierFiles.size() [%d]", (int)tThresholds.size(), (int)tClassifierFiles.size());
		}
		if(tPosLabels.size() != tNegLabels.size()){
			ROS_ERROR("tPosLabels.size() [%d] != tNegLabels.size() [%d]", (int)tPosLabels.size(), (int)tNegLabels.size());
		}
		if(tPosChilds.size() != tNegChilds.size()){
			ROS_ERROR("tPosChilds.size() [%d] != tNegChilds.size() [%d]", (int)tPosChilds.size(), (int)tNegChilds.size());
		}
		if(tPosChilds.size() != tPosLabels.size()){
			ROS_ERROR("tPosChilds.size() [%d] != tPosLabels.svn size() [%d]", (int)tPosChilds.size(), (int)tPosLabels.size());
		}
		if(tPosChilds.size() != tThresholds.size()){
			ROS_ERROR("tPosChilds.size() [%d] != tThresholds.size() [%d]", (int)tPosChilds.size(), (int)tThresholds.size());
		}

		for(uint32 i = 0; i < tThresholds.size(); ++i){
			tAdaboostClassifierNodeParams.push_back(boost::shared_ptr<AdaboostClassifierNodeParams>(new AdaboostClassifierNodeParams((StageLabel) tPosLabels[i], (StageLabel) tNegLabels[i], tClassifierFiles[i], tThresholds[i], tFeatureVectorSize)));

			boost::filesystem::path testPath(tClassifierFiles[i]);
			if(!boost::filesystem::exists(testPath)){
				ROS_ERROR("Could not find opencv classifier file: [%s]", tClassifierFiles[i].c_str());
			}
		}
		for(uint32 i = 0; i < tThresholds.size(); ++i){
			if(tPosChilds[i] >= (int)tThresholds.size())
				ROS_ERROR("tPosChilds[i] [%d] >=  tThresholds.size() [%d]", tPosChilds[i], (int)tThresholds.size());
			if(tNegChilds[i] >= (int)tThresholds.size())
				ROS_ERROR("tNegChilds[i] [%d] >=  tThresholds.size() [%d]", tNegChilds[i], (int)tThresholds.size());
			if(tPosChilds[i] >= 0)
				tAdaboostClassifierNodeParams[i]->mPosChild = tAdaboostClassifierNodeParams[tPosChilds[i]];
			if(tNegChilds[i] >= 0)
				tAdaboostClassifierNodeParams[i]->mNegChild = tAdaboostClassifierNodeParams[tNegChilds[i]];
		}

		mNodeHandle.param("JumpDistance", tDouble, 0.1);
		mSegmentationParams.mJumpDistance = tDouble;
		//ROS_INFO("jump distance [%f] ", mParams.mJumpDistance);
		mNodeHandle.param("MaxRange", tDouble, 10.0);
		mSegmentationParams.mMaxRange = tDouble;
		mNodeHandle.param("MinSegmentSize", tInt, 3);
		mSegmentationParams.mMinSegmentSize = tInt;
		mNodeHandle.param("BinQuantity", tInt, 15);
		mBoundingBoxParams.mBinQuantity = tInt;
		mNodeHandle.param("BoxWidth", tDouble, 0.8);
		mBoundingBoxParams.mBoxWidth = tDouble;
		mNodeHandle.param("BoxHeight", tDouble, 3.0);
		mBoundingBoxParams.mBoxHeight = tDouble;
		// the reference point is the center of a segment (0) or the most left point of a segment (1)
		mNodeHandle.param("BoxMode", tInt, 0);
		mBoundingBoxParams.mBoxMode = (BoxMode)tInt;
		mNodeHandle.param("BoxFromLeftOffset", tDouble, -0.3);
		mBoundingBoxParams.mBoxFromLeftOffset = tDouble;
		mNodeHandle.param("UseHighFreqFeats", mBoundingBoxParams.mUseHighFreqFeats, true);

		mGDIFDetector.inititalize(mAdaBoostParams, mSegmentationParams, mBoundingBoxParams);
	};

	/**
	 * This tutorial demonstrates simple receipt of messages over the ROS system.
	 */
	//void chatterCallback(const std_msgs::String::ConstPtr& msg)
	void laserCallback(const sensor_msgs::LaserScan::ConstPtr& laserScan)
	{

		// Converting the ROS range scan to the mira range scan
		RangeScan rangeScan;
		// header
		rangeScan.startAngle = laserScan->angle_min;
		rangeScan.deltaAngle = laserScan->angle_increment;
		// this is a hack
		rangeScan.coneAngle = laserScan->angle_increment;
		/// The aperture of the sensor in [m].
		rangeScan.aperture = 0.0;
		/// The measuring error of the sensor that delivered this scan.
		rangeScan.stdError = 0.0;
		/// The minimum range of valid measurements
		/// (measurements below this value result in BelowMinimum range code).
		rangeScan.minimumRange = laserScan->range_min;
		/// The maximum range of valid measurements
		/// (measurements above this value result in AboveMaximum range code).
		rangeScan.maximumRange = laserScan->range_max;

		// data
		//out.ranges.resize(data->value().range.size());
		//for(unsigned int i = 0; i < data->value().range.size(); ++i) {
		//	out.ranges[i] = data->value().range[i];
		//}
		rangeScan.range = laserScan->ranges;
		rangeScan.valid.resize(rangeScan.range.size(), RangeScan::Valid);
		rangeScan.certainty.clear();
		//out.intensities.resize(data->value().reflectance.size());
		//for(unsigned int i = 0; i < data->value().reflectance.size(); ++i) {
		//	out.intensities[i] = data->value().reflectance[i];
		//}
		rangeScan.reflectance = laserScan->intensities;

		//
		std::vector<Point2f> detections = mGDIFDetector.classifyScan(rangeScan);

		visualization_msgs::Marker marker;
		marker.header.seq = laserScan->header.seq;
		marker.header.stamp = laserScan->header.stamp;
		marker.header.frame_id = laserScan->header.frame_id;

		marker.ns = std::string("people_detections");
		marker.id = 0;
		marker.type = visualization_msgs::Marker::SPHERE_LIST;
		//marker.action = visualization_msgs::Marker::ADD;
		marker.action = visualization_msgs::Marker::MODIFY;
		marker.pose.position.x = 0;
		marker.pose.position.y = 0;
		marker.pose.position.z = 0;
		marker.pose.orientation.x = 0.0;
		marker.pose.orientation.y = 0.0;
		marker.pose.orientation.z = 0.0;
		marker.pose.orientation.w = 1.0;
		marker.scale.x = 0.3;
		marker.scale.y = 0.3;
		marker.scale.z = 0.1;
		marker.color.a = 1.0;
		marker.color.r = 0.0;
		marker.color.g = 1.0;
		marker.color.b = 0.0;
		//marker.lifetime = 10;
		//marker.frame_locked = true;

		geometry_msgs::PoseArray poseArray;
		poseArray.header.seq = laserScan->header.seq;
		poseArray.header.stamp = laserScan->header.stamp;
		poseArray.header.frame_id = laserScan->header.frame_id;

		geometry_msgs::Pose pose;
		pose = marker.pose;
		geometry_msgs::Point point;
		point.z = 0.0;

		std_msgs::ColorRGBA color;
		color.a = 1.0;
		color.r = 0.0;
		color.g = 1.0;
		color.b = 0.0;

		for(uint i=0;i<detections.size();i++){
			pose.position.x = detections[i].x();
			pose.position.y = detections[i].y();
			poseArray.poses.push_back(pose);

			point.x = detections[i].x();
			point.y = detections[i].y();

			marker.points.push_back(point);
			marker.colors.push_back(color);
		}
		mHypothesesPoseArrayTopic.publish(poseArray);
		mHypothesesMarkerTopic.publish(marker);
	}

private:
	ros::NodeHandle mNodeHandle;
	ros::Publisher mHypothesesPoseArrayTopic;
	ros::Publisher mHypothesesMarkerTopic;
	ros::Subscriber mLaserSub;

	GDIFDetector mGDIFDetector;
	// remove this
	boost::shared_ptr<AdaboostClassifierParams> mAdaBoostParams;

	SegmentationParams mSegmentationParams;
	BoundingBoxParams mBoundingBoxParams;
};



int main(int argc, char **argv)
{
	ros::init(argc, argv, "GDIFMultiObjectDetector");
	GDIFMultiObjectDetectorNode tGDIFMultiObjectDetectorNode;
	tGDIFMultiObjectDetectorNode.init();
	ros::spin();

	return 0;
}