#pragma once
#include <ofxSoylent.h>
#include <SoyApp.h>
#include "TJob.h"
#include "TFrameContainer.h"
#include "TFindFeatures.h"
#include "TFIndFeaturesMatch.h"
#include <TJobEventSubscriber.h>

class TChannel;

#pragma once
#include <ofxSoylent.h>
#include <SoyApp.h>
#include <TJob.h>
#include <TChannel.h>


//#define ENABLE_SKELTRACK
#define ENABLE_FREENECT
//#define ENABLE_NITE

#if defined(ENABLE_FREENECT)
#include "SoyFreenect.h"
#else
class SoyFreenect : public TJobHandler
{
};
#endif

#if defined(ENABLE_SKELTRACK)
#include "SoySkelTrack.h"
#else
class SoySkelTrack
{
};
#endif

#if defined(ENABLE_NITE)
#include "SoyNite.h"
#else
class SoyNite
{
};
#endif

class TPopKinect : public TJobHandler, public TChannelManager
{
public:
	TPopKinect();
	
	virtual void	AddChannel(std::shared_ptr<TChannel> Channel) override;
	
	void			OnExit(TJobAndChannel& JobAndChannel);
	void			OnList(TJobAndChannel& JobAndChannel);
	void			OnGetSkeleton(TJobAndChannel& JobAndChannel);
	void			OnGetDepth(TJobAndChannel& JobAndChannel);
	void			OnGetVideo(TJobAndChannel& JobAndChannel);
	
	void			SubscribeNewDepth(TJobAndChannel& JobAndChannel);
	bool			OnNewDepthCallback(TEventSubscriptionManager& SubscriptionManager,TJobChannelMeta Client,TVideoDevice& Device);

public:
	Soy::Platform::TConsoleApp	mConsoleApp;
	SoyVideoCapture		mVideoCapture;
	TSubscriberManager	mSubcriberManager;

	SoyFreenect		mFreenect;
	
	SoySkelTrack	mSkelTrack;
	SoyNite			mNite;
};


