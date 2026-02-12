
/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BluetoothComponent: внешняя компонента для мобильной платформы 1С (Android).
// Реализует JNI-мост и методы печати по Bluetooth и получения списка устройств.
// Содержит нативную (C++) и Java части, предназначенные для интеграции с 1С.
//
// Copyright: BULKkA
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../include/AddInDefBase.h"
#include "../include/IAndroidComponentHelper.h"
#include "../jni/jnienv.h"
#include "../include/IMemoryManager.h"
#include <string>

class MainApp
{
private:

	jclass cc;
	jobject obj;
	std::wstring jstring2wstring(JNIEnv* jenv, jstring aStr);

public:

	MainApp();
	~MainApp();

	void setCC(jclass _cc);
	void setOBJ(jobject _obj);

	void Initialize(IAddInDefBaseEx*);

	void bluetoothPrint(const std::wstring& address, const std::wstring& data);
	std::wstring getPairedDevices(bool onlyPrinters);
};