#include "PopKinect.h"
#include "TParameters.h"
#include <SoyDebug.h>
#include "TProtocolCli.h"
#include "TProtocolHttp.h"
#include <SoyApp.h>
#include "PopMain.h"
#include "TJobRelay.h"
#include <SoyPixels.h>
#include <SoyString.h>
#include <SoySkeleton.h>





TPopKinect::TPopKinect()
{
	//	add video contaienr
	std::shared_ptr<SoyVideoContainer> KinectContainer( new SoyFreenect() );
	mVideoCapture.AddContainer( KinectContainer );

	AddJobHandler("list", TParameterTraits(), *this, &TPopKinect::OnList );
	AddJobHandler("exit", TParameterTraits(), *this, &TPopKinect::OnExit );
	
	TParameterTraits GetXXXTraits;
	GetXXXTraits.mAssumedKeys.PushBack("serial");
	
	AddJobHandler("getdepth", GetXXXTraits, *this, &TPopKinect::OnGetDepth );
	AddJobHandler("getvideo", GetXXXTraits, *this, &TPopKinect::OnGetVideo );
	AddJobHandler("getframe", GetXXXTraits, *this, &TPopKinect::OnGetVideo );
	
	//	gr: this might want a serial?
	AddJobHandler("getskeleton", TParameterTraits(), *this, &TPopKinect::OnGetSkeleton );
}

void TPopKinect::AddChannel(std::shared_ptr<TChannel> Channel)
{
	if ( !Channel )
		return;
	mChannels.push_back( Channel );
	TJobHandler::BindToChannel( *Channel );
}

void TPopKinect::OnExit(TJobAndChannel& JobAndChannel)
{
	mConsoleApp.Exit();
	
	//	should probably still send a reply
	TJobReply Reply( JobAndChannel );
	Reply.mParams.AddDefaultParam(std::string("exiting..."));
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}


void TPopKinect::OnGetSkeleton(TJobAndChannel& JobAndChannel)
{
	TJobReply Reply( JobAndChannel );
	
	SoyData_Stack<TSkeleton> Skeleton;
#if defined(ENABLE_NITE)
	Skeleton.mValue = mNite.GetLastSkeleton();
#endif
	
	if ( !Skeleton.mValue.IsValid() )
	{
#if defined(ENABLE_SKELTRACK)
		Skeleton.mValue = mSkelTrack.GetLastSkeleton();
#endif
	}
	
	SoyData_Stack<json::Object>* pJson = new SoyData_Stack<json::Object>();
	std::shared_ptr<SoyData> Json( pJson );
	pJson->Encode( Skeleton );
	
	Reply.mParams.AddDefaultParam( Json );
 
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}



void TPopKinect::OnGetVideo(TJobAndChannel& JobAndChannel)
{
	//	gr: need to distinguish between depth and video on a single video device....
	OnGetDepth( JobAndChannel );
}

void TPopKinect::OnGetDepth(TJobAndChannel& JobAndChannel)
{
	const TJob& Job = JobAndChannel;
	TJobReply Reply( JobAndChannel );
	
	auto Serial = Job.mParams.GetParamAs<std::string>("serial");
	auto AsMemFile = Job.mParams.GetParamAsWithDefault<bool>("memfile",true);
	
	std::stringstream Error;
	auto Device = mVideoCapture.GetDevice( Serial, Error );
	
	if ( !Device )
	{
		std::stringstream ReplyError;
		ReplyError << "Device " << Serial << " not found " << Error.str();
		Reply.mParams.AddErrorParam( ReplyError.str() );
		TChannel& Channel = JobAndChannel;
		Channel.OnJobCompleted( Reply );
		return;
	}
	
	//	grab pixels
	auto& LastFrame = Device->GetLastFrame( Error );
	if ( LastFrame.IsValid() )
	{
		TYPE_MemFile MemFile( LastFrame.mPixels.mMemFileArray );
		Reply.mParams.AddDefaultParam( MemFile );
	}
	
	//	add error if present (last frame could be out of date)
	if ( !Error.str().empty() )
		Reply.mParams.AddErrorParam( Error.str() );
	
	//	add other stats
	auto FrameRate = Device->GetFps();
	auto FrameMs = Device->GetFrameMs();
	Reply.mParams.AddParam("fps", FrameRate);
	Reply.mParams.AddParam("framems", FrameMs );
	Reply.mParams.AddParam("serial", Device->GetMeta().mSerial );
	
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
	
}

//	copied from TPopCapture::OnListDevices
void TPopKinect::OnList(TJobAndChannel& JobAndChannel)
{
	TJobReply Reply( JobAndChannel );
	
	Array<TVideoDeviceMeta> Metas;
	mVideoCapture.GetDevices( GetArrayBridge(Metas) );
	
	std::stringstream MetasString;
	for ( int i=0;	i<Metas.GetSize();	i++ )
	{
		auto& Meta = Metas[i];
		if ( i > 0 )
			MetasString << ",";
		
		MetasString << Meta;
	}
	
	Reply.mParams.AddDefaultParam( MetasString.str() );
	
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}



class TChannelLiteral : public TChannel
{
public:
	TChannelLiteral(SoyRef ChannelRef) :
	TChannel	( ChannelRef )
	{
	}
	
	virtual void				GetClients(ArrayBridge<SoyRef>&& Clients)
	{
		
	}
	
	bool				FixParamFormat(TJobParam& Param,std::stringstream& Error) override
	{
		return true;
	}
	void		Execute(std::string Command)
	{
		TJobParams Params;
		Execute( Command, Params );
	}
	void		Execute(std::string Command,const TJobParams& Params)
	{
		auto& Channel = *this;
		TJob Job;
		Job.mParams = Params;
		Job.mParams.mCommand = Command;
		Job.mChannelMeta.mChannelRef = Channel.GetChannelRef();
		Job.mChannelMeta.mClientRef = SoyRef("x");
		
		//	send job to handler
		Channel.OnJobRecieved( Job );
	}
	
	//	we don't do anything, but to enable relay, we say it's "done"
	virtual bool				SendJobReply(const TJobReply& Job) override
	{
		OnJobSent( Job );
		return true;
	}
	virtual bool				SendCommandImpl(const TJob& Job) override
	{
		OnJobSent( Job );
		return true;
	}
};



//	horrible global for lambda
std::shared_ptr<TChannel> gStdioChannel;



TPopAppError::Type PopMain(TJobParams& Params)
{
	std::cout << Params << std::endl;
	
	//	job handler
	TPopKinect App;

	auto CommandLineChannel = std::shared_ptr<TChan<TChannelLiteral,TProtocolCli>>( new TChan<TChannelLiteral,TProtocolCli>( SoyRef("cmdline") ) );
	
	//	create stdio channel for commandline output
	auto StdioChannel = CreateChannelFromInputString("std:", SoyRef("stdio") );
	gStdioChannel = StdioChannel;
	auto HttpChannel = CreateChannelFromInputString("http:8080-8090", SoyRef("http") );
	auto WebSocketChannel = CreateChannelFromInputString("ws:json:9090-9099", SoyRef("websock") );
	auto SocksChannel = CreateChannelFromInputString("cli:7070-7079", SoyRef("socks") );
	
	App.AddChannel( CommandLineChannel );
	App.AddChannel( StdioChannel );
	App.AddChannel( HttpChannel );
	App.AddChannel( WebSocketChannel );
	App.AddChannel( SocksChannel );

	//	when the commandline SENDs a command (a reply), send it to stdout
	auto RelayFunc = [](TJobAndChannel& JobAndChannel)
	{
		if ( !gStdioChannel )
			return;
		TJob Job = JobAndChannel;
		Job.mChannelMeta.mChannelRef = gStdioChannel->GetChannelRef();
		Job.mChannelMeta.mClientRef = SoyRef();
		gStdioChannel->SendCommand( Job );
	};
	CommandLineChannel->mOnJobSent.AddListener( RelayFunc );


#if defined(ENABLE_NITE)
	//	auto-init kinect if we're not using depth from open ni
	if ( !App.mNite.mDepthFromDevice )
	{

		//	feed depth to skeleton trackers
		App.mFreenect.mOnDepthFrame.AddListener( static_cast<SoySkeletonDetector&>(App.mNite), &SoySkeletonDetector::OnDepthFrame );
		App.mFreenect.mOnDepthFrame.AddListener( static_cast<SoySkeletonDetector&>(App.mSkelTrack), &SoySkeletonDetector::OnDepthFrame );

		//	auto start
		CommandLineChannel->Execute("init", TJobParams() );
		CommandLineChannel->Execute("open", TJobParams() );
	}
	else
	{
		App.mNite.mOnDeviceDepth.AddListener( CopyDepthFunc );
	}
#endif
	
	//	run
	App.mConsoleApp.WaitForExit();

	gStdioChannel.reset();
	return TPopAppError::Success;
}




