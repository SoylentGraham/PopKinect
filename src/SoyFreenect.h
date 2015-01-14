#pragma once
#include <ofxSoylent.h>
#include <libfreenect/libfreenect.h>
#include "TJob.h"
#include <SoyVideoDevice.h>


class SoyFreenect;




//	runtime device
class SoyFreenectDevice : public TVideoDevice
{
public:
	SoyFreenectDevice(SoyFreenect& Parent,const TVideoDeviceMeta& Meta,std::stringstream& Error);

	bool					Open(std::stringstream& Error);
	void					Close();
	void					OnVideo(void *rgb, uint32_t timestamp);
	void					OnDepth(void *depth, uint32_t timestamp);
	
	virtual TVideoDeviceMeta	GetMeta() const override;
	std::string				GetRealSerial() const;
	
	bool					SetVideoFormat(freenect_resolution Resolution,SoyPixelsFormat::Type Format);
	bool					SetDepthFormat(freenect_resolution Resolution,SoyPixelsFormat::Type Format);
	
public:
	freenect_frame_mode		mVideoMode;
	SoyPixels				mVideoBuffer;
	
	SoyFreenect&			mParent;
	TVideoDeviceMeta		mMeta;
	freenect_device*		mDevice;
};



class SoyFreenect : public SoyVideoContainer, public SoyWorkerThread
{
public:
	SoyFreenect();
	virtual ~SoyFreenect();
	
	virtual void							GetDevices(ArrayBridge<TVideoDeviceMeta>& Metas) override;
	virtual std::shared_ptr<TVideoDevice>	AllocDevice(const TVideoDeviceMeta& Meta,std::stringstream& Error) override;
	
	freenect_context*	GetContext()	{	return mContext;	}

private:
	virtual bool		Iteration();
	bool				CreateContext(std::stringstream& Error);
	void				DestroyContext();

private:
	std::map<int,std::shared_ptr<SoyFreenectDevice>>	mDevices;
	std::recursive_mutex	mContextLock;
	freenect_context*		mContext;	//	gr: put in it's own free-me class and smart pointer and lock
};


