#include "SoyFreenect.h"
#include <SoyDebug.h>
#include <SoyTypes.h>
#include "TJob.h"
#include "TParameters.h"
#include "TChannel.h"
#include <libusb.h>


const std::string VideoPrefix = "Video";
const std::string DepthPrefix = "Depth";

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
	
	DestroyContext();
}

void SoyFreenect::GetDevices(ArrayBridge<TVideoDeviceMeta>& Metas)
{
	//	get the sub device serials
	std::lock_guard<std::recursive_mutex> Lock(mContextLock);

	struct freenect_device_attributes* DeviceList = nullptr;
	//	returns <0 on error, or number of devices.
	freenect_list_device_attributes( mContext, &DeviceList );
	if ( true )
	{
		auto Device = DeviceList;
		while ( Device )
		{
			//	seperate devices for seperate content
			TVideoDeviceMeta VideoDeviceMeta;
			VideoDeviceMeta.mSerial = VideoPrefix + Device->camera_serial;
			VideoDeviceMeta.mVideo = true;
			
			TVideoDeviceMeta DepthDeviceMeta;
			DepthDeviceMeta.mSerial = DepthPrefix + Device->camera_serial;
			DepthDeviceMeta.mDepth = true;
			
			Metas.PushBack( VideoDeviceMeta );
			Metas.PushBack( DepthDeviceMeta );

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

void SoyFreenect::DestroyContext()
{
	std::lock_guard<std::recursive_mutex> Lock(mContextLock);
	if ( mContext )
	{
		freenect_shutdown( mContext );
		mContext = nullptr;
	}
}

bool SoyFreenect::Iteration()
{
	std::lock_guard<std::recursive_mutex> Lock(mContextLock);

	//	gr: use this for the thread block
	int TimeoutMs = 10;
	int TimeoutSecs = 0;
	int TimeoutMicroSecs = TimeoutMs*1000;
	timeval Timeout = {TimeoutSecs,TimeoutMicroSecs};
	libusb_error Result = static_cast<libusb_error>( freenect_process_events_timeout( mContext, &Timeout ) );
	if ( Result != LIBUSB_SUCCESS )
	{
		std::string LibUsbError = libusb_strerror(Result);
		std::Debug << "Freenect_events error: " << LibUsbError << "(" << static_cast<int>(Result) << ")";

		DestroyContext();
		std::stringstream CreateError;
		CreateContext( CreateError );
	}

	return true;
}

std::shared_ptr<TVideoDevice> SoyFreenect::AllocDevice(const TVideoDeviceMeta& Meta,std::stringstream& Error)
{
	std::shared_ptr<TVideoDevice> Device( new SoyFreenectDevice( *this, Meta, Error ) );
	return Device;
}


SoyFreenectDevice::SoyFreenectDevice(SoyFreenect& Parent,const TVideoDeviceMeta& Meta,std::stringstream& Error) :
	TVideoDevice	( Meta, Error ),
	mParent			( Parent ),
	mMeta			( Meta )
{
	Open(Error);
}

TVideoDeviceMeta SoyFreenectDevice::GetMeta() const
{
	return mMeta;
}

std::string SoyFreenectDevice::GetRealSerial() const
{
	if ( Soy::StringBeginsWith( mMeta.mSerial, VideoPrefix, true ) )
		return mMeta.mSerial.substr( VideoPrefix.length() );
	if ( Soy::StringBeginsWith( mMeta.mSerial, DepthPrefix, true ) )
		return mMeta.mSerial.substr( DepthPrefix.length() );

	Soy::Assert( false, std::stringstream() << "Cannot determine real serial from " << mMeta.mSerial );
	return std::string();
}

bool SoyFreenectDevice::Open(std::stringstream& Error)
{
	//	open device
	auto Result = freenect_open_device_by_camera_serial( mParent.GetContext(), &mDevice, GetRealSerial().c_str() );
	if ( Result < 0 )
	{
		Error << "Failed to open device " << Result;
		return false;
	}
	
	//	setup
	//	gr: different LED's for different modes please :)
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
	mVideoMode = freenect_find_depth_mode( Resolution, FreenectFormat );
	if ( !mVideoMode.is_valid )
		return false;
	
	/*
	auto SoyFormat = SoyPixelsFormat::GetFormatFromChannelCount( mVideoMode.data_bits_per_pixel/8 );
	if ( SoyFormat == SoyPixelsFormat::Invalid )
	 return false;

*/
	//	setup format
	if ( !mVideoBuffer.Init( mVideoMode.width, mVideoMode.height, Format ) )
		return false;
	
	if ( freenect_set_depth_mode( mDevice, mVideoMode ) < 0 )
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
	auto& Pixels = mVideoBuffer.GetPixelsArray();
	auto* Depth16 = static_cast<uint16*>( depth );
	//	copy to target
	int Bytes = std::min( mVideoMode.bytes, Pixels.GetDataSize() );
	memcpy( Pixels.GetArray(), Depth16, Bytes );
	
	static uint16 Min = ~0;
	static uint16 Max = 0;

	uint16 MaxDepth = SoyPixelsFormat::GetMaxValue( mVideoBuffer.GetFormat() );
	uint16 InvalidDepth = SoyPixelsFormat::GetInvalidValue( mVideoBuffer.GetFormat() );

	
	for ( int i=0;	i<mVideoMode.bytes/2;	i++ )
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
	OnNewFrame( mVideoBuffer, Timecode );
}
