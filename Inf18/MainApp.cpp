
/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Examples for the report "Making external components for 1C mobile platform for Android""
// at the conference INFOSTART 2018 EVENT EDUCATION https://event.infostart.ru/2018/
//
// Sample 1: Delay in code
// Sample 2: Getting device information
// Sample 3: Device blocking: receiving external event about changing of sceen
//
// Copyright: Igor Kisil 2018
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

#include <wchar.h>
#include <thread>
#include <cstdint>
#include "MainApp.h"
#include "ConversionWchar.h"

namespace
{
	std::u16string WStringToUtf16(const std::wstring& value)
	{
		std::u16string out;
		out.reserve(value.size());
		for (wchar_t wc : value)
		{
			uint32_t cp = static_cast<uint32_t>(wc);
			if (cp <= 0xFFFF)
			{
				out.push_back(static_cast<char16_t>(cp));
			}
			else if (cp <= 0x10FFFF)
			{
				cp -= 0x10000;
				out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
				out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
			}
		}
		return out;
	}

	std::wstring Utf16ToWString(const jchar* data, jsize len)
	{
		std::wstring out;
		out.reserve(static_cast<size_t>(len));
		for (jsize i = 0; i < len; ++i)
		{
			uint32_t cp = data[i];
			if (cp >= 0xD800 && cp <= 0xDBFF && (i + 1) < len)
			{
				uint32_t low = data[i + 1];
				if (low >= 0xDC00 && low <= 0xDFFF)
				{
					cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
					++i;
				}
			}
			out.push_back(static_cast<wchar_t>(cp));
		}
		return out;
	}

	jstring ToJString(JNIEnv* env, const std::wstring& value)
	{
		std::u16string utf16 = WStringToUtf16(value);
		return env->NewString(reinterpret_cast<const jchar*>(utf16.data()), static_cast<jsize>(utf16.size()));
	}
}

MainApp::MainApp() : cc(nullptr), obj(nullptr)
{
}

MainApp::~MainApp()
{
	Shutdown();
}

void MainApp::Initialize(IAddInDefBaseEx* cnn)
{
	std::lock_guard<std::mutex> lock(jniMutex);
	if (!obj)
	{
		trace("MainApp::Initialize start");
		IAndroidComponentHelper* helper = (IAndroidComponentHelper*)cnn->GetInterface(eIAndroidComponentHelper);
		if (helper)
		{
			WCHAR_T* className = nullptr;
			convToShortWchar(&className, L"org.printtech.printaddin.PrintApp");
			jclass ccloc = helper->FindClass(className);
			delete[] className;
			className = nullptr;
			if (ccloc)
			{
				JNIEnv* env = getJniEnv();
				if (!env)
				{
					trace("MainApp::Initialize no JNIEnv");
					return;
				}
				cc = static_cast<jclass>(env->NewGlobalRef(ccloc));
				env->DeleteLocalRef(ccloc);
				jobject activity = helper->GetActivity();
				if (!activity || !cc)
				{
					trace("MainApp::Initialize missing activity or class");
					if (activity)
						env->DeleteLocalRef(activity);
					return;
				}
				// call of constructor for java class
				jmethodID methID = env->GetMethodID(cc, "<init>", "(Landroid/app/Activity;J)V");
				if (!methID)
				{
					trace("MainApp::Initialize ctor not found");
					env->DeleteLocalRef(activity);
					return;
				}
				jobject objloc = env->NewObject(cc, methID, activity, (jlong)cnn);
				if (!objloc)
				{
					trace("MainApp::Initialize object creation failed");
					env->DeleteLocalRef(activity);
					return;
				}
				obj = static_cast<jobject>(env->NewGlobalRef(objloc));
				env->DeleteLocalRef(objloc);
				methID = env->GetMethodID(cc, "show", "()V");
				if (obj && methID)
					env->CallVoidMethod(obj, methID);
				env->DeleteLocalRef(activity);
				trace("MainApp::Initialize done");
			}
			else
			{
				trace("MainApp::Initialize class not found");
			}
		}
	}
}

void MainApp::bluetoothPrint(const std::wstring& address, const std::wstring& data)
{
	std::lock_guard<std::mutex> lock(jniMutex);
	trace("MainApp::bluetoothPrint start, addrLen=%d, dataLen=%d", (int)address.length(), (int)data.length());
	if (obj && cc)
	{
		JNIEnv* env = getJniEnv();
		if (!env)
		{
			trace("MainApp::bluetoothPrint no JNIEnv");
			return;
		}
		jstring jAddress = ToJString(env, address);
		jstring jData = ToJString(env, data);
		jmethodID methID = env->GetMethodID(cc, "bluetoothPrint", "(Ljava/lang/String;Ljava/lang/String;)V");
		if (methID)
			env->CallVoidMethod(obj, methID, jAddress, jData);
		else
			trace("MainApp::bluetoothPrint method not found");
		env->DeleteLocalRef(jAddress);
		env->DeleteLocalRef(jData);
	}
	else
	{
		trace("MainApp::bluetoothPrint missing obj/cc");
	}
}

void MainApp::connectPrinter(const std::wstring& address)
{
	std::lock_guard<std::mutex> lock(jniMutex);
	trace("MainApp::connectPrinter start, addrLen=%d", (int)address.length());
	if (obj && cc)
	{
		JNIEnv* env = getJniEnv();
		if (!env)
		{
			trace("MainApp::connectPrinter no JNIEnv");
			return;
		}
		jstring jAddress = ToJString(env, address);
		jmethodID methID = env->GetMethodID(cc, "connectPrinter", "(Ljava/lang/String;)Z");
		if (methID)
			(void)env->CallBooleanMethod(obj, methID, jAddress);
		else
			trace("MainApp::connectPrinter method not found");
		env->DeleteLocalRef(jAddress);
	}
	else
	{
		trace("MainApp::connectPrinter missing obj/cc");
	}
}

void MainApp::disconnectPrinter()
{
	std::lock_guard<std::mutex> lock(jniMutex);
	trace("MainApp::disconnectPrinter start");
	if (obj && cc)
	{
		JNIEnv* env = getJniEnv();
		if (!env)
		{
			trace("MainApp::disconnectPrinter no JNIEnv");
			return;
		}
		jmethodID methID = env->GetMethodID(cc, "disconnectPrinter", "()V");
		if (methID)
			env->CallVoidMethod(obj, methID);
		else
			trace("MainApp::disconnectPrinter method not found");
	}
	else
	{
		trace("MainApp::disconnectPrinter missing obj/cc");
	}
}

void MainApp::setIdleDisconnectMs(long ms)
{
	std::lock_guard<std::mutex> lock(jniMutex);
	trace("MainApp::setIdleDisconnectMs start, ms=%ld", ms);
	if (obj && cc)
	{
		JNIEnv* env = getJniEnv();
		if (!env)
		{
			trace("MainApp::setIdleDisconnectMs no JNIEnv");
			return;
		}
		jmethodID methID = env->GetMethodID(cc, "setIdleDisconnectMs", "(J)V");
		if (methID)
			env->CallVoidMethod(obj, methID, (jlong)ms);
		else
			trace("MainApp::setIdleDisconnectMs method not found");
	}
	else
	{
		trace("MainApp::setIdleDisconnectMs missing obj/cc");
	}
}

void MainApp::setUseInsecureSocket(bool value)
{
	std::lock_guard<std::mutex> lock(jniMutex);
	trace("MainApp::setUseInsecureSocket start, value=%d", value ? 1 : 0);
	if (obj && cc)
	{
		JNIEnv* env = getJniEnv();
		if (!env)
		{
			trace("MainApp::setUseInsecureSocket no JNIEnv");
			return;
		}
		jmethodID methID = env->GetMethodID(cc, "setUseInsecureSocket", "(Z)V");
		if (methID)
			env->CallVoidMethod(obj, methID, (jboolean)value);
		else
			trace("MainApp::setUseInsecureSocket method not found");
	}
	else
	{
		trace("MainApp::setUseInsecureSocket missing obj/cc");
	}
}

std::wstring MainApp::getPairedDevices(bool onlyPrinters)
{
	std::lock_guard<std::mutex> lock(jniMutex);
	trace("MainApp::getPairedDevices start, onlyPrinters=%d", onlyPrinters ? 1 : 0);
	if (!obj || !cc)
	{
		trace("MainApp::getPairedDevices missing obj/cc");
		return std::wstring();
	}

	JNIEnv* env = getJniEnv();
	if (!env)
	{
		trace("MainApp::getPairedDevices no JNIEnv");
		return std::wstring();
	}
	jmethodID methID = env->GetMethodID(cc, "getPairedDevices", "(Z)Ljava/lang/String;");
	if (!methID)
	{
		trace("MainApp::getPairedDevices method not found");
		return std::wstring();
	}

	jstring result = (jstring)env->CallObjectMethod(obj, methID, (jboolean)onlyPrinters);
	std::wstring list = jstring2wstring(env, result);
	if (result)
		env->DeleteLocalRef(result);
	trace("MainApp::getPairedDevices done, resultLen=%d", (int)list.length());
	return list;
}

void MainApp::sleep(long delay) {
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
}

void MainApp::startScreenWatch() const
{
	std::lock_guard<std::mutex> lock(jniMutex);
	if (obj && cc)
	{
		JNIEnv* env = getJniEnv();
		if (!env)
			return;
		jmethodID methID = env->GetMethodID(cc, "startScreenWatch", "()V");
		if (methID)
			env->CallVoidMethod(obj, methID);
	}
}

void MainApp::stopScreenWatch() const
{
	std::lock_guard<std::mutex> lock(jniMutex);
	if (!obj || !cc)
		return;

	JNIEnv* env = getJniEnv();
	if (!env)
		return;

	jmethodID methID = env->GetMethodID(cc, "stopScreenWatch", "()V");
	if (methID)
		env->CallVoidMethod(obj, methID);
}

static const wchar_t g_EventSource[] = L"org.ripreal.androidutils";
static const wchar_t g_EventName[] = L"LockChanged";
static WcharWrapper s_EventSource(g_EventSource);
static WcharWrapper s_EventName(g_EventName);

// name of function built according to Java native call
//
extern "C" JNIEXPORT void JNICALL Java_org_ripreal_androidutils_MainApp_OnLockChanged(JNIEnv* env, jclass jClass, jlong pObject) {
	IAddInDefBaseEx *pAddIn = (IAddInDefBaseEx *) pObject;
	if (pAddIn != nullptr) {
		pAddIn->ExternalEvent(s_EventSource, s_EventName, nullptr);
	}
}

std::wstring MainApp::jstring2wstring(JNIEnv* jenv, jstring aStr)
{
	std::wstring result;

	if (aStr)
	{
		const jchar* pCh = jenv->GetStringChars(aStr, 0);
		jsize len = jenv->GetStringLength(aStr);
		result = Utf16ToWString(pCh, len);
		jenv->ReleaseStringChars(aStr, pCh);
	}
	return result;
}

void MainApp::setCC(jclass _cc) {
	std::lock_guard<std::mutex> lock(jniMutex);
	cc = _cc;
}

void MainApp::setOBJ(jobject _obj) {
	std::lock_guard<std::mutex> lock(jniMutex);
	obj= _obj;
}

void MainApp::Shutdown()
{
	std::lock_guard<std::mutex> lock(jniMutex);
	if (!obj && !cc)
		return;

	JNIEnv* env = getJniEnv();
	if (!env)
	{
		obj = nullptr;
		cc = nullptr;
		return;
	}

	if (obj)
	{
		jmethodID methID = env->GetMethodID(cc, "stopScreenWatch", "()V");
		if (methID)
			env->CallVoidMethod(obj, methID);
	}

	if (obj)
		env->DeleteGlobalRef(obj);
	if (cc)
		env->DeleteGlobalRef(cc);

	obj = nullptr;
	cc = nullptr;
}
