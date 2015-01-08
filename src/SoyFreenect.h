#pragma once
#include <ofxSoylent.h>
#include <libfreenect/libfreenect.h>
#include "TJob.h"


class SoyFreenect;
class SoyFreenectContextLoop;


class SoyFreenectDeviceMeta
{
public:
	SoyFreenectDeviceMeta()
	{
	}
	
public:
	std::string		mSerial;
};

//	runtime device
class SoyFreenectDevice
{
public:
	SoyFreenectDevice(SoyFreenectContextLoop& Parent,int Serial);

	bool					Open();
	void					Close();
	void					OnVideo(void *rgb, uint32_t timestamp);
	void					OnDepth(void *depth, uint32_t timestamp);
	
	SoyFreenectDeviceMeta	GetMeta() const;
	
	bool					SetVideoFormat(freenect_resolution Resolution,SoyPixelsFormat::Type Format);
	bool					SetDepthFormat(freenect_resolution Resolution,SoyPixelsFormat::Type Format);
	
public:
	freenect_frame_mode		mVideoMode;
	SoyPixels				mVideoBuffer;
	
	freenect_frame_mode		mDepthMode;
	SoyPixels				mDepthBuffer;
	
	SoyFreenectContextLoop&	mParent;
	int						mSerial;
	freenect_device*		mDevice;
};


class TTempJobAndChannel
{
public:
	TTempJobAndChannel() :
		mChannel	( nullptr )
	{
	}
	TTempJobAndChannel(TJobAndChannel& JobAndChannel);
	
	bool		IsValid() const		{	return mChannel && mJob.IsValid();	}
	
public:
	TJob		mJob;
	TChannel*	mChannel;
};

class SoyFreenectContextLoop : public SoyThread
{
public:
	SoyFreenectContextLoop(SoyFreenect& Parent);
	virtual ~SoyFreenectContextLoop();
	
	bool			QueueJob(TJobAndChannel& JobAndChannel);
	
	void			OnOpenDevice(TJobAndChannel& JobAndChannel);
	void			OnCloseDevice(TJobAndChannel& JobAndChannel);
	void			OnShutdown(TJobAndChannel& JobAndChannel);
	void			OnGetVideo(TJobAndChannel& JobAndChannel);
	void			OnGetDepth(TJobAndChannel& JobAndChannel);
	
	bool			OpenDevice(int DeviceSerial);
	void			CloseDevice(int DeviceSerial);
	void			Shutdown();
	
	freenect_context*	GetContext()	{	return mContext;	}
	
private:
	virtual void	threadedFunction();

private:
	std::map<int,std::shared_ptr<SoyFreenectDevice>>	mDevices;
	TLockQueue<TTempJobAndChannel>	mQueue;
	freenect_context*	mContext;	//	gr: put in it's own free-me class and smart pointer and lock

public:
	SoyFreenect&		mParent;
};


class SoyFreenect : public TJobHandler
{
public:
	SoyFreenect();
	~SoyFreenect();
	
	void				OnJobInit(TJobAndChannel& JobAndChannel);
	void				OnJobGetDevices(TJobAndChannel& JobAndChannel);
	void				OnJob(TJobAndChannel& JobAndChannel);
	
public:
	SoyEvent<const SoyPixels>	mOnDepthFrame;
	SoyEvent<const SoyPixels>	mOnVideoFrame;
	
	std::shared_ptr<SoyFreenectContextLoop>	mThread;
};

