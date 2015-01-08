#pragma once
#include <ofxSoylent.h>
#include "SoySkeleton.h"

#include <XnOpenNI.h>
#include <XnCodecIDs.h>
#include <XnCppWrapper.h>
#include <XnPropNames.h>


class SoyNite : public SoySkeletonDetector
{
public:
	SoyNite();
	virtual ~SoyNite();
	
protected:
	virtual void	threadedFunction();
	virtual void		ProcessDepth(std::shared_ptr<SoyPixels> Pixels) override;

public:
	bool				mDepthFromDevice;
	SoyEvent<SoyPixels&>	mOnDeviceDepth;
	
	XnChar				mPoseName[100];
	bool				mNeedPose;
	xn::Context			mContext;
	xn::DepthGenerator	mDepthGenerator;
	xn::MockDepthGenerator	mMockDepthGenerator;
	xn::UserGenerator	mUserGenerator;
};


