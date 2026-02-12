
/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BluetoothComponent: внешняя компонента для мобильной платформы 1С (Android).
// Реализует JNI-мост и методы печати по Bluetooth и получения списка устройств.
// Содержит нативную (C++) и Java части, предназначенные для интеграции с 1С.
//
// Copyright: BULKkA
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

#include <wchar.h>
#include "MainApp.h"
#include "ConversionWchar.h"

MainApp::MainApp() : cc(nullptr), obj(nullptr)
{
}

MainApp::~MainApp()
{
	if (obj)
	{
		JNIEnv *env = getJniEnv();
		env->DeleteGlobalRef(obj);
		env->DeleteGlobalRef(cc);
	}
}

void MainApp::Initialize(IAddInDefBaseEx* cnn)
{
	if (!obj)
	{
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
				cc = static_cast<jclass>(env->NewGlobalRef(ccloc));
				env->DeleteLocalRef(ccloc);
				jobject activity = helper->GetActivity();
				// call of constructor for java class
				jmethodID methID = env->GetMethodID(cc, "<init>", "(Landroid/app/Activity;J)V");
				jobject objloc = env->NewObject(cc, methID, activity, (jlong)cnn);
				obj = static_cast<jobject>(env->NewGlobalRef(objloc));
				env->DeleteLocalRef(objloc);
				methID = env->GetMethodID(cc, "show", "()V");
				env->CallVoidMethod(obj, methID);
				env->DeleteLocalRef(activity);
			}
		}
	}
}

void MainApp::bluetoothPrint(const std::wstring& address, const std::wstring& data)
{
	if (obj)
	{
		JNIEnv* env = getJniEnv();
		jstring jAddress = env->NewString((const jchar*)address.c_str(), address.length());
		jstring jData = env->NewString((const jchar*)data.c_str(), data.length());
		jmethodID methID = env->GetMethodID(cc, "bluetoothPrint", "(Ljava/lang/String;Ljava/lang/String;)V");
		env->CallVoidMethod(obj, methID, jAddress, jData);
		env->DeleteLocalRef(jAddress);
		env->DeleteLocalRef(jData);
	}
}

std::wstring MainApp::getPairedDevices(bool onlyPrinters)
{
	if (!obj)
		return std::wstring();

	JNIEnv* env = getJniEnv();
	jmethodID methID = env->GetMethodID(cc, "getPairedDevices", "(Z)Ljava/lang/String;");
	if (!methID)
		return std::wstring();

	jstring result = (jstring)env->CallObjectMethod(obj, methID, (jboolean)onlyPrinters);
	std::wstring list = jstring2wstring(env, result);
	if (result)
		env->DeleteLocalRef(result);
	return list;
}
std::wstring MainApp::jstring2wstring(JNIEnv* jenv, jstring aStr)
{
	std::wstring result;

	if (aStr)
	{
		const jchar* pCh = jenv->GetStringChars(aStr, 0);
		jsize len = jenv->GetStringLength(aStr);
		const jchar* temp = pCh;
		while (len > 0)
		{
			result += *(temp++);
			--len;
		}
		jenv->ReleaseStringChars(aStr, pCh);
	}
	return result;
}

void MainApp::setCC(jclass _cc) {
    cc = _cc;
}

void MainApp::setOBJ(jobject _obj) {
    obj= _obj;
}
