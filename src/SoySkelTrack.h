#pragma once
#include <ofxSoylent.h>
#include <SoyPixels.h>
#include "SoySkeleton.h"
#include <skeltrack.h>


class SoySkelTrack : public SoySkeletonDetector
{
public:
	SoySkelTrack();
	virtual ~SoySkelTrack();
	
protected:
	virtual void			ProcessDepth(std::shared_ptr<SoyPixels> Pixels) override;
	
public:
	SkeltrackSkeleton*		mSkelTrack;
};

