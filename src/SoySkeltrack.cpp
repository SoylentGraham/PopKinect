#include "SoySkelTrack.h"
#include <SoyDebug.h>
#include <json/elements.h>
#include "TSerialisation.h"


std::string SkelJointIdToString(SkeltrackJointId JointId)
{
	switch ( JointId )
	{
		case SKELTRACK_JOINT_ID_HEAD:	return "Head";
		case SKELTRACK_JOINT_ID_LEFT_SHOULDER:	return "LeftShoulder";
		case SKELTRACK_JOINT_ID_RIGHT_SHOULDER:	return "RightShoulder";
		case SKELTRACK_JOINT_ID_LEFT_ELBOW:	return "LeftElbow";
		case SKELTRACK_JOINT_ID_RIGHT_ELBOW:	return "RightElbow";
		case SKELTRACK_JOINT_ID_LEFT_HAND:	return "LeftHand";
		case SKELTRACK_JOINT_ID_RIGHT_HAND:	return "RightHand";
		default:
		{
			std::stringstream String;
			String << "Uknown" << JointId;
			return String.str();
		}
	}
}


void AddJoint(TSkeleton& Skeleton,SkeltrackJoint& SkelJoint,SkeltrackJointId JointId)
{
	TJoint Joint;
	Joint.mName = SkelJointIdToString( JointId );
//	Joint.mName = SkelJointIdToString( SkelJoint.id );
	
	vec2f ScreenScale( 1.f, 1.f );
	vec3f WorldScale( 1.f/1000.f, 1.f/1000.f, 1.f/1000.f );
	
	Joint.mScreenPos = vec2f( SkelJoint.screen_x, SkelJoint.screen_y );
	Joint.mScreenPos *= ScreenScale;
	Joint.mWorldPos = vec3f( SkelJoint.x, SkelJoint.y, SkelJoint.z );
	Joint.mWorldPos *= WorldScale;
	
	Skeleton.AddJoint( Joint );
}

void AddJoint(TSkeleton& Skeleton,SkeltrackJointList& JointList,SkeltrackJointId JointId)
{
	auto Joint = skeltrack_joint_list_get_joint( JointList, JointId );
	if ( !Joint )
		return;
	
	AddJoint( Skeleton, *Joint, JointId );
}


SoySkelTrack::SoySkelTrack() :
	mSkelTrack	( nullptr )
{
	mSkelTrack = skeltrack_skeleton_new();
	startThread();
	
}

SoySkelTrack::~SoySkelTrack()
{
	//	too late!
	this->waitForThread();
}


void SoySkelTrack::ProcessDepth(std::shared_ptr<SoyPixels> DepthPixels)
{
//	std::Debug << "processing new depth data " << std::endl;
	auto& Pixels = *DepthPixels;
	Pixels.ResizeFastSample( 100, 100 );
	
	GError* Error = nullptr;
	auto JointList = skeltrack_skeleton_track_joints_sync(
											mSkelTrack,
										  reinterpret_cast<guint16*>( Pixels.GetPixelsArray().GetArray() ),
										  Pixels.GetWidth(),
										  Pixels.GetHeight(),
										nullptr,
										&Error );

	TSkeleton NewSkeleton;
	AddJoint( NewSkeleton, JointList, SKELTRACK_JOINT_ID_HEAD );
	AddJoint( NewSkeleton, JointList, SKELTRACK_JOINT_ID_LEFT_HAND );
	AddJoint( NewSkeleton, JointList, SKELTRACK_JOINT_ID_RIGHT_HAND );
	AddJoint( NewSkeleton, JointList, SKELTRACK_JOINT_ID_LEFT_SHOULDER );
	AddJoint( NewSkeleton, JointList, SKELTRACK_JOINT_ID_RIGHT_SHOULDER );
	AddJoint( NewSkeleton, JointList, SKELTRACK_JOINT_ID_LEFT_ELBOW );
	AddJoint( NewSkeleton, JointList, SKELTRACK_JOINT_ID_RIGHT_ELBOW );

	skeltrack_joint_list_free( JointList );

	OnNewSkeleton( NewSkeleton );
}

