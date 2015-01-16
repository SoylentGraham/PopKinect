#include "SoyFreenect.h"
#include <SoyDebug.h>
#include <SoyTypes.h>
#include "TJob.h"
#include "TParameters.h"
#include "TChannel.h"
#include <libusb.h>

//#define ENABLE_CONTEXT_LOCK

const std::string VideoPrefix = "Video";
const std::string DepthPrefix = "Depth";

TVideoDeviceMeta SplitSerial(const std::string& Serial)
{
	//	get real serial
	TVideoDeviceMeta Meta;
	
	if ( Soy::StringBeginsWith( Serial, VideoPrefix, true ) )
	{
		Meta.mVideo = true;
		Meta.mSerial = Serial.substr( VideoPrefix.length() );
		return Meta;
	}
	
	
	if ( Soy::StringBeginsWith( Serial, DepthPrefix, true ) )
	{
		Meta.mDepth = true;
		Meta.mSerial = Serial.substr( DepthPrefix.length() );
		return Meta;
	}

	return Meta;
}


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
	SoyWorkerThread	( "SoyFreenect", SoyWorkerWaitMode::NoWait ),
	mContext		( nullptr )
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
#if defined(ENABLE_CONTEXT_LOCK)
	std::lock_guard<std::recursive_mutex> Lock(mContextLock);
#endif

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
#if defined(ENABLE_CONTEXT_LOCK)
	std::lock_guard<std::recursive_mutex> Lock(mContextLock);
#endif
	//	already created
	if ( mContext )
		return true;

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
#if defined(ENABLE_CONTEXT_LOCK)
	std::lock_guard<std::recursive_mutex> Lock(mContextLock);
#endif
	if ( mContext )
	{
		freenect_shutdown( mContext );
		mContext = nullptr;
	}
}

bool SoyFreenect::Iteration()
{
	if ( !mContext )
		return true;
	
#if defined(ENABLE_CONTEXT_LOCK)
	std::lock_guard<std::recursive_mutex> Lock(mContextLock);
#endif
	
	/*	gr: creating out of main thread is causing problems??
	//	recreate context if required
	std::stringstream Error;
	if ( !CreateContext(Error) )
	{
		std::Debug << "Re-creating context failed: " << Error.str() << std::endl;
		return true;
	}
*/
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
		std::this_thread::sleep_for( std::chrono::milliseconds(2000) );
	}

	return true;
}

std::shared_ptr<SoyFreenectDevice> SoyFreenect::GetDevice(const std::string& Serial,std::stringstream& Error)
{
	//	find existing sub device
	for ( int d=0;	d<mDevices.GetSize();	d++ )
	{
		auto Device = mDevices[d];
		if ( Device->mSerial == Serial )
			return Device;
	}
	
	//	doesn't exist, add
	std::shared_ptr<SoyFreenectDevice> Device( new SoyFreenectDevice( *this, Serial, Error ) );
	mDevices.PushBack( Device );
	return Device;
}


std::shared_ptr<TVideoDevice> SoyFreenect::AllocDevice(const TVideoDeviceMeta& Meta,std::stringstream& Error)
{
	//	get the real serial from the one desired
	TVideoDeviceMeta RealMeta = SplitSerial( Meta.mSerial );
	if ( !Soy::Assert( RealMeta.IsValid(), "Failed to split meta from serial" ) )
		return nullptr;

	//	get parent device for this serial first...
	auto Device = GetDevice( RealMeta.mSerial, Error );
	if ( !Device )
		return nullptr;
	
	//	now get the sub device
	if ( Meta.mVideo )
	{
		return Device->GetVideoDevice( TVideoQuality::High, Error );
	}
	else if ( Meta.mDepth )
	{
		return Device->GetDepthDevice( TVideoQuality::High, Error );
	}

	
	Error << "No video or depth supplied in meta, don't know what sub device to create";
	return nullptr;
}


SoyFreenectDevice::SoyFreenectDevice(SoyFreenect& Parent,const std::string& Serial,std::stringstream& Error) :
	mParent			( Parent ),
	mSerial			( Serial )
{
	Open( false, false, Error );
}


std::shared_ptr<SoyFreenectSubDevice> SoyFreenectDevice::GetVideoDevice(TVideoQuality::Type Quality,std::stringstream& Error)
{
	Open( true, mDepthDevice!=nullptr, Error );
	return mVideoDevice;
}

std::shared_ptr<SoyFreenectSubDevice> SoyFreenectDevice::GetDepthDevice(TVideoQuality::Type Quality,std::stringstream& Error)
{
	Open( mVideoDevice!=nullptr, true, Error );
	return mDepthDevice;
}

bool SoyFreenectDevice::Open(bool Video,bool Depth,std::stringstream& Error)
{
	//	gr: adding this context lock stops freenect_open_device_by_camera_serial from finding a device...
	//std::lock_guard<std::recursive_mutex> Lock(mParent.mContextLock);

	//	open device
	if ( !mDevice )
	{
		auto Result = freenect_open_device_by_camera_serial( mParent.GetContext(), &mDevice, mSerial.c_str() );
		if ( Result < 0 )
		{
			Error << "Failed to open device " << Result;
			return false;
		}

		auto OldUserData = freenect_get_user(mDevice);
		//	set user info - is this shared for the same device? or is mDevice isntanced?
		std::Debug << "Opened freenect device (" << mDevice << ") from serial " << mSerial << "; old user data: " << OldUserData << std::endl;
		freenect_set_user( mDevice, this );
	}
	
	//	gr: where do we get this config from?
	auto Resolution = FREENECT_RESOLUTION_MEDIUM;
	
	//	setup video device
	if ( Video && !mVideoDevice )
	{
		auto PixelFormat = SoyPixelsFormat::RGB;
		if ( !SetVideoFormat( Resolution, SoyPixelsFormat::RGB ) )
		{
			Error << "Failed to set video format " << PixelFormat;
			CloseVideoDevice();
			return false;
		}
	}
	else if ( !Video && mVideoDevice )
	{
		CloseVideoDevice();
	}
	
	if ( Depth && !mDepthDevice )
	{
		auto DepthCallback = [](freenect_device *dev, void *rgb, uint32_t timestamp)
		{
			std::Debug << "depth callback" << std::endl;
			SoyFreenectDevice* This = reinterpret_cast<SoyFreenectDevice*>( freenect_get_user(dev) );
			This->OnDepth( rgb, timestamp );
		};
		freenect_set_depth_callback( mDevice, DepthCallback );

		auto PixelFormat = SoyPixelsFormat::FreenectDepth11bit;
		if ( !SetDepthFormat( Resolution, PixelFormat ) )
		{
			Error << "Failed to set depth format";
			CloseDepthDevice();
			return false;
		}
	}
	else if ( !Depth && mDepthDevice )
	{
		CloseDepthDevice();
	}
	
	//	set LED based on what's enabled
	freenect_led_options LedColour = LED_BLINK_RED_YELLOW;
	if ( mVideoDevice && mDepthDevice )
		LedColour = LED_GREEN;
	else if ( mVideoDevice )
		LedColour = LED_YELLOW;
	else if ( mDepthDevice )
		LedColour = LED_RED;
	freenect_set_led( mDevice, LedColour );
	
	return true;
}

bool SoyFreenectDevice::SetVideoFormat(freenect_resolution Resolution,SoyPixelsFormat::Type Format)
{
	//	create sub device
	TVideoDeviceMeta Meta;
	Meta.mSerial = VideoPrefix + mSerial;
	Meta.mVideo = true;
	std::stringstream Error;
	std::shared_ptr<SoyFreenectSubDevice> pDevice( new SoyFreenectSubDevice( *this, Meta, Error ) );

	auto& Device = *pDevice;
	auto& VideoMode = Device.mVideoMode;
	auto& VideoBuffer = Device.mVideoBuffer;
	
	auto VideoCallback = [](freenect_device *dev, void *rgb, uint32_t timestamp)
	{
		std::Debug << "video callback" << std::endl;
		SoyFreenectDevice* This = reinterpret_cast<SoyFreenectDevice*>( freenect_get_user(dev) );
		This->OnVideo( rgb, timestamp );
	};
	freenect_set_video_callback( mDevice, VideoCallback );

	//	stop current mode
	//	gr: commented out lets video start. maybe this is refcounted... so only turn off if we've started?
	//freenect_stop_video( mDevice );

	//	get freenect format
	auto FreenectFormat = GetFreenectVideoFormat( Format );
	
	//	setup format
	VideoMode = freenect_find_video_mode( Resolution, FreenectFormat );
	if ( !VideoMode.is_valid )
		return false;
	
	if ( !Soy::Assert( VideoMode.padding_bits_per_pixel==0, "don't support aligned pixel formats" ) )
		return false;
	
	VideoBuffer.Init( VideoMode.width, VideoMode.height, Format );
	if ( !Soy::Assert( VideoBuffer.GetPixelsArray().GetDataSize() == Device.mVideoMode.bytes, "Allocated wrong amount of pixel data" ) )
		return false;
	
	if ( freenect_set_video_mode( mDevice, VideoMode ) < 0 )
		return false;
	//freenect_set_video_buffer( mDevice, rgb_back);
	
	if ( freenect_start_video( mDevice ) < 0 )
		return false;
	
	//	save subdevice
	mVideoDevice = pDevice;
	
	return true;
}


bool SoyFreenectDevice::SetDepthFormat(freenect_resolution Resolution,SoyPixelsFormat::Type Format)
{
	//	create sub device
	TVideoDeviceMeta Meta;
	Meta.mSerial = DepthPrefix + mSerial;
	Meta.mDepth = true;
	std::stringstream Error;
	std::shared_ptr<SoyFreenectSubDevice> pDevice( new SoyFreenectSubDevice( *this, Meta, Error ) );

	auto& Device = *pDevice;
	auto& VideoMode = Device.mVideoMode;
	auto& VideoBuffer = Device.mVideoBuffer;

	//	stop current mode
	freenect_stop_depth( mDevice );
	
	//	get freenect format
	auto FreenectFormat = GetFreenectDepthFormat( Format );
	
	//	setup format
	VideoMode = freenect_find_depth_mode( Resolution, FreenectFormat );
	if ( !VideoMode.is_valid )
		return false;
	
	/*
	auto SoyFormat = SoyPixelsFormat::GetFormatFromChannelCount( mVideoMode.data_bits_per_pixel/8 );
	if ( SoyFormat == SoyPixelsFormat::Invalid )
	 return false;

*/
	//	setup format
	if ( !VideoBuffer.Init( VideoMode.width, VideoMode.height, Format ) )
		return false;
	
	if ( freenect_set_depth_mode( mDevice, VideoMode ) < 0 )
		return false;
	
	if ( freenect_start_depth( mDevice ) < 0 )
		return false;
	
	mDepthDevice = pDevice;
	
	return true;

}


void SoyFreenectDevice::CloseVideoDevice()
{
	std::Debug << __FUNCTION__ << std::endl;
}

void SoyFreenectDevice::CloseDepthDevice()
{
	std::Debug << __FUNCTION__ << std::endl;
}

void SoyFreenectDevice::OnVideo(void *rgb, uint32_t timestamp)
{
	if ( !Soy::Assert( rgb, "rgb data expected" ) )
		return;
	
	auto& pDevice = mVideoDevice;
	auto& VideoMode = pDevice->mVideoMode;
	auto& VideoBuffer = pDevice->mVideoBuffer;
	if ( !Soy::Assert( pDevice!=nullptr, "Expected video device" ) )
		return;
	
	//std::Debug << "On video " << timestamp << std::endl;

	auto& Pixels = VideoBuffer.GetPixelsArray();
	int Bytes = std::min( VideoMode.bytes, Pixels.GetDataSize() );
	memcpy( Pixels.GetArray(), rgb, Bytes );

	//	notify change
	SoyTime Timecode( static_cast<uint64>(timestamp) );
	pDevice->OnBufferChanged( Timecode );
}





void SoyFreenectDevice::OnDepth(void *depth, uint32_t timestamp)
{
	if ( !Soy::Assert( depth, "depth data expected" ) )
		return;
	
	auto& pDevice = mDepthDevice;
	auto& VideoMode = pDevice->mVideoMode;
	auto& VideoBuffer = pDevice->mVideoBuffer;
	if ( !Soy::Assert( pDevice!=nullptr, "Expected depth device" ) )
		return;
	
	//	gr: change all this to a SoyPixelsImpl and make a copy in the event!
	auto& Pixels = VideoBuffer.GetPixelsArray();
	auto* Depth16 = static_cast<uint16*>( depth );
	//	copy to target
	int Bytes = std::min( VideoMode.bytes, Pixels.GetDataSize() );
	memcpy( Pixels.GetArray(), Depth16, Bytes );
	
	static uint16 Min = ~0;
	static uint16 Max = 0;

	uint16 MaxDepth = SoyPixelsFormat::GetMaxValue( VideoBuffer.GetFormat() );
	uint16 InvalidDepth = SoyPixelsFormat::GetInvalidValue( VideoBuffer.GetFormat() );

	
	for ( int i=0;	i<VideoMode.bytes/2;	i++ )
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
	pDevice->OnBufferChanged( Timecode );
}



SoyFreenectSubDevice::SoyFreenectSubDevice(SoyFreenectDevice& Parent,const TVideoDeviceMeta& Meta,std::stringstream& Error) :
	TVideoDevice	( Meta, Error ),
	mParent			( Parent ),
	mMeta			( Meta )
{
	
}
