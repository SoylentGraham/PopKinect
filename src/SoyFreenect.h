#pragma once
#include <ofxSoylent.h>
#include <libfreenect/libfreenect.h>
#include "TJob.h"
#include <SoyVideoDevice.h>


class SoyFreenect;
class SoyFreenectDevice;


//	device per-stream which the user really recieves,
//	pretty dumb and main device just pushes data onto it
class SoyFreenectSubDevice : public TVideoDevice
{
public:
	SoyFreenectSubDevice(SoyFreenectDevice& Parent,const TVideoDeviceMeta& Meta,std::stringstream& Error);
	
	void					OnVideo(void *rgb, uint32_t timestamp);
	void					OnDepth(void *depth, uint32_t timestamp);
	
	virtual TVideoDeviceMeta	GetMeta() const override
	{
		return mMeta;
	}
	
	void					OnBufferChanged(const SoyTime& Timecode)
	{
		OnNewFrame( mVideoBuffer, Timecode );
	}
	
public:
	freenect_frame_mode		mVideoMode;
	SoyPixels				mVideoBuffer;
	
	SoyFreenectDevice&		mParent;
	TVideoDeviceMeta		mMeta;
};



//	runtime device
class SoyFreenectDevice
{
public:
	SoyFreenectDevice(SoyFreenect& Parent,const std::string& Serial,std::stringstream& Error);

	std::shared_ptr<SoyFreenectSubDevice>	GetVideoDevice(TVideoQuality::Type Quality,std::stringstream& Error);
	std::shared_ptr<SoyFreenectSubDevice>	GetDepthDevice(TVideoQuality::Type Quality,std::stringstream& Error);
	
	bool					Open(bool Video,bool Depth,std::stringstream& Error);
	void					CloseVideoDevice();
	void					CloseDepthDevice();
	
	void					OnVideo(void *rgb, uint32_t timestamp);
	void					OnDepth(void *depth, uint32_t timestamp);
	
	bool					SetVideoFormat(freenect_resolution Resolution,SoyPixelsFormat::Type Format);
	bool					SetDepthFormat(freenect_resolution Resolution,SoyPixelsFormat::Type Format);
	
public:
	SoyFreenect&			mParent;
	freenect_device*		mDevice;
	std::string				mSerial;

	std::shared_ptr<SoyFreenectSubDevice>	mVideoDevice;
	std::shared_ptr<SoyFreenectSubDevice>	mDepthDevice;
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

	//	get/create device for this serial
	std::shared_ptr<SoyFreenectDevice>	GetDevice(const std::string& Serial,std::stringstream& Error);

	
public:
	Array<std::shared_ptr<SoyFreenectDevice>>	mDevices;
	std::recursive_mutex	mContextLock;
	freenect_context*		mContext;	//	gr: put in it's own free-me class and smart pointer and lock
};


