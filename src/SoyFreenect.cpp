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



TTempJobAndChannel::TTempJobAndChannel(TJobAndChannel& JobAndChannel) :
	mJob		( JobAndChannel.GetJob() ),
	mChannel	( &JobAndChannel.GetChannel() )
{
}


SoyFreenect::SoyFreenect()
{
	TParameterTraits DeviceTraits;
	DeviceTraits.mAssumedKeys.PushBack("device");
	DeviceTraits.mDefaultParams.PushBack( std::make_tuple("device","0" ) );
	

	AddJobHandler("init", TParameterTraits(), *this, &SoyFreenect::OnJobInit );
	AddJobHandler("open", DeviceTraits, *this, &SoyFreenect::OnJob );
	AddJobHandler("close", DeviceTraits, *this, &SoyFreenect::OnJob );
	AddJobHandler("getvideo", DeviceTraits, *this, &SoyFreenect::OnJob );

}

SoyFreenect::~SoyFreenect()
{
	if ( mThread )
	{
		mThread->waitForThread();
		mThread.reset();
	}
}


void SoyFreenect::OnJobInit(TJobAndChannel& JobAndChannel)
{
	//	request to init, if we don't have a queue, start it
	TJobReply Reply( JobAndChannel );
	
	if ( !mThread )
		mThread.reset( new SoyFreenectContextLoop(*this) );
	
	Reply.mParams.AddDefaultParam("OK");
	
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}

void SoyFreenect::OnJob(TJobAndChannel& JobAndChannel)
{
	//	put in the thread's queue
	if ( !mThread )
	{
		TJobReply Reply( JobAndChannel );
		
		Reply.mParams.AddErrorParam("Freenect not initialised");
		
		TChannel& Channel = JobAndChannel;
		Channel.OnJobCompleted( Reply );
		return;
	}
	
	//	gr: what we want here is a semaphore or callback or yeild to wait for the thread to loop where can do messages
	mThread->QueueJob( JobAndChannel );
}

/*
void SoyFreenect::GetDevices(ArrayBridge<std::string>&& Serials,std::stringstream& Error)
{
	auto* Context = mThread ? mThread->mContext : nullptr;
	if ( !Context )
	{
		Error << "No context";
		return;
	}
	
	int NumDevices = freenect_num_devices( Context );
	for ( int i=0;	i<NumDevices;	i++ )
	{
		std::stringstream DeviceSerial;
		DeviceSerial << i;
		Serials.PushBack( DeviceSerial.str() );
	}
}
*/

SoyFreenectContextLoop::SoyFreenectContextLoop(SoyFreenect& Parent) :
	SoyThread	( "SoyFreenectContextLoop" ),
	mContext	( nullptr ),
	mParent		( Parent )
{
	startThread();
}

SoyFreenectContextLoop::~SoyFreenectContextLoop()
{
	//	wait for thread?
	
	if ( mContext )
	{
		freenect_shutdown( mContext );
		mContext = nullptr;
	}
}
	
void SoyFreenectContextLoop::threadedFunction()
{
	//	init context
	freenect_usb_context* UsbContext = nullptr;
	auto Result = freenect_init( &mContext, UsbContext );
	if ( Result < 0 )
	{
		std::Debug << "freenect_init error: " << Result;
		Soy::Assert( mContext == nullptr, "Expected null context" );
		return;
	}

	freenect_set_log_level( mContext, FREENECT_LOG_WARNING);
	freenect_device_flags flags = (freenect_device_flags)( FREENECT_DEVICE_CAMERA );
	freenect_select_subdevices( mContext, flags );

	Array<SoyFreenectDeviceMeta> DeviceMetas;
	
	//	get the sub device serials
	struct freenect_device_attributes* DeviceList = nullptr;
	auto ListError = freenect_list_device_attributes( mContext, &DeviceList );
	if ( true )
	{
		auto Device = DeviceList;
		while ( Device )
		{
			auto& Meta = DeviceMetas.PushBack();
			Meta.mSerial = Device->camera_serial;
			Device = Device->next;
		}
		freenect_free_device_attributes( DeviceList );
	}

	
	std::Debug << DeviceMetas.GetSize() << " Freenect devices: ";
	for ( int d=0;	d<DeviceMetas.GetSize();	d++ )
	{
		std::Debug << DeviceMetas[d].mSerial << " ";
	}
	std::Debug << std::endl;
	
	
	while ( isThreadRunning() )
	{
		Sleep();
		
		//	process jobs
		//	gr: can I re-use jobhandler here?
		auto QueueJobAndChannel = mQueue.Pop();
		while ( QueueJobAndChannel.IsValid() )
		{
			//	process
			TJobAndChannel JobAndChannel( QueueJobAndChannel.mJob, *QueueJobAndChannel.mChannel );
			auto& Command = QueueJobAndChannel.mJob.mParams.mCommand;
			std::Debug << "Process kinect job: " << Command << std::endl;
			if ( Command == "open" )
				OnOpenDevice( JobAndChannel );
			else if ( Command == "close" )
				OnCloseDevice( JobAndChannel );
			else if ( Command == "shutdown" )
				OnShutdown( JobAndChannel );
			else if ( Command == "getvideo" )
				OnGetVideo( JobAndChannel );
			
			QueueJobAndChannel = mQueue.Pop();
		}
		
		timeval Timeout = {0,0};
		auto Result = freenect_process_events_timeout( mContext, &Timeout );
		if ( Result < 0 )
		{
			std::Debug << "Freenect_events error: " << Result;
			break;
		}
	}
}


bool SoyFreenectContextLoop::QueueJob(TJobAndChannel &JobAndChannel)
{
	TTempJobAndChannel Temp( JobAndChannel );
	mQueue.Push( Temp );

	return true;
}

void SoyFreenectContextLoop::OnOpenDevice(TJobAndChannel& JobAndChannel)
{
	const TJob& Job = JobAndChannel;
	
	std::stringstream Error;
	int DeviceSerial;
	if ( !Job.mParams.GetParamAs<int>("device",DeviceSerial) )
	{
		Error << "device not specified";
	}
	else
	{
		if ( !OpenDevice( DeviceSerial ) )
			Error << "Error opening device";
	}
	
	TJobReply Reply( JobAndChannel );
	
	if ( !Error.str().empty() )
		Reply.mParams.AddErrorParam( Error.str() );
	else
		Reply.mParams.AddDefaultParam("OK");

	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}

void SoyFreenectContextLoop::OnCloseDevice(TJobAndChannel& JobAndChannel)
{
	const TJob& Job = JobAndChannel;
	
	std::stringstream Error;
	int DeviceSerial;
	if ( !Job.mParams.GetParamAs("device",DeviceSerial) )
	{
		Error << "device not specified";
	}
	else
	{
		CloseDevice( DeviceSerial );
	}
	
	TJobReply Reply( JobAndChannel );
	
	if ( !Error.str().empty() )
		Reply.mParams.AddErrorParam( Error.str() );
	else
		Reply.mParams.AddDefaultParam("OK");
	
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}

void SoyFreenectContextLoop::OnShutdown(TJobAndChannel& JobAndChannel)
{
	Shutdown();
	
	TJobReply Reply( JobAndChannel );
	
	Reply.mParams.AddDefaultParam("OK");
	
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );

}

void SoyFreenectContextLoop::OnGetVideo(TJobAndChannel& JobAndChannel)
{
	const TJob& Job = JobAndChannel;
	
	std::stringstream Error;
	int DeviceSerial;
	SoyPixels Pixels;
	if ( !Job.mParams.GetParamAs("device",DeviceSerial) )
	{
		Error << "device not specified";
	}
	else
	{
		//	get the device
		auto Device = mDevices.find(DeviceSerial);
		if ( Device != mDevices.end() )
		{
			auto& Buffer = Device->second->mVideoBuffer;
			if ( !Buffer.IsValid() )
				Error << "Video data invalid" << Buffer.GetWidth() << "x" << Buffer.GetHeight() << " " << Buffer.GetFormat();
			else
				Pixels.Copy( Buffer );
		}
		else
		{
			Error << "Unknown device " << DeviceSerial;
		}
	}
	
	TJobReply Reply( JobAndChannel );
	
	if ( !Error.str().empty() )
		Reply.mParams.AddErrorParam( Error.str() );
	else
		Reply.mParams.AddDefaultParam( Pixels );
	
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}


bool SoyFreenectContextLoop::OpenDevice(int DeviceSerial)
{
	//	already exists
	if ( mDevices.find(DeviceSerial) != mDevices.end() )
		return true;
	
	//	create device
	std::shared_ptr<SoyFreenectDevice> Device( new SoyFreenectDevice(*this,DeviceSerial) );
	if ( !Device->Open() )
		return false;

	//	opened okay, save it
	mDevices[DeviceSerial] = Device;
	return true;
}

void SoyFreenectContextLoop::CloseDevice(int DeviceSerial)
{
	//	doesn't exist
	if ( mDevices.find(DeviceSerial) == mDevices.end() )
		return;
	
	//	kill device
	mDevices[DeviceSerial]->Close();
	mDevices[DeviceSerial].reset();
	
	Soy::Assert(false,"Can't remember here, std::erase?");
}

void SoyFreenectContextLoop::Shutdown()
{
	stopThread();
}

SoyFreenectDevice::SoyFreenectDevice(SoyFreenectContextLoop& Parent,int Serial) :
	mParent			( Parent ),
	mSerial			( Serial )
{
}


bool SoyFreenectDevice::Open()
{
	//	open device
	auto Result = freenect_open_device( mParent.GetContext(), &mDevice, mSerial );
	if ( Result < 0 )
		return false;
	
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
	this->mParent.mParent.mOnVideoFrame.OnTriggered( mVideoBuffer );
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
	this->mParent.mParent.mOnDepthFrame.OnTriggered( mDepthBuffer );
}
