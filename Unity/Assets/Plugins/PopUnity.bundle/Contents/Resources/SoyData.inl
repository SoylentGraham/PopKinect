//#include "SoyData.h"
#pragma once


template <> template<>
inline bool SoyData_Impl<std::string>::DecodeTo(SoyData_Impl<Array<char>>& Data) const 
{ 
	return false; 
}

template <> template<>
inline bool SoyData_Impl<std::string>::Encode(const SoyData_Impl<Array<char>>& Data) 
{
	return false;
}

template <> template<>
inline bool SoyData_Impl<std::string>::DecodeTo(SoyData_Impl<TYPE_Png>& Data) const
{
	return false; 
}

template <> template<>
inline bool SoyData_Impl<std::string>::Encode(const SoyData_Impl<TYPE_Png>& Data)
{ 
	return false; 
}


template<> template<>
inline bool SoyData_Impl<TYPE_DataUri>::CastFrom(const SoyData_Impl<std::string>& Data)
{
	this->mValue.mDataUri = Data.mValue;
	OnCastFrom(Data.GetFormat());
	return true;
}


template <> template<>
inline bool SoyData_Impl<TYPE_Base64>::DecodeTo(SoyData_Impl<TYPE_Png>& Data) const
{
	auto PngData = GetArrayBridge(Data.mValue.mData);
	if ( !mValue.Decode(PngData) )
		return false;
	Data.OnDecodedFrom(*this);
	return true;
}


template <> template<>
inline bool SoyData_Impl<TYPE_Base64>::Encode(const SoyData_Impl<Array<char>>& Data)
{
	auto DataBridge = GetArrayBridge(Data.mValue);
	if ( !mValue.Encode(DataBridge) )
		return false;
	OnEncode(Data.GetFormat());
	return true;
}

//	gr: maybe try and find a way to extract Array<char> automatically?
template <> template<>
inline bool SoyData_Impl<TYPE_Base64>::DecodeTo(SoyData_Impl<Array<char>>& Data) const
{
	auto DataBridge = GetArrayBridge(Data.mValue);
	if ( !mValue.Decode(DataBridge) )
		return false;
	Data.OnDecodedFrom(*this);
	return true;
}


//	gr: maybe try and find a way to extract Array<char> automatically?
template <> template<>
inline bool SoyData_Impl<TYPE_Base64>::Encode(const SoyData_Impl<TYPE_Png>& Data)
{
	auto PngData = GetArrayBridge(Data.mValue.mData);
	if ( !mValue.Encode(PngData) )
		return false;
	OnEncode(Data.GetFormat());
	return true;
}


template <> template<typename THAT>
inline bool SoyData_Impl<TYPE_Base64>::DecodeTo(SoyData_Impl<THAT>& Data) const
{
	if ( !mValue.Decode(Data.mValue) )
		return false;
	Data.OnDecodedFrom(*this);
	return true;
}


template <> template<typename THAT>
inline bool SoyData_Impl<TYPE_Base64>::Encode(const SoyData_Impl<THAT>& Data)
{
	if ( !mValue.Encode(Data.mValue) )
		return false;
	OnEncode(Data.GetFormat());
	return true;
}


template <> template<>
inline bool SoyData_Impl<std::string>::CastFrom(const SoyData_Impl<TYPE_File>& Data)
{
	mValue = Data.mValue.GetFilename();
	OnCastFrom(Data.GetFormat());
	return true;
}


//	cast from is basically the same as encode
template <> template<>
inline bool SoyData_Impl<std::string>::CastFrom(const SoyData_Impl<TYPE_DataUri>& Data)
{
	mValue = Data.mValue.mDataUri;
	OnCastFrom(Data.GetFormat());
	return true;
}

template <> template<>
inline bool SoyData_Impl<std::string>::CastFrom(const SoyData_Impl<TYPE_MemFile>& Data)
{
	mValue = Data.mValue.GetFilenameAndSize();
	OnCastFrom(Data.GetFormat());
	return true;
}




//	cast from is basically the same as encode
template <> template<>
inline bool SoyData_Impl<std::string>::CastFrom(const SoyData_Impl<float>& Data)
{
	std::stringstream s;
	s << Data.mValue;
	if ( s.fail() )
		return false;
	mValue = s.str();
	OnCastFrom(Data.GetFormat());
	return true;
}


/*	gr: don't know why this fails on many cases when Encode doesn't....
//	cast from is basically the same as encode
template <> template<typename THAT>
inline bool SoyData_Impl<std::string>::CastFrom(const SoyData_Impl<THAT>& Data)
{
std::stringstream s;
s << Data.mValue;
if ( s.fail() )
return false;
mValue = s.str();
OnCastFrom( Data.GetFormat() );
return true;
}
*/



//	cast from is basically the same as encode
template <> template<>
inline bool SoyData_Impl<std::string>::CastFrom(const SoyData_Impl<int>& Data)
{
	std::stringstream s;
	s << Data.mValue;
	if ( s.fail() )
		return false;
	mValue = s.str();
	OnCastFrom(Data.GetFormat());
	return true;
}




//	gr: on windows, this is not being caught. Overloading istream>> instead
template<> template<>
inline bool SoyData_Impl<std::string>::DecodeTo(SoyData_Impl<SoyPixels>& Data) const
{
	return false;
}

template <> template<>
inline bool SoyData_Impl<std::string>::Encode(const SoyData_Impl<SoyPixels>& Data)
{
	return false;
}



template <> template<>
inline bool SoyData_Impl<Array<char>>::DecodeTo(SoyData_Impl<std::string>& Data) const
{
	auto Array = GetArrayBridge(this->mValue);
	Data.mValue = Soy::ArrayToString(Array);
	Data.OnDecodedFrom(*this);
	return true;
}

template <> template<>
inline bool SoyData_Impl<Array<char>>::Encode(const SoyData_Impl<std::string>& Data)
{
	auto Array = GetArrayBridge(this->mValue);
	Soy::StringToArray(Data.mValue, Array);
	OnEncode(Data.GetFormat());
	return true;
}

template <> template<>
inline bool SoyData_Impl<Array<char>>::Encode(const SoyData_Impl<json::Object>& Data)
{
	auto Array = GetArrayBridge(this->mValue);

	//	gr: maybe there's a proper function/method for this... convert to string
	std::stringstream ss;
	json::Writer::Write(Data.mValue, ss);
	Soy::StringToArray(ss.str(), Array);

	OnEncode(Data.GetFormat());
	return true;
}

template <> template<>
inline bool SoyData_Impl<Array<char>>::Encode(const SoyData_Impl<TYPE_Png>& Data)
{
	auto Array = GetArrayBridge(this->mValue);
	Array.Copy(Data.mValue.mData);
	OnEncode(Data.GetFormat());
	return true;
}


template <> template<>
inline bool SoyData_Impl<TYPE_MemFile>::DecodeTo(SoyData_Impl<std::string>& Data) const
{
	//	todo: decode to array, then to pixels
	Soy_AssertTodo();
	return false;
	//	Data.OnDecodedFrom( *this );
	//	return true;
}

bool MemFile_DecodeTo_Binary(const TYPE_MemFile& MemFile, Array<char>& Binary);
template <> template<>
inline bool SoyData_Impl<TYPE_MemFile>::DecodeTo(SoyData_Impl<Array<char>>& Data) const
{
	if ( !MemFile_DecodeTo_Binary(this->mValue, Data.mValue) )
		return false;

	Data.OnDecodedFrom(*this);
	return true;
}

template <> template<>
inline bool SoyData_Impl<std::string>::DecodeTo(SoyData_Impl<TYPE_MemFile>& Data) const
{
	if ( !Data.mValue.SetFilenameAndSize(this->mValue) )
		return false;
	Data.OnDecodedFrom(*this);
	return true;
}

template <> template<>
inline bool SoyData_Impl<Array<char>>::DecodeTo(SoyData_Impl<TYPE_MemFile>& Data) const
{
	Data.mValue.SetFilenameAndSize(Soy::ArrayToString(GetArrayBridge(mValue)));
	//	data should contain a filename...
	if ( !Data.mValue.HasValidFilename() )
		return false;
	Data.OnDecodedFrom(*this);
	return true;
}


template<> template<>
inline bool SoyData_Impl<TYPE_MemFile>::CastFrom(const SoyData_Impl<std::string>& Data)
{
	if ( !this->mValue.SetFilenameAndSize(Data.mValue) )
		return false;

	OnCastFrom(Data.GetFormat());
	return true;
}

template<> template<>
inline bool SoyData_Impl<Array<char>>::CastFrom(const SoyData_Impl<TYPE_MemFile>& Data)
{
	//	data should contain a filename...
	if ( !Data.mValue.HasValidFilename() )
		return false;
	Soy::StringToArray(Data.mValue.GetFilenameAndSize(), GetArrayBridge(mValue));
	OnCastFrom(Data.GetFormat());
	return true;
}


template <> template<>
inline bool SoyData_Impl<TYPE_File>::DecodeTo(SoyData_Impl<SoyPixels>& Data) const
{
	if ( !mValue.IsValid() )
		return false;

	auto& Pixels = Data.mValue;
	auto& File = this->mValue;

	//	gr: memory mapped file here would be nice!
	std::stringstream Error;
	Array<char> FileData;
	if ( !Soy::FileToArray(GetArrayBridge(FileData), File.GetFilename(), Error) )
	{
		std::Debug << "Failed to load file: " << File.GetFilename() << ": " << Error.str() << std::endl;
		return false;
	}

	if ( !Pixels.SetRawSoyPixels(GetArrayBridge(FileData)) )
		return false;

	Data.OnDecodedFrom(*this);
	return true;
}

//	same as SoyData_Impl<TYPE_File>::DecodeTo(SoyData_Impl<SoyPixels>& Data)
template<> template<>
inline bool SoyData_Impl<SoyPixels>::CastFrom(const SoyData_Impl<TYPE_File>& Data)
{
	if ( !Data.mValue.IsValid() )
		return false;

	auto& Pixels = this->mValue;
	auto& File = Data.mValue;

	//	gr: memory mapped file here would be nice!
	std::stringstream Error;
	Array<char> FileData;
	if ( !Soy::FileToArray(GetArrayBridge(FileData), File.GetFilename(), Error) )
	{
		std::Debug << "Failed to load file: " << File.GetFilename() << ": " << Error.str() << std::endl;
		return false;
	}

	if ( !Pixels.SetRawSoyPixels(GetArrayBridge(FileData)) )
		return false;

	this->OnCastFrom(Data.GetFormat());
	return true;
}


template <> template<>
inline bool SoyData_Impl<TYPE_File>::DecodeTo(SoyData_Impl<std::string>& Data) const
{
	//	todo: decode to array, then to pixels
	Soy_AssertTodo();
	return false;
	//	Data.OnDecodedFrom( *this );
	//	return true;
}

template <> template<>
inline bool SoyData_Impl<TYPE_File>::DecodeTo(SoyData_Impl<Array<char>>& Data) const
{
	std::stringstream Error;
	if ( !Soy::FileToArray(GetArrayBridge(Data.mValue), this->mValue.GetFilename(), Error) )
	{
		std::Debug << "Failed to decode File to binary; " << Error.str() << std::endl;
		return false;
	}

	Data.OnDecodedFrom(*this);
	return true;
}

template <> template<>
inline bool SoyData_Impl<std::string>::DecodeTo(SoyData_Impl<TYPE_File>& Data) const
{
	if ( !Data.mValue.SetFilename(this->mValue) )
		return false;

	Data.OnDecodedFrom(*this);
	return true;
}

template <> template<>
inline bool SoyData_Impl<Array<char>>::DecodeTo(SoyData_Impl<TYPE_File>& Data) const
{
	if ( !Data.mValue.SetFilename(Soy::ArrayToString(GetArrayBridge(mValue))) )
		return false;
	//	data should contain a filename...
	if ( !Data.mValue.HasValidFilename() )
		return false;
	Data.OnDecodedFrom(*this);
	return true;
}


template<> template<>
inline bool SoyData_Impl<TYPE_File>::CastFrom(const SoyData_Impl<std::string>& Data)
{
	if ( !this->mValue.SetFilename(Data.mValue) )
		return false;

	OnCastFrom(Data.GetFormat());
	return true;
}

template<> template<>
inline bool SoyData_Impl<Array<char>>::CastFrom(const SoyData_Impl<TYPE_File>& Data)
{
	//	data should contain a filename...
	if ( !Data.mValue.HasValidFilename() )
		return false;
	Soy::StringToArray(Data.mValue.GetFilename(), GetArrayBridge(mValue));
	OnCastFrom(Data.GetFormat());
	return true;
}


bool Png_CastFrom_File(TYPE_Png& Png, const TYPE_File& File);
template<> template<>
inline bool SoyData_Impl<TYPE_Png>::CastFrom(const SoyData_Impl<TYPE_File>& Data)
{
	if ( !Png_CastFrom_File(this->mValue, Data.mValue) )
		return false;
	this->OnCastFrom(Data);
	return true;
}

bool File_DecodeTo_Png(const TYPE_File& File, TYPE_Png& Png);
template<> template<>
inline bool SoyData_Impl<TYPE_File>::DecodeTo(SoyData_Impl<TYPE_Png>& Data) const
{
	if ( !File_DecodeTo_Png(this->mValue, Data.mValue) )
		return false;

	Data.OnDecodedFrom(*this);
	return true;
}




//	pixels -> Data
template <> template<>
inline bool SoyData_Impl<Array<char>>::Encode(const SoyData_Impl<SoyPixels>& Data)
{
	if ( !Data.mValue.GetRawSoyPixels(GetArrayBridge(this->mValue)) )
		return false;
	OnEncode(Data.GetFormat());
	return true;
}


template <> template<>
inline bool SoyData_Impl<Array<char>>::DecodeTo(SoyData_Impl<SoyPixels>& Data) const
{
	if ( !Data.mValue.SetRawSoyPixels(GetArrayBridge(this->mValue)) )
		return false;
	Data.OnDecodedFrom(*this);
	return true;
}


template <> template<>
inline bool SoyData_Impl<std::string>::Encode(const SoyData_Impl<json::Object>& Data)
{
	std::stringstream ss;
	json::Writer::Write(Data.mValue, ss);
	mValue = ss.str();
	OnEncode(Data.GetFormat());
	return true;
}

template <> template<>
inline bool SoyData_Impl<std::string>::DecodeTo<json::Object>(SoyData_Impl<json::Object>& Data) const
{
	std::stringstream ss(mValue);
	if ( !Soy::StringToJson(ss, Data.mValue) )
		return false;

	Data.OnDecodedFrom(*this);
	return true;
}


template <> template<>
inline bool SoyData_Impl<std::string>::DecodeTo(SoyData_Impl<TYPE_Base64>& Data) const
{
	auto DataBridge = GetArrayBridge(Data.mValue.mData64);
	Soy::StringToArray(this->mValue, DataBridge);
	Data.OnDecodedFrom(*this);
	return true;
}

template <> template<>
inline bool SoyData_Impl<std::string>::Encode(const SoyData_Impl<TYPE_Base64>& Data)
{
	auto DataBridge = GetArrayBridge(Data.mValue.mData64);
	this->mValue = Soy::ArrayToString(DataBridge);
	OnEncode(Data.GetFormat());
	return true;
}

template <> template<>
inline bool SoyData_Impl<std::string>::DecodeTo(SoyData_Impl<TYPE_DataUri>& Data) const
{
	Data.mValue.mDataUri = this->mValue;
	Data.OnDecodedFrom(*this);
	return true;
}

template <> template<>
inline bool SoyData_Impl<std::string>::Encode(const SoyData_Impl<TYPE_DataUri>& Data)
{
	assert(false);
	return false;
}

template <> template<>
inline bool SoyData_Impl<std::string>::Encode(const SoyData_Impl<TYPE_MemFile>& Data)
{
	this->mValue = Data.mValue.GetFilenameAndSize();
	OnEncode(Data.GetFormat());
	return true;
}

template <> template<>
inline bool SoyData_Impl<std::string>::Encode(const SoyData_Impl<TYPE_File>& Data)
{
	this->mValue = Data.mValue.GetFilename();
	OnEncode(Data.GetFormat());
	return true;
}


bool MemFile_DecodeTo_Pixels(const TYPE_MemFile& MemFile, SoyPixels& Pixels);
template <> template<>
inline bool SoyData_Impl<TYPE_MemFile>::DecodeTo(SoyData_Impl<SoyPixels>& Data) const
{
	if ( !MemFile_DecodeTo_Pixels(mValue, Data.mValue) )
		return false;
	Data.OnDecodedFrom(*this);
	return true;
}

bool Pixels_CastFrom_MemFile(const TYPE_MemFile& MemFile, SoyPixels& Pixels);
template<> template<>
inline bool SoyData_Impl<SoyPixels>::CastFrom(const SoyData_Impl<TYPE_MemFile>& Data)
{
	if ( !Pixels_CastFrom_MemFile(Data.mValue, mValue) )
		return false;
	this->OnCastFrom(Data.GetFormat());
	return true;
}


//	soypixels -> png data
template <> template<>
inline bool SoyData_Impl<TYPE_Png>::Encode(const SoyData_Impl<SoyPixels>& Data)
{
	auto& Pixels = Data.mValue;
	auto PngData = GetArrayBridge(this->mValue.mData);

	if ( !Pixels.GetPng(PngData) )
		return false;
	OnEncode(Data.GetFormat());
	return true;
}

template <> template<>
inline bool SoyData_Impl<SoyPixels>::DecodeTo(SoyData_Impl<TYPE_Png>& Data) const
{
	auto& Pixels = this->mValue;
	auto PngData = GetArrayBridge(Data.mValue.mData);

	if ( !Pixels.GetPng(PngData) )
		return false;
	Data.OnDecodedFrom(*this);
	return true;
}

template <> template<>
inline bool SoyData_Impl<TYPE_Png>::DecodeTo(SoyData_Impl<SoyPixels>& Data) const
{
	auto& Pixels = Data.mValue;
	auto PngData = GetArrayBridge(this->mValue.mData);

	//	shit we havent encapsulated error reporting...
	std::stringstream Error;
	if ( !Pixels.SetPng(PngData, Error) )
	{
		std::Debug << "Failed to decode PNG -> Pixels: " << Error.str() << std::endl;
		return false;
	}
	Data.OnDecodedFrom(*this);
	return true;
}


template<> template<>
inline bool SoyData_Impl<TYPE_DataUri>::Encode(const SoyData_Impl<TYPE_Base64>& Data)
{
	auto& DataUri = this->mValue;
	auto& Data64 = Data.mValue;

	//	need a type->mime convertor!
	std::string MimeType;
	auto Format = Data.GetFormat();
	if ( Format.mContainers.Find(Soy::GetTypeName<TYPE_Png>()) )
		MimeType = "image/png";

	if ( MimeType.empty() )
	{
		std::Debug << "Don't know mime type of " << Format << std::endl;
		return false;
	}

	static bool DoUtf8Check = false;

	if ( DoUtf8Check && !Soy::Assert(Soy::IsUtf8String(MimeType), "Paramter is not utf8 safe") )
		return false;

	std::stringstream String;
	String << "data:" << MimeType << ";" << "base64,";
	Data64.GetString(String);
	DataUri.mDataUri = String.str();

	if ( DoUtf8Check && !Soy::Assert(Soy::IsUtf8String(DataUri.mDataUri), "Made non utf8 safe data uri") )
	{
		//	gr: corrupted string datauri here now!
		return false;
	}

	OnEncode(Data.GetFormat());
	return true;
}


bool DataUri_DecodeTo_Base64(const TYPE_DataUri& DataUri, TYPE_Base64& Data64);
template<> template<>
inline bool SoyData_Impl<TYPE_DataUri>::DecodeTo(SoyData_Impl<TYPE_Base64>& Data) const
{
	if ( !DataUri_DecodeTo_Base64( this->mValue, Data.mValue ) )
		return false;
	Data.OnDecodedFrom(*this);
	return true;
}


template <> template<typename THAT>
inline bool SoyData_Impl<std::string>::Encode(const SoyData_Impl<THAT>& Data)
{
	std::stringstream s;
	s << Data.mValue;
	if ( s.fail() )
		return false;
	mValue = s.str();
	OnEncode(Data.GetFormat());
	return true;
}

template<> template<typename THAT>
inline bool SoyData_Impl<std::string>::DecodeTo(SoyData_Impl<THAT>& Data) const
{
	if ( !Soy::StringToType(Data.mValue, mValue) )
		return false;
	Data.OnDecodedFrom(*this);
	return true;
}
