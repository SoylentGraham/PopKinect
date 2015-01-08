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
	AddJobHandler("list", TParameterTraits(), *this, &TPopKinect::OnList );
	AddJobHandler("exit", TParameterTraits(), *this, &TPopKinect::OnExit );
	AddJobHandler("getdepth", TParameterTraits(), *this, &TPopKinect::OnGetDepth );
	AddJobHandler("getskeleton", TParameterTraits(), *this, &TPopKinect::OnGetSkeleton );

	//	until the subsystem has a better solution, silently take jobs that the freenect lib will take
	Array<std::string> FreenectJobs;
	mFreenect.GetJobHandlerCommandList( GetArrayBridge( FreenectJobs ) );
	for ( int i=0;	i<FreenectJobs.GetSize();	i++ )
	{
		auto CommandName = FreenectJobs[i];
		AddJobHandler( CommandName, TParameterTraits(), *this, &TPopKinect::FakeJobHandler );
	}
}

void TPopKinect::AddChannel(std::shared_ptr<TChannel> Channel)
{
	if ( !Channel )
		return;
	mChannels.push_back( Channel );
	TJobHandler::BindToChannel( *Channel );
	
	mFreenect.BindToChannel( *Channel );
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


void TPopKinect::OnGetDepth(TJobAndChannel& JobAndChannel)
{
	const TJob& Job = JobAndChannel;
	
	std::stringstream Error;
	SoyPixels Pixels;
	Pixels.Copy( mLastDepth );
	
	std::Debug << "GetDepth: " << Pixels.GetFormat() << std::endl;
	//Pixels.SetFormat( SoyPixelsFormat::KinectDepth );
	
//	Pixels.ResizeFastSample( 100, 100 );
	
	
	TJobReply Reply( JobAndChannel );
	
	if ( !Error.str().empty() )
		Reply.mParams.AddErrorParam( Error.str() );
	else
		Reply.mParams.AddDefaultParam( Pixels );
	
	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
}

void TPopKinect::OnList(TJobAndChannel& JobAndChannel)
{
	/*
	TJobReply Reply( JobAndChannel );

	std::stringstream Error;
	std::stringstream Output;
	
	Array<std::string> Serials;
	mFreenect.GetDevices( GetArrayBridge(Serials), Error );
		
	for ( int i=0;	i<Serials.GetSize();	i++ )
	{
		if ( i > 0 )
			Output << ' ';
		Output << Serials[i];
	}

	if ( !Error.str().empty() )
		Reply.mParams.AddErrorParam( Error.str() );

	Reply.mParams.AddDefaultParam( Output.str() );

	TChannel& Channel = JobAndChannel;
	Channel.OnJobCompleted( Reply );
	return;
	 */
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
	
	App.AddChannel( CommandLineChannel );
	App.AddChannel( StdioChannel );
	App.AddChannel( HttpChannel );
	App.AddChannel( WebSocketChannel );

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

	//	copy texture to app for viewing
	auto CopyDepthFunc = [&App](const SoyPixels& Pixels)
	{
	//	std::Debug << "new frame from device " << std::endl;
		App.mLastDepth.Copy( Pixels );
	};

#if defined(ENABLE_NITE)
	//	auto-init kinect if we're not using depth from open ni
	if ( !App.mNite.mDepthFromDevice )
	{
		App.mFreenect.mOnDepthFrame.AddListener( CopyDepthFunc );

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




