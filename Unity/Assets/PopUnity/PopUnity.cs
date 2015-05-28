using UnityEngine;
using System.Collections;
using System.Collections.Generic;
using System;
using System.Runtime.InteropServices;

/*
[StructLayout(LayoutKind.Sequential, Size=228),Serializable]
public struct TFeatureMatch
{
	public UInt32	mCoord_x;
	public UInt32	mCoord_y;
	public float			mScore;

	[MarshalAsAttribute(UnmanagedType.ByValArray, SizeConst = 100)]
	public bool[]				mFeature;
	public UInt32			mFeatureSize;

	public UInt32			mScoreCoord_x;
	public UInt32			mScoreCoord_y;

	[MarshalAsAttribute(UnmanagedType.ByValArray, SizeConst = 100)]
	public bool[]			mSourceFeature;
	public UInt32			mSourceFeatureSize;
};
*/
[StructLayout(LayoutKind.Sequential, Size=12),Serializable]
public struct TFeatureMatch
{
	public UInt32	mCoord_x;
	public UInt32	mCoord_y;
	public float	mScore;
};

public struct TJobInterface
{
	public System.IntPtr	pJob;
	public System.IntPtr	sCommand;
	public System.IntPtr	sError;
	public UInt32			ParamCount;
	//public System.IntPtr[10]	sParamName;
};

public enum SoyPixelsFormat
{
	Invalid			= 0,
	Greyscale		= 1,
	GreyscaleAlpha	= 2,	//	png has this for 2 channel, so why not us!
	RGB				= 3,
	RGBA			= 4,
	
	//	non integer-based channel counts
	BGRA			= 5,
	BGR				= 6,
	KinectDepth		= 7,	//	16 bit, so "two channels". 13 bits of depth, 3 bits of user-index
	FreenectDepth10bit	= 8,	//	16 bit
	FreenectDepth11bit	= 9,	//	16 bit
	FreenectDepthmm	= 10,	//	16 bit
}

public class PopJob
{
	public TJobInterface	mInterface;
	public String			Command = "";
	public String			Error = null;

	public PopJob(TJobInterface Interface)
	{
		//	gr: strings aren't converting
		mInterface = Interface;
		Command = Marshal.PtrToStringAuto( Interface.sCommand );
		Error = Marshal.PtrToStringAuto( Interface.sError );
	}

	public int GetParam(string Param,int DefaultValue)
	{
		return PopUnity.GetJobParam_int( ref mInterface, Param, DefaultValue );
	}
	
	public float GetParam(string Param,float DefaultValue)
	{
		return PopUnity.GetJobParam_float( ref mInterface, Param, DefaultValue );
	}
	
	public string GetParam(string Param,string DefaultValue)
	{
		System.IntPtr stringPtr = PopUnity.GetJobParam_string( ref mInterface, Param, DefaultValue );
		return Marshal.PtrToStringAuto( stringPtr );
	}

	public bool GetParam(string Param,Texture2D texture,SoyPixelsFormat Format=SoyPixelsFormat.Invalid,bool Stretch=false)
	{
		int FormatInt = Convert.ToInt32 (Format);
		return PopUnity.GetJobParam_texture( ref mInterface, Param, texture.GetNativeTextureID(), FormatInt, Stretch );
	}

	public bool GetParamPixelsWidthHeight(string Param,out int Width,out int Height)
	{
		//	gr: to remove warning
		Width = 0;
		Height = 0;
		return PopUnity.GetJobParam_PixelsWidthHeight( ref mInterface, Param, ref Width, ref Height );
	}

	public bool GetParamArray(string Param,string ElementTypeName,ref List<TFeatureMatch> Array,int MaxSize)
	{
		TFeatureMatch[] Buffer = new TFeatureMatch[MaxSize];
		//	test for reading format
		Buffer [0].mCoord_x = 1;
		Buffer [0].mCoord_y = 2;
		Buffer [0].mScore = 3.0f;
		Buffer [1].mCoord_x = 4;
		Buffer [1].mCoord_y = 5;
		Buffer [1].mScore = 6.0f;

		IntPtr ptr = Marshal.AllocHGlobal( Marshal.SizeOf(Buffer[0]) * Buffer.Length);
		long LongPtr = ptr.ToInt64(); // Must work both on x86 and x64
		for (int I = 0; I < Buffer.Length; I++)
		{
			IntPtr RectPtr = new IntPtr(LongPtr);
			Marshal.StructureToPtr(Buffer[I], RectPtr, false); // You do not need to erase struct in this case
			LongPtr += Marshal.SizeOf(Buffer[I]);
		}

		int ElementsRead = PopUnity.GetJobParam_Array( ref mInterface, Param, ElementTypeName, ptr, Buffer.Length );
		if (ElementsRead < 0) {
			Array.Clear ();
			return false;
		}

		Array.Clear ();

		//	copy back
		LongPtr = ptr.ToInt64(); // Must work both on x86 and x64
		for (int i=0; i<Math.Min( Buffer.Length, ElementsRead); i++) {
			IntPtr pElement = new IntPtr (LongPtr);
			LongPtr += Marshal.SizeOf (Buffer [i]);
			Buffer[i] = (TFeatureMatch)Marshal.PtrToStructure (pElement, typeof(TFeatureMatch));
			Array.Add (Buffer [i]);
		}

		return true;
	}
}



public class PopUnity
{
	public delegate void TJobHandler(PopJob Job);
	private static Dictionary<string,TJobHandler> mJobHandlers = new Dictionary<string,TJobHandler>();

	public static void AssignJobHandler(string JobName,TJobHandler Delegate)
	{
		mJobHandlers[JobName] = Delegate;
	}

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void DebugLogDelegate(string str);
	
	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	private delegate void OnJobDelegate(ref TJobInterface Job);

	[DllImport("PopUnity", CallingConvention = CallingConvention.Cdecl)]
	public static extern UInt64 CreateChannel(string ChannelSpec);

	[DllImport("PopUnity", CallingConvention = CallingConvention.Cdecl)]
	public static extern bool SendJob(UInt64 ChannelRef,string Command);

	[DllImport("PopUnity")]
	public static extern void FlushDebug (System.IntPtr FunctionPtr);
	
	[DllImport("PopUnity")]
	public static extern bool PopJob (System.IntPtr FunctionPtr);

	[DllImport("PopUnity", CallingConvention = CallingConvention.Cdecl)]
	public static extern int GetJobParam_int(ref TJobInterface JobInterface,string Param,int DefaultValue);
	
	[DllImport("PopUnity", CallingConvention = CallingConvention.Cdecl)]
	public static extern float GetJobParam_float(ref TJobInterface JobInterface,string Param,float DefaultValue);
	
	[DllImport("PopUnity", CallingConvention = CallingConvention.Cdecl)]
	public static extern System.IntPtr GetJobParam_string(ref TJobInterface JobInterface,string Param,string DefaultValue);

	[DllImport("PopUnity", CallingConvention = CallingConvention.Cdecl)]
	public static extern bool GetJobParam_texture(ref TJobInterface JobInterface,string Param,int Texture,int ConvertToFormat,bool Stretch);
	
	[DllImport("PopUnity", CallingConvention = CallingConvention.Cdecl)]
	public static extern bool GetJobParam_PixelsWidthHeight(ref TJobInterface JobInterface,string Param,ref int Width,ref int Height);
	
	[DllImport("PopUnity", CallingConvention = CallingConvention.Cdecl)]
	public static extern int GetJobParam_Array(ref TJobInterface JobInterface,string Param,string ElementTypeName,System.IntPtr Array,int ArraySize);

	[DllImport("PopUnity")]
	private static extern void OnStopped ();

	static private DebugLogDelegate	mDebugLogDelegate = new DebugLogDelegate(Log);
	static private OnJobDelegate	mOnJobDelegate = new OnJobDelegate( OnJob );

	public delegate void TDebugDelegate(String Log);
	public static TDebugDelegate DebugDelegate;

	public PopUnity()
	{
#if UNITY_EDITOR
		UnityEditor.EditorApplication.playmodeStateChanged += OnAppStateChanged;
#endif
	}

	public void OnAppStateChanged()
	{
#if UNITY_EDITOR
		if (UnityEditor.EditorApplication.isPlayingOrWillChangePlaymode && UnityEditor.EditorApplication.isPlaying)
			OnStopped();
#endif
	}

	static void ConsoleDebugLog(String Log)
	{
		UnityEngine.Debug.Log (Log);
	}

	static void Log(string str)
	{
		if (DebugDelegate != null)
			DebugDelegate ("PopUnity: " + str);
	}

	static void OnJob(ref TJobInterface JobInterface)
	{
		//	turn into the more clever c# class
		PopJob Job = new PopJob( JobInterface );
		UnityEngine.Debug.Log ("job! " + Job.Command );
		if ( Job.Error != null )
			UnityEngine.Debug.Log ("(error: " + Job.Error);

		//	send job to handler
		try
		{
			TJobHandler Handler = mJobHandlers[Job.Command];
			Handler( Job );
		}
		catch ( KeyNotFoundException )
		{
			UnityEngine.Debug.Log ("Unhandled job " + Job.Command);
		}
	}

	static public void Start()
	{
		//	gr: only add the console log in editor mode. Can't see it in builds, and saves CPU time if we blindly flush messages by having no delegates
#if UNITY_EDITOR
		DebugDelegate += ConsoleDebugLog;
#endif
	}

	static public void Update()
	{
		//	if we have no listeners, do fast flush
		bool HasListeners = (DebugDelegate!=null) && (DebugDelegate.GetInvocationList().Length > 0);
		HasListeners &= (mDebugLogDelegate!=null) &&(mDebugLogDelegate.GetInvocationList().Length > 0);
		if ( HasListeners )
			FlushDebug (Marshal.GetFunctionPointerForDelegate (mDebugLogDelegate));
		else
			FlushDebug (System.IntPtr.Zero);

		//	pop all jobs
		bool More = PopJob (Marshal.GetFunctionPointerForDelegate (mOnJobDelegate));
		while ( More )
		{
			More = PopJob (Marshal.GetFunctionPointerForDelegate (mOnJobDelegate));
		}
	}
};
	

public class PopUnityChannel
{
	public UInt64	mChannel = 0;

	public PopUnityChannel(string Channel)
	{
		mChannel = PopUnity.CreateChannel(Channel);
	}

	public bool SendJob(string Command)
	{
		return PopUnity.SendJob (mChannel, Command);
	}
};


