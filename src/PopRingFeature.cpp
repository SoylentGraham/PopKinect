#include "PopRingFeature.h"


TPopRingFeatureParams::TPopRingFeatureParams() :
	mContrastTolerance	( 0.5f )
{
	
}

TPopRingFeatureParams::TPopRingFeatureParams(const TJobParams& params) :
	TPopRingFeatureParams	()
{
}


TPopRingFeature TPopRingFeature::GetFeature(const SoyPixelsImpl& Pixels,int x,int y,const TPopRingFeatureParams& Params)
{
	TPopRingFeature Feature;
	std::bitset<32> Bits( std::string("10101010101010101010101010101010" ));
	auto BitsLong = Bits.to_ulong();
	Feature.mBrighters = BitsLong;
	return Feature;
}

