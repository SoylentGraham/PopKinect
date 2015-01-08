#include "SoyNite.h"
#include <SoyDebug.h>
#include <json/elements.h>
#include "TSerialisation.h"



#define PRINT_RC(nRetVal, what)										\
if (nRetVal != XN_STATUS_OK)									\
{																\
std::Debug << what << " failed: " << xnGetStatusString(nRetVal) << std::endl;	\
}


#define CHECK_RC(nRetVal, what)										\
if (nRetVal != XN_STATUS_OK)									\
{																\
	PRINT_RC( nRetVal, what );	\
	return false;												\
}


void XN_CALLBACK_TYPE MyCalibrationInProgress(xn::SkeletonCapability& capability, XnUserID id, XnCalibrationStatus calibrationError, void* pCookie);
void XN_CALLBACK_TYPE MyPoseInProgress(xn::PoseDetectionCapability& capability, const XnChar* strPose, XnUserID id, XnPoseDetectionStatus poseError, void* pCookie);

std::map<XnUInt32, std::pair<XnCalibrationStatus, XnPoseDetectionStatus> > m_Errors;
void XN_CALLBACK_TYPE MyCalibrationInProgress(xn::SkeletonCapability& /*capability*/, XnUserID id, XnCalibrationStatus calibrationError, void* /*pCookie*/)
{
	m_Errors[id].first = calibrationError;
}
void XN_CALLBACK_TYPE MyPoseInProgress(xn::PoseDetectionCapability& /*capability*/, const XnChar* /*strPose*/, XnUserID id, XnPoseDetectionStatus poseError, void* /*pCookie*/)
{
	m_Errors[id].second = poseError;
}




// Callback: New user was detected
void XN_CALLBACK_TYPE User_NewUser(xn::UserGenerator& /*generator*/, XnUserID nId, void* pCookie)
{
	auto& Detector = *static_cast<SoyNite*>( pCookie );
	
	XnUInt32 epochTime = 0;
	xnOSGetEpochTime(&epochTime);
	std::Debug << "New User " << nId << std::endl;
	
	// New user found
	if ( Detector.mNeedPose )
	{
		auto Result = Detector.mUserGenerator.GetPoseDetectionCap().StartPoseDetection( Detector.mPoseName, nId);
		std::Debug << "StartPoseDetection: " << xnGetStatusString(Result) << std::endl;
	}
	else
	{
		auto Result = Detector.mUserGenerator.GetSkeletonCap().RequestCalibration(nId, TRUE);
		std::Debug << "RequestCalibration: " << xnGetStatusString(Result) << std::endl;
	}
}
// Callback: An existing user was lost
void XN_CALLBACK_TYPE User_LostUser(xn::UserGenerator& /*generator*/, XnUserID nId, void* pCookie)
{
	auto& Detector = *static_cast<SoyNite*>( pCookie );

	XnUInt32 epochTime = 0;
	xnOSGetEpochTime(&epochTime);
	printf("%d Lost user %d\n", epochTime, nId);
}
// Callback: Detected a pose
void XN_CALLBACK_TYPE UserPose_PoseDetected(xn::PoseDetectionCapability& /*capability*/, const XnChar* strPose, XnUserID nId, void* pCookie)
{
	auto& Detector = *static_cast<SoyNite*>( pCookie );

	XnUInt32 epochTime = 0;
	xnOSGetEpochTime(&epochTime);
	printf("%d Pose %s detected for user %d\n", epochTime, strPose, nId);
	Detector.mUserGenerator.GetPoseDetectionCap().StopPoseDetection(nId);
	Detector.mUserGenerator.GetSkeletonCap().RequestCalibration(nId, TRUE);
}
// Callback: Started calibration
void XN_CALLBACK_TYPE UserCalibration_CalibrationStart(xn::SkeletonCapability& /*capability*/, XnUserID nId, void* pCookie)
{
	auto& Detector = *static_cast<SoyNite*>( pCookie );

	XnUInt32 epochTime = 0;
	xnOSGetEpochTime(&epochTime);
	printf("%d Calibration started for user %d\n", epochTime, nId);
}
// Callback: Finished calibration
void XN_CALLBACK_TYPE UserCalibration_CalibrationComplete(xn::SkeletonCapability& /*capability*/, XnUserID nId, XnCalibrationStatus eStatus, void* pCookie)
{
	auto& Detector = *static_cast<SoyNite*>( pCookie );

	XnUInt32 epochTime = 0;
	xnOSGetEpochTime(&epochTime);
	if (eStatus == XN_CALIBRATION_STATUS_OK)
	{
		// Calibration succeeded
		printf("%d Calibration complete, start tracking user %d\n", epochTime, nId);
		Detector.mUserGenerator.GetSkeletonCap().StartTracking(nId);
	}
	else
	{
		// Calibration failed
		printf("%d Calibration failed for user %d\n", epochTime, nId);
		if(eStatus==XN_CALIBRATION_STATUS_MANUAL_ABORT)
		{
			printf("Manual abort occured, stop attempting to calibrate!");
			return;
		}
		
		if ( Detector.mNeedPose )
		{
			Detector.mUserGenerator.GetPoseDetectionCap().StartPoseDetection( Detector.mPoseName, nId);
		}
		else
		{
			Detector.mUserGenerator.GetSkeletonCap().RequestCalibration(nId, TRUE);
		}
	}
}



/*
std::ostream& operator<< (std::ostream &out,const XnStatus &in)
{
	auto* String = xnGetStatusString( in );
	
	if ( String )
	{
		out << String;
	}
	else
	{
		//unsigned int Status = in;
		//out << "(Unknown XnStatus " << Status << ")";
		out << "(Unknown XnStatus " << ")";
	}
	return out;
}
*/


XnCallbackHandle hUserCallbacks, hCalibrationStart, hCalibrationComplete, hPoseDetected, hCalibrationInProgress, hPoseInProgress;


bool InitOpenni(SoyNite& Detector)
{
	XnStatus nRetVal = XN_STATUS_OK;

	
	nRetVal = Detector.mContext.Init();
	if (nRetVal != XN_STATUS_OK)
	{
		std::Debug << "failed to init context" << std::endl;
		return false;
	}

	xn::DepthGenerator DeviceDepthGenerator;
	auto DepthGeneratorResult = Detector.mDepthGenerator.Create(Detector.mContext);
	PRINT_RC( DepthGeneratorResult, "create depth generator" );
	bool HasDeviceDepthGenerator = (XN_STATUS_OK == DepthGeneratorResult);

	//	use openni device
	if ( Detector.mDepthFromDevice )
	{
		if ( !HasDeviceDepthGenerator )
			return false;
		
		xn::DepthGenerator DeviceDepthGeneratorNode;
		nRetVal = Detector.mContext.FindExistingNode(XN_NODE_TYPE_DEPTH, DeviceDepthGeneratorNode);
		//Detector.mDepthGenerator = DeviceDepthGenerator;
		Detector.mDepthGenerator = DeviceDepthGeneratorNode;
	}
	else
	{
		//	gr: creating depth from scratch fails, so base it on device... need to fix that but google is no help
		xn::MockDepthGenerator& mockDepth = Detector.mMockDepthGenerator;
		if ( HasDeviceDepthGenerator )
		{
			xn::DepthGenerator DeviceDepthGeneratorNode;
			nRetVal = Detector.mContext.FindExistingNode(XN_NODE_TYPE_DEPTH, DeviceDepthGeneratorNode);
			nRetVal = mockDepth.CreateBasedOn( DeviceDepthGeneratorNode );
			CHECK_RC(nRetVal, "Create mock depth");
			DeviceDepthGeneratorNode.Release();
			DeviceDepthGenerator.Release();
		}
		else
		{
			nRetVal = mockDepth.Create( Detector.mContext );
			CHECK_RC(nRetVal, "Create mock depth");
		}
		Detector.mDepthGenerator = mockDepth;

		
		//	setup data (maybe can init/change as we recieve [meta]data)
		XnMapOutputMode defaultMode;
		defaultMode.nXRes = 320;
		defaultMode.nYRes = 240;
		defaultMode.nFPS = 30;
		nRetVal = mockDepth.SetMapOutputMode(defaultMode);
		CHECK_RC(nRetVal, "set default mode");
		
		// set FOV
		XnFieldOfView fov;
		fov.fHFOV = 1.0225999419141749;
		fov.fVFOV = 0.79661567681716894;
		nRetVal = mockDepth.SetGeneralProperty(XN_PROP_FIELD_OF_VIEW, sizeof(fov), &fov);
		CHECK_RC(nRetVal, "set FOV");
		
		
		XnDepthPixel MaxDepth = 1024;
		nRetVal = mockDepth.SetGeneralProperty(XN_PROP_DEVICE_MAX_DEPTH, sizeof(MaxDepth), &MaxDepth);
		CHECK_RC(nRetVal, "set MaxDepth");
		
		
		XnUInt32 nDataSize = defaultMode.nXRes * defaultMode.nYRes * sizeof(XnDepthPixel);
		XnDepthPixel* pData = (XnDepthPixel*)xnOSCallocAligned(nDataSize, 1, XN_DEFAULT_MEM_ALIGN);
		
		nRetVal = mockDepth.SetData(1, 0, nDataSize, pData);
		CHECK_RC(nRetVal, "set empty depth map");
	}
	
	//	create user generator
	nRetVal = Detector.mUserGenerator.Create( Detector.mContext );
	CHECK_RC(nRetVal, "create generator");

	
	if (!Detector.mUserGenerator.IsCapabilitySupported(XN_CAPABILITY_SKELETON))
	{
		printf("Supplied user generator doesn't support skeleton\n");
		return false;
	}
	
	nRetVal = Detector.mUserGenerator.RegisterUserCallbacks(User_NewUser, User_LostUser, &Detector, hUserCallbacks);
	CHECK_RC(nRetVal, "Register to user callbacks");
	nRetVal = Detector.mUserGenerator.GetSkeletonCap().RegisterToCalibrationStart(UserCalibration_CalibrationStart, &Detector, hCalibrationStart);
	CHECK_RC(nRetVal, "Register to calibration start");
	nRetVal = Detector.mUserGenerator.GetSkeletonCap().RegisterToCalibrationComplete(UserCalibration_CalibrationComplete, &Detector, hCalibrationComplete);
	CHECK_RC(nRetVal, "Register to calibration complete");
	
	if (Detector.mUserGenerator.GetSkeletonCap().NeedPoseForCalibration())
	{
		Detector.mNeedPose = true;
		if (!Detector.mUserGenerator.IsCapabilitySupported(XN_CAPABILITY_POSE_DETECTION))
		{
			printf("Pose required, but not supported\n");
			return false;
		}
		nRetVal = Detector.mUserGenerator.GetPoseDetectionCap().RegisterToPoseDetected(UserPose_PoseDetected, &Detector, hPoseDetected);
		//CHECK_RC(nRetVal, "Register to Pose Detected");
		Detector.mUserGenerator.GetSkeletonCap().GetCalibrationPose( Detector.mPoseName );
		
		nRetVal = Detector.mUserGenerator.GetPoseDetectionCap().RegisterToPoseInProgress(MyPoseInProgress, &Detector, hPoseInProgress);
		CHECK_RC(nRetVal, "Register to pose in progress");
	}
	
	Detector.mUserGenerator.GetSkeletonCap().SetSkeletonProfile(XN_SKEL_PROFILE_ALL);
	
	nRetVal = Detector.mUserGenerator.GetSkeletonCap().RegisterToCalibrationInProgress(MyCalibrationInProgress, &Detector, hCalibrationInProgress);
	CHECK_RC(nRetVal, "Register to calibration in progress");
	
	nRetVal = Detector.mContext.StartGeneratingAll();
	CHECK_RC(nRetVal, "StartGenerating");
	
	return true;
}

SoyNite::SoyNite() :
	mPoseName			( "\0" ),
	mNeedPose			( false ),
	mDepthFromDevice	( true )
{
	if ( !InitOpenni( *this ) )
		return;

	startThread();
}

SoyNite::~SoyNite()
{
	//	too late!
	this->waitForThread();

	
	mMockDepthGenerator.Release();
	mDepthGenerator.Release();
	mUserGenerator.Release();
	mContext.Release();
}


void SoyNite::threadedFunction()
{
	while ( isThreadRunning() )
	{
		Sleep(1);
		
		if ( mDepthFromDevice )
		{
			//	read depth
			xn::DepthMetaData Meta;
			mDepthGenerator.GetMetaData( Meta );
			
			if ( Meta.IsDataNew() )
			{
				auto* DeviceDepthPixels = mDepthGenerator.GetDepthMap();
				SoyPixels NewDepth;
				//	gr: figure out this format!
				if ( NewDepth.Init( Meta.XRes(), Meta.YRes(), SoyPixelsFormat::KinectDepth ) )
				{
					auto& NewDepthPixelsArray = NewDepth.GetPixelsArray();
					if ( NewDepthPixelsArray.GetDataSize() == mDepthGenerator.GetDataSize() )
					{
						memcpy( NewDepthPixelsArray.GetArray(), DeviceDepthPixels, NewDepthPixelsArray.GetDataSize() );
						mOnDeviceDepth.OnTriggered( NewDepth );
						OnDepthFrame( NewDepth );
					}
				}
			}
			else
			{
				mContext.WaitOneUpdateAll( mDepthGenerator );
				//mContext.WaitOneUpdateAll( mUserGenerator );
			}
		}
		
		ThreadLoop();
	}
}


BufferArray<XnSkeletonJoint,100> EnumJoints()
{
	XnSkeletonJoint Joints[] =
	{
		XN_SKEL_HEAD,
		XN_SKEL_NECK,
		XN_SKEL_TORSO,
		XN_SKEL_WAIST,
		XN_SKEL_LEFT_COLLAR,
		XN_SKEL_LEFT_SHOULDER,
		XN_SKEL_LEFT_ELBOW,
		XN_SKEL_LEFT_WRIST,
		XN_SKEL_LEFT_HAND,
		XN_SKEL_LEFT_FINGERTIP,
		XN_SKEL_RIGHT_COLLAR,
		XN_SKEL_RIGHT_SHOULDER,
		XN_SKEL_RIGHT_ELBOW,
		XN_SKEL_RIGHT_WRIST,
		XN_SKEL_RIGHT_HAND,
		XN_SKEL_RIGHT_FINGERTIP,
		XN_SKEL_LEFT_HIP,
		XN_SKEL_LEFT_KNEE,
		XN_SKEL_LEFT_ANKLE,
		XN_SKEL_LEFT_FOOT,
		XN_SKEL_RIGHT_HIP,
		XN_SKEL_RIGHT_KNEE,
		XN_SKEL_RIGHT_ANKLE,
		XN_SKEL_RIGHT_FOOT,
	};
	
	return BufferArray<XnSkeletonJoint,100>( Joints );
}

std::string JointToString(XnSkeletonJoint eJoint)
{
	switch ( eJoint )
	{
	case XN_SKEL_HEAD:	return "Head";
	case XN_SKEL_NECK:	return "Neck";
	case XN_SKEL_TORSO:	return "Torso";
	case XN_SKEL_WAIST:	return "Waist";
	
	case XN_SKEL_LEFT_COLLAR:	return "LeftCollar";
	case XN_SKEL_LEFT_SHOULDER:	return "LeftShoulder";
	case XN_SKEL_LEFT_ELBOW:	return "LeftElbow";
	case XN_SKEL_LEFT_WRIST:	return "LeftWrist";
	case XN_SKEL_LEFT_HAND:	return "LeftHand";
	case XN_SKEL_LEFT_FINGERTIP:	return "LeftFingertip";
	
	case XN_SKEL_RIGHT_COLLAR:	return "RightCollar";
	case XN_SKEL_RIGHT_SHOULDER:	return "RightShoulder";
	case XN_SKEL_RIGHT_ELBOW:	return "RightElbow";
	case XN_SKEL_RIGHT_WRIST:	return "RightWrist";
	case XN_SKEL_RIGHT_HAND:	return "RightHand";
	case XN_SKEL_RIGHT_FINGERTIP:	return "RightFingerTip";
	
	case XN_SKEL_LEFT_HIP:	return "LeftHip";
	case XN_SKEL_LEFT_KNEE:	return "LeftKnee";
	case XN_SKEL_LEFT_ANKLE:	return "LeftAnkle";
	case XN_SKEL_LEFT_FOOT:	return "LeftFoot";
	
	case XN_SKEL_RIGHT_HIP:	return "RightHip";
	case XN_SKEL_RIGHT_KNEE:	return "RightKnee";
	case XN_SKEL_RIGHT_ANKLE:	return "RightAnkle";
	case XN_SKEL_RIGHT_FOOT:	return "RightFoot";
	
	 default:
			return "Unknown";
	}
}

bool AddJointToSkeleton(TSkeleton& Skeleton,XnUserID User,XnSkeletonJoint eJoint,xn::UserGenerator& UserGenerator)
{
	bool JointActive = UserGenerator.GetSkeletonCap().IsJointActive(eJoint);
	if ( !JointActive )
		return false;

	XnSkeletonJointPosition joint;
	UserGenerator.GetSkeletonCap().GetSkeletonJointPosition( User, eJoint, joint );
	static float MinConfidence = 0.2f;
	if (joint.fConfidence < MinConfidence )
		return false;
	
	XnPoint3D pt;
	pt = joint.position;
	
	//mDepthGenerator.ConvertRealWorldToProjective(1, &pt, &pt);
	TJoint Joint;
	static float Scale = 1.f / 100.f;
	Joint.mWorldPos.x = pt.X * Scale;
	Joint.mWorldPos.y = pt.Y * Scale;
	Joint.mWorldPos.z = pt.Z * Scale;
	Joint.mScreenPos.x = 0;
	Joint.mScreenPos.y = 0;
	Joint.mName = JointToString( eJoint );

	Skeleton.AddJoint( Joint );
	return true;
}



void SoyNite::ProcessDepth(std::shared_ptr<SoyPixels> pDepthPixels)
{
	//	push depth data into our depth generator
	if ( !mDepthFromDevice )
	{
		SoyPixels& DepthPixels = *pDepthPixels;

		DepthPixels.SetFormat( SoyPixelsFormat::KinectDepth );
		
		//	get expected resolution
		xn::DepthMetaData Meta;
		mMockDepthGenerator.GetMetaData( Meta );
		DepthPixels.ResizeFastSample( Meta.XRes(), Meta.YRes() );

		XnMapOutputMode Mode;
		Mode.nXRes = DepthPixels.GetWidth();
		Mode.nYRes = DepthPixels.GetHeight();
		Mode.nFPS = 30;
		XnStatus Result = XN_STATUS_OK;
		//	gr: causes crash in WaitOneUpdateAll :(
	//	auto Result = mMockDepthGenerator.SetMapOutputMode(Mode);
		if ( Result != XN_STATUS_OK )
			return;
		
		XnDepthPixel MaxDepth = SoyPixelsFormat::GetMaxValue( DepthPixels.GetFormat() );
		Result = mMockDepthGenerator.SetGeneralProperty(XN_PROP_DEVICE_MAX_DEPTH, sizeof(MaxDepth), &MaxDepth);
		if ( Result != XN_STATUS_OK )
			return;
		
		//	gr: change to format
		if ( DepthPixels.GetChannels() != 2 )
			return;
		
		SoyTime Now( true );
		XnUInt64 Timestamp = Now.GetTime();
		
		//	gr: when using device, frame before WaitOneUpdateAll is same as user generator
		XnUInt32 Frame = mUserGenerator.GetFrameID();
		
		Frame++;
		
		XnUInt32 DataSize = Mode.nXRes * Mode.nYRes * sizeof(XnDepthPixel);
		auto& PixelsArray = DepthPixels.GetPixelsArray();
		if ( PixelsArray.GetDataSize() != DataSize )
			return;
		
		const XnDepthPixel* DepthData = reinterpret_cast<const XnDepthPixel*>( PixelsArray.GetArray() );
		//XnDepthPixel* DepthData = (XnDepthPixel*)xnOSCallocAligned( DataSize, 1, XN_DEFAULT_MEM_ALIGN );
		Result = mMockDepthGenerator.SetData( Frame, Timestamp, DataSize, DepthData );
		if ( Result != XN_STATUS_OK )
			return;
	}
	
	//std::Debug << "user frame: " << mUserGenerator.GetFrameID() << "; depth frame: " << mDepthGenerator.GetFrameID() << std::endl;
	//	make user generator update
	mContext.WaitOneUpdateAll( mUserGenerator );
	
	/*
	xn::SceneMetaData sceneMD;
	xn::DepthMetaData depthMD;
	mDepthGenerator.GetMetaData(depthMD);
	*/
	
	XnUserID aUsers[15];
	XnUInt16 nUsers = 15;
	mUserGenerator.GetUsers(aUsers, nUsers);
	static int LastUserCount = -1;
	if ( nUsers != LastUserCount )
		std::Debug << nUsers << " users" << std::endl;
	LastUserCount = nUsers;
	
	auto Joints = EnumJoints();
	
	for (int i = 0; i < nUsers; ++i)
	{
		XnUserID User = aUsers[i];
		TSkeleton Skeleton;
		
		bool Tracking = mUserGenerator.GetSkeletonCap().IsTracking( User );
		//bool Calibrating = mUserGenerator.GetSkeletonCap().IsCalibrating( User );
	
		if ( !Tracking )
			continue;
		
		//	find joints
		for ( int j=0;	j<Joints.GetSize();	j++ )
		{
			auto eJoint = Joints[j];
			AddJointToSkeleton( Skeleton, User, eJoint, mUserGenerator );
		}
		
		OnNewSkeleton( Skeleton );
	}

	
	
}
