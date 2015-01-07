#pragma once

#include <ofxSoylent.h>
#include <SoyData.h>

class TJobParams;
class SoyPixelsImpl;




class TPopRingFeatureParams
{
public:
	TPopRingFeatureParams();
	TPopRingFeatureParams(const	TJobParams& params);
	
	float		mContrastTolerance;
};




class TPopRingFeature
{
public:
	TPopRingFeature() :
		mBrighters	( 0 )
	{
	}
	static TPopRingFeature	GetFeature(const SoyPixelsImpl& Pixels,int x,int y,const TPopRingFeatureParams& Params);
	
public:
	uint32		mBrighters;	//	1 brighter, 0 darker
};

template <> template<>
inline bool SoyData_Impl<std::string>::Encode(const SoyData_Impl<TPopRingFeature>& Data)
{
	auto& String = this->mValue;
	auto& Feature = Data.mValue;
	String = (std::stringstream() << Feature.mBrighters).str();
	return true;
}
