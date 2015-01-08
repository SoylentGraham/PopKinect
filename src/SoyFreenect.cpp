#include "SoyFreenect.h"
#include <SoyDebug.h>
#include <SoyTypes.h>
#include "TJob.h"
#include "TParameters.h"
#include "TChannel.h"



freenect_video_format GetFreenectVideoFormat(SoyPixelsFormat::Type Format)
{
	switch ( Format )
	{
		case SoyPixelsFormat::RGB:	return FREENECT_VIDEO_RGB;
		default:
			return FREENECT_VIDEO_DUMMY;
	}
}

freenect_depth_format GetFreenectDepthFormat(SoyPixelsFormat::Type Format)
{
	switch ( Format )
	{
		case SoyPixelsFormat::FreenectDepth11bit:	return FREENECT_DEPTH_11BIT;
		case SoyPixelsFormat::FreenectDepth10bit:	return FREENECT_DEPTH_10BIT;
		case SoyPixelsFormat::FreenectDepthmm:		return FREENECT_DEPTH_REGISTERED;
		default:
			return FREENECT_DEPTH_DUMMY;
	}
}

SoyPixelsFormat::Type GetPixelFormat(freenect_depth_format Format)
{
	switch ( Format )
	{
		case FREENECT_DEPTH_11BIT:	return SoyPixelsFormat::FreenectDepth11bit;
		case FREENECT_DEPTH_10BIT:	return SoyPixelsFormat::FreenectDepth10bit;
		case FREENECT_DEPTH_REGISTERED:	return SoyPixelsFormat::FreenectDepthmm;
		default:
			return SoyPixelsFormat::Invalid;
	}
}


SoyFreenect::SoyFreenect() :
	SoyWorkerThread	( "SoyFreenect", SoyWorkerWaitMode::NoWait )
{
	std::stringstream Error;
	if ( !CreateContext(Error) )
		return;
	Start();
}

SoyFreenect::~SoyFreenect()
{
	WaitToFinish();
	
	std::lock_guard<std::recursive_mutex> Lock(mContextLock);
	if ( mContext )
	{
		freenect_shutdown( mContext );
		mContext = nullptr;
	}
}

TVideoDeviceMeta GetMeta(struct freenect_device_attributes& Device)
{
	TVideoDeviceMeta Meta;
	Meta.mSerial = Device.camera_serial;
	Meta.mVideo = true;
	Meta.mDepth = true;

	return Meta;
}

void SoyFreenect::GetDevices(ArrayBridge<TVideoDeviceMeta>& Metas)
{
	//	get the sub device serials
	std::lock_guard<std::recursive_mutex> Lock(mContextLock);

	struct freenect_device_attributes* DeviceList = nullptr;
	auto ListError = freenect_list_device_attributes( mContext, &DeviceList );
	if ( true )
	{
		auto Device = DeviceList;
		while ( Device )
		{
			TVideoDeviceMeta Meta = GetMeta( *Device );
			Metas.PushBack( Meta );
						Device = Device->next;
		}
		freenect_free_device_attributes( DeviceList );
	}
}


bool SoyFreenect::CreateContext(std::stringstream& Error)
{
	std::lock_guard<std::recursive_mutex> Lock(mContextLock);

	//	init context
	freenect_usb_context* UsbContext = nullptr;
	auto Result = freenect_init( &mContext, UsbContext );
	if ( Result < 0 )
	{
		Error << "freenect_init result: " << Result;
		Soy::Assert( mContext == nullptr, "Expected null context" );
		return false;
	}

	freenect_set_log_level( mContext, FREENECT_LOG_WARNING);
	freenect_device_flags flags = (freenect_device_flags)( FREENECT_DEVICE_CAMERA );
	freenect_select_subdevices( mContext, flags );

	return true;
}

bool SoyFreenect::Iteration()
{
	std::lock_guard<std::recursive_mutex> Lock(mContextLock);

	//	gr: use this for the thread block
	int TimeoutSecs = 1;
	int TimeoutMicroSecs = 0;
	timeval Timeout = {TimeoutSecs,TimeoutMicroSecs};
	auto Result = freenect_process_events_timeout( mContext, &Timeout );
	if ( Result < 0 )
	{
		std::Debug << "Freenect_events error: " << Result;
	}

	return true;
}

std::shared_ptr<TVideoDevice> SoyFreenect::AllocDevice(const std::string& Serial,std::stringstream& Error)
{
	std::shared_ptr<TVideoDevice> Device( new SoyFreenectDevice( *this, Serial, Error ) );
	return Device;
}


SoyFreenectDevice::SoyFreenectDevice(SoyFreenect& Parent,const std::string& Serial,std::stringstream& Error) :
	TVideoDevice	( Serial, Error ),
	mParent			( Parent ),
	mSerial			( Serial )
{
	Open(Error);
}

TVideoDeviceMeta SoyFreenectDevice::GetMeta() const
{
	//	gr: maybe need to cache meta or something
	
	Array<TVideoDeviceMeta> Metas;
	auto MetasBridge = GetArrayBridge(Metas);
	mParent.GetDevices( MetasBridge );
	auto pMeta = Metas.Find( mSerial );

	if ( !Soy::Assert( pMeta, "Meta expected" ) )
		return TVideoDeviceMeta();
	
	return *pMeta;
}

bool SoyFreenectDevice::Open(std::stringstream& Error)
{
	//	open device
	auto Result = freenect_open_device_by_camera_serial( mParent.GetContext(), &mDevice, mSerial.c_str() );
	if ( Result < 0 )
	{
		Error << "Failed to open device " << Result;
		return false;
	}
	
	//	setup
	freenect_set_led( mDevice, LED_YELLOW );
	freenect_set_user( mDevice, this );
	
	auto VideoCallback = [](freenect_device *dev, void *rgb, uint32_t timestamp)
	{
		SoyFreenectDevice* This = reinterpret_cast<SoyFreenectDevice*>( freenect_get_user(dev) );
		This->OnVideo( rgb, timestamp );
	};
	freenect_set_video_callback( mDevice, VideoCallback );
	
	auto DepthCallback = [](freenect_device *dev, void *rgb, uint32_t timestamp)
	{
		SoyFreenectDevice* This = reinterpret_cast<SoyFreenectDevice*>( freenect_get_user(dev) );
		This->OnDepth( rgb, timestamp );
	};
	freenect_set_depth_callback( mDevice, DepthCallback );
	
	/*
	if ( !SetVideoFormat( FREENECT_RESOLUTION_MEDIUM, SoyPixelsFormat::RGB ) )
	{
		Close();
		return false;
	}
	*/
	if ( !SetDepthFormat( FREENECT_RESOLUTION_MEDIUM, SoyPixelsFormat::FreenectDepth11bit ) )
	{
		Error << "Failed to set depth format";
		Close();
		return false;
	}
	
	return true;
}

bool SoyFreenectDevice::SetVideoFormat(freenect_resolution Resolution,SoyPixelsFormat::Type Format)
{
	//	stop current mode
	freenect_stop_video( mDevice );

	//	get freenect format
	auto FreenectFormat = GetFreenectVideoFormat( Format );
	
	//	setup format
	mVideoMode = freenect_find_video_mode( Resolution, FreenectFormat );
	if ( !mVideoMode.is_valid )
		return false;
	
	if ( !Soy::Assert( mVideoMode.padding_bits_per_pixel==0, "don't support aligned pixel formats" ) )
		return false;
	
	mVideoBuffer.Init( mVideoMode.width, mVideoMode.height, Format );
	if ( !Soy::Assert( mVideoBuffer.GetPixelsArray().GetDataSize() == mVideoMode.bytes, "Allocated wrong amount of pixel data" ) )
		return false;
	
	if ( freenect_set_video_mode( mDevice, mVideoMode ) < 0 )
		return false;
	//freenect_set_video_buffer( mDevice, rgb_back);
	
	if ( freenect_start_video( mDevice ) < 0 )
		return false;
	
	return true;
}


bool SoyFreenectDevice::SetDepthFormat(freenect_resolution Resolution,SoyPixelsFormat::Type Format)
{
	//	stop current mode
	freenect_stop_depth( mDevice );
	
	//	get freenect format
	auto FreenectFormat = GetFreenectDepthFormat( Format );
	
	//	setup format
	mDepthMode = freenect_find_depth_mode( Resolution, FreenectFormat );
	if ( !mDepthMode.is_valid )
		return false;
	
	/*
	auto SoyFormat = SoyPixelsFormat::GetFormatFromChannelCount( mVideoMode.data_bits_per_pixel/8 );
	if ( SoyFormat == SoyPixelsFormat::Invalid )
	 return false;

*/
	//	setup format
	if ( !mDepthBuffer.Init( mDepthMode.width, mDepthMode.height, Format ) )
		return false;
	
	if ( freenect_set_depth_mode( mDevice, mDepthMode ) < 0 )
		return false;
	
	SoyThread::Sleep(100);
	if ( freenect_start_depth( mDevice ) < 0 )
		return false;
	
	return true;

}


void SoyFreenectDevice::Close()
{
	freenect_set_led( mDevice, LED_RED );
}

void SoyFreenectDevice::OnVideo(void *rgb, uint32_t timestamp)
{
	if ( !Soy::Assert( rgb, "rgb data expected" ) )
		return;
	
	//std::Debug << "On video " << timestamp << std::endl;

	auto& Pixels = mVideoBuffer.GetPixelsArray();
	int Bytes = std::min( mVideoMode.bytes, Pixels.GetDataSize() );
	memcpy( Pixels.GetArray(), rgb, Bytes );

	//	notify change
	SoyTime Timecode( static_cast<uint64>(timestamp) );
	OnNewFrame( mVideoBuffer, Timecode );
}





void SoyFreenectDevice::OnDepth(void *depth, uint32_t timestamp)
{
	if ( !Soy::Assert( depth, "depth data expected" ) )
		return;
	
	//	gr: change all this to a SoyPixelsImpl and make a copy in the event!
	auto& Pixels = mDepthBuffer.GetPixelsArray();
	auto* Depth16 = static_cast<uint16*>( depth );
	//	copy to target
	int Bytes = std::min( mDepthMode.bytes, Pixels.GetDataSize() );
	memcpy( Pixels.GetArray(), Depth16, Bytes );
	
	static uint16 Min = ~0;
	static uint16 Max = 0;

	uint16 MaxDepth = SoyPixelsFormat::GetMaxValue( mDepthBuffer.GetFormat() );
	uint16 InvalidDepth = SoyPixelsFormat::GetInvalidValue( mDepthBuffer.GetFormat() );

	
	for ( int i=0;	i<mDepthMode.bytes/2;	i++ )
	{
		auto Depth = Depth16[i];
		if ( Depth == 0 )
			continue;
		if ( Depth == MaxDepth )
			continue;
		if ( Depth == InvalidDepth )
			continue;
		Min = std::min( Min, Depth );
		Max = std::max( Max, Depth );
	}

	/*
	SoyPixelsFormat::Type Format = GetPixelFormat( mDepthMode.depth_format );
	int InvalidValue = SoyPixelsFormat::GetInvalidValue( Format );
	
	for ( int i=0;	i<mDepthMode.bytes/2;	i++ )
	{
		//	gr: instead of threshold
		static int Threshold = 800;
		auto& Depth = Depth16[i];
		if ( Depth > Threshold )
		{
			Depth = InvalidValue;
		}
		else
		{
			//	set player id
			Depth |= 1<<13;
		}
	}
	*/

	static int Counter = 0;
	if ( (Counter++ % 60) == 0 )
	{
		//	11bit: min: 246, Max: 1120
		std::Debug << "freenect min: " << Min << ", Max: " << Max << std::endl;
	}
	
	//	notify change
	SoyTime Timecode( static_cast<uint64>(timestamp) );
	OnNewFrame( mDepthBuffer, Timecode );
}
