using UnityEngine;
using System.Collections;
using System.Collections.Generic;
using System;
using System.Runtime.InteropServices;


public class Kinect : MonoBehaviour {

	public String	ServerAddress = "cli://localhost:7070";
	public String	Serial = "0";
	private PopUnityChannel	mChannel = null;
	public Texture2D mDepthTexture;
	public Texture2D mColourTexture;


	public Material MaterialForTexture;

	Texture2D	GetDepthTexture()
	{
		if (mDepthTexture == null) {
			int Width = 1024;
			int Height = 1024;
			mDepthTexture = new Texture2D (1024, 1024, TextureFormat.BGRA32, false);
		}
		return mDepthTexture;
	}
	
	Texture2D	GetColourTexture()
	{
		if (mColourTexture == null) {
			int Width = 1024;
			int Height = 1024;
			mColourTexture = new Texture2D (1024, 1024, TextureFormat.BGRA32, false);
		}
		return mColourTexture;
	}
	
	void Start()
	{
		PopUnity.AssignJobHandler("newframe", ((Job) => this.OnColour(Job)) );
		PopUnity.AssignJobHandler("newdepth", ((Job) => this.OnDepth (Job)));
	}

	void Update () {
		PopUnity.Update ();

		//	keep trying to connect
		if (mChannel == null) {
			mChannel = new PopUnityChannel (ServerAddress);
			mChannel.SendJob( "subscribenewframe serial=Depth" + Serial + " command=newdepth memfile=1" );
			mChannel.SendJob( "subscribenewframe serial=Video" + Serial + " command=newframe memfile=1" );
		}	
	}

	void OnDepth(PopJob Job)
	{
		Job.GetParam("default", GetDepthTexture(), SoyPixelsFormat.Greyscale );
	}
	
	void OnColour(PopJob Job)
	{
		Job.GetParam("default", GetColourTexture() );
	}





	Rect StepRect(Rect rect)
	{
		rect.y += rect.height + (rect.height*0.30f);
		return rect;
	}

	void OnGUI()
	{
		int Width = 200;
		int Height = 200;
		if (mDepthTexture != null) {
			GUI.DrawTexture ( new Rect( 0, 0, Width, Height ), mDepthTexture);
		}
		if (mColourTexture != null) {
			GUI.DrawTexture ( new Rect( Width, 0, Width, Height ), mColourTexture);
		}
	}

	void OnPostRender()
	{
		GL.IssuePluginEvent (0);
	}
}
