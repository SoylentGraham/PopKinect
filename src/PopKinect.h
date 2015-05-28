#pragma once
#include <ofxSoylent.h>
#include <TChannel.h>
#include <SoyApp.h>
#include <TFindFeatures.h>
#include <TFIndFeaturesMatch.h>
#include <TJobEventSubscriber.h>
#include <SoyVideoDevice.h>

class TChannel;
class TVideoDevice;


//#define ENABLE_SKELTRACK
#if defined(TARGET_OSX)
#define ENABLE_FREENECT
#endif
//#define ENABLE_NITE

#if defined(ENABLE_FREENECT)
#include "SoyFreenect.h"
#else
class SoyFreenect : public SoyVideoContainer
{
public:
	virtual void							GetDevices(ArrayBridge<TVideoDeviceMeta>& Metas)
	{
	}
	virtual std::shared_ptr<TVideoDevice>	AllocDevice(const TVideoDeviceMeta& Meta, std::stringstream& Error)
	{
		return nullptr;
	}
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
	
	virtual bool	AddChannel(std::shared_ptr<TChannel> Channel) override;
	
	void			OnExit(TJobAndChannel& JobAndChannel);
	void			OnList(TJobAndChannel& JobAndChannel);
	void			OnGetSkeleton(TJobAndChannel& JobAndChannel);
	void			OnGetFrame(TJobAndChannel& JobAndChannel);
	
	void			SubscribeNewFrame(TJobAndChannel& JobAndChannel);
	bool			OnNewFrameCallback(TEventSubscriptionManager& SubscriptionManager,TJobChannelMeta Client,TVideoDevice& Device);

public:
	Soy::Platform::TConsoleApp	mConsoleApp;
	SoyVideoCapture		mVideoCapture;
	TSubscriberManager	mSubcriberManager;
	
	SoySkelTrack	mSkelTrack;
	SoyNite			mNite;
};


