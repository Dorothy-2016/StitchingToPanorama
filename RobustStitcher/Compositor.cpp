/**
@brief Compositor.cpp
C++ source file for compositing images into panorams
@author Shane Yuan
@date Feb 9, 2018
*/

#include <memory>

#include <opencv2/stitching/warpers.hpp>

#include "Compositor.h"

#define DEBUG_COMPOSITOR

calib::Compositor::Compositor() {}
calib::Compositor::~Compositor() {}

/**
@brief init function set image and cameras
@param std::vector<cv::Mat> imgs: input images
@param std::vector<calib::CameraParams> cameras: input camera params
@return int
*/
int calib::Compositor::init(std::vector<cv::Mat> imgs, 
	std::vector<calib::CameraParams> cameras) {
	this->imgs = imgs;
	this->cameras = cameras;
	return 0;
}

/**
@brief composite panorama
@return int
*/
int calib::Compositor::composite() {
	cv::Ptr<cv::detail::SphericalWarper> w = cv::makePtr<cv::detail::SphericalWarper>(false);
	std::shared_ptr<cv::detail::Blender> blender_ = std::make_shared<cv::detail::MultiBandBlender>(false);
	corners.resize(imgs.size());
	sizes.resize(imgs.size());
	roiRect.resize(imgs.size());
	imgWarped.resize(imgs.size());
	imgMask.resize(imgs.size());
	warpMaps.resize(imgs.size());
	backwarpMaps.resize(imgs.size());
	w->setScale(cameras[0].focal);
	for (int i = 0; i < imgs.size(); i++) {
		// calculate warping filed
		cv::Mat K, R;
		cameras[i].K().convertTo(K, CV_32F);
		cameras[i].R.convertTo(R, CV_32F);
		cv::Mat initmask(imgs[i].rows, imgs[i].cols, CV_8U);
		initmask.setTo(cv::Scalar::all(255));
		corners[i] = w->warp(imgs[i], K, R, cv::INTER_LINEAR, cv::BORDER_CONSTANT, imgWarped[i]);
		w->warp(initmask, K, R, cv::INTER_NEAREST, cv::BORDER_CONSTANT, imgMask[i]);
		sizes[i] = imgMask[i].size();
#ifdef DEBUG_COMPOSITOR
		std::cout << corners[i] << std::endl;
		cv::imwrite(cv::format("%d.jpg", i), imgWarped[i]);
		cv::imwrite(cv::format("%d_mask.jpg", i), imgMask[i]);
#endif
	}
	cv::Mat result, result_mask;
	blender_->prepare(corners, sizes);
	for (int i = 0; i < imgs.size(); i++) {
		// feed to blender
		blender_->feed(imgWarped[i], imgMask[i], corners[i]);
	}
	blender_->blend(result, result_mask);
	result.convertTo(result, CV_8U);
#ifdef DEBUG_COMPOSITOR
	cv::imwrite("result.jpg", result);
#endif

	return 0;
}

int calib::Compositor::single_composite(calib::CameraParams& current_camera, std::vector<calib::CameraParams>& cameras, 
						cv::Mat& src, std::vector<cv::Mat>& imgs) {
	cv::Ptr<cv::detail::SphericalWarper> w = cv::makePtr<cv::detail::SphericalWarper>(false);
	std::shared_ptr<cv::detail::Blender> blender_ = std::make_shared<cv::detail::MultiBandBlender>(false);
	std::vector<cv::Point> corners(73);
	std::vector<cv::Size> sizes(73);
	std::vector<cv::Rect> roiRect(73);    //隐患！！！！！！！！！！
	std::vector<cv::Mat> imgWarped(73);
	std::vector<cv::Mat> imgMask(73);
	w->setScale(16000);
	for (int i = 0; i < imgs.size(); i++) {
		// calculate warping filed
		cv::Mat K, R;
		cameras[i].K().convertTo(K, CV_32F);
		cameras[i].R.convertTo(R, CV_32F);
		cv::Mat initmask(imgs[i].rows, imgs[i].cols, CV_8U);
		initmask.setTo(cv::Scalar::all(255));
		corners[i] = w->warp(imgs[i], K, R, cv::INTER_LINEAR, cv::BORDER_CONSTANT, imgWarped[i]);
		w->warp(initmask, K, R, cv::INTER_NEAREST, cv::BORDER_CONSTANT, imgMask[i]);
		sizes[i] = imgMask[i].size();
#ifdef DEBUG_COMPOSITOR
		//std::cout << corners[i] << std::endl;
		//cv::imwrite(cv::format("%d.jpg", i), imgWarped[i]);
		//cv::imwrite(cv::format("%d_mask.jpg", i), imgMask[i]);
#endif
	}

	cv::Mat K, R;
	current_camera.K().convertTo(K, CV_32F);
	current_camera.R.convertTo(R, CV_32F);
	cv::Mat initmask(src.rows, src.cols, CV_8U);
	initmask.setTo(cv::Scalar::all(255));
	corners[imgs.size()] = w->warp(src, K, R, cv::INTER_LINEAR, cv::BORDER_CONSTANT, imgWarped[imgs.size()]);
	w->warp(initmask, K, R, cv::INTER_NEAREST, cv::BORDER_CONSTANT, imgMask[imgs.size()]);
	sizes[imgs.size()] = imgMask[imgs.size()].size();

	cv::Rect roi(corners[imgs.size()], sizes[imgs.size()]);
	//cv::imwrite("E:/code/panoramastitcherSln/warped.jpg", imgWarped[imgs.size()]);
	cv::Mat target = cv::imread("result.jpg");
	CV_Assert(sizes.size() == corners.size());
	cv::Point tl(std::numeric_limits<int>::max(), std::numeric_limits<int>::max());
	cv::Point br(std::numeric_limits<int>::min(), std::numeric_limits<int>::min());
	for (size_t i = 0; i < corners.size(); ++i)
	{
		//////
		//corners[i].x *= -1;
		///////
		tl.x = std::min(tl.x, corners[i].x);
		tl.y = std::min(tl.y, corners[i].y);
		br.x = std::max(br.x, corners[i].x + sizes[i].width);
		br.y = std::max(br.y, corners[i].y + sizes[i].height);
	}
	cv::Rect pano(tl, br);     //这里用corners而不用输入的全景图的原因是想得到全景图的绝对坐标

	int dx = roi.x - pano.x;
	int dy = roi.y - pano.y;    //表示输入图像在全景图中的相对位置
	cv::Rect roi1(cv::Point(dx, dy), sizes[imgs.size()]);
	imgWarped[imgs.size()].copyTo(target(roi1), imgMask[imgs.size()]);





	//cv::Mat result, result_mask;
	//blender_->prepare(corners, sizes);
	//for (int i = 0; i < imgs.size() + 1; i++) {
	//	// feed to blender
	//	blender_->feed(imgWarped[i], imgMask[i], corners[i]);
	//}
	//blender_->blend(result, result_mask);
	//result.convertTo(result, CV_8U);
#ifdef DEBUG_COMPOSITOR
	cv::imwrite("E:/code/from-zero/Sln/result11duibi.jpg", target);
#endif

	return 0;
}