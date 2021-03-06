﻿using UnityEngine;
using System.Collections;
using System.Collections.Generic;
using System;
using System.Runtime.InteropServices;


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
		if ( Interface.sError != System.IntPtr.Zero )
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

	public bool GetParam(string Param,Texture2D texture,SoyPixelsFormat Format=SoyPixelsFormat.Invalid)
	{
		int FormatInt = Convert.ToInt32 (Format);
		return PopUnity.GetJobParam_texture( ref mInterface, Param, texture.GetNativeTextureID(), FormatInt );
	}

	public bool GetParamPixelsWidthHeight(string Param,out int Width,out int Height)
	{
		//	gr: to remove warning
		Width = 0;
		Height = 0;
		return PopUnity.GetJobParam_PixelsWidthHeight( ref mInterface, Param, ref Width, ref Height );
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
	public static extern bool GetJobParam_texture(ref TJobInterface JobInterface,string Param,int Texture,int ConvertToFormat);
	
	[DllImport("PopUnity", CallingConvention = CallingConvention.Cdecl)]
	public static extern bool GetJobParam_PixelsWidthHeight(ref TJobInterface JobInterface,string Param,ref int Width,ref int Height);
	


	[DllImport("PopUnity")]
	private static extern void Cleanup ();

	static private DebugLogDelegate	mDebugLogDelegate = new DebugLogDelegate( Log );
	static private OnJobDelegate	mOnJobDelegate = new OnJobDelegate( OnJob );

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
			Cleanup();
#endif
	}

	static void Log(string str)
	{
		UnityEngine.Debug.Log("PopUnity: " + str);
	}

	static void OnJob(ref TJobInterface JobInterface)
	{
		//	turn into the more clever c# class
		PopJob Job = new PopJob( JobInterface );
		//UnityEngine.Debug.Log ("job! " + Job.Command );
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

	static public void Update()
	{
		FlushDebug (Marshal.GetFunctionPointerForDelegate (mDebugLogDelegate));

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


