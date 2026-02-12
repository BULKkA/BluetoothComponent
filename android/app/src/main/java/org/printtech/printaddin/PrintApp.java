
/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BluetoothComponent: внешняя компонента для мобильной платформы 1С (Android).
// Реализует JNI-мост и методы печати по Bluetooth и получения списка устройств.
// Содержит нативную (C++) и Java части, предназначенные для интеграции с 1С.
//
// Copyright: BULKkA
//
/////////////////////////////////////////////////////////////////////////////////////////////////////

package org.printtech.printaddin;

import android.app.Activity;
import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothClass;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

import java.io.IOException;
import java.io.OutputStream;
import java.util.Set;
import java.util.UUID;

public class PrintApp implements Runnable {

  private long m_V8Object; // 1C application context
  private Activity m_Activity; // custom activity of 1C:Enterprise

  public PrintApp(Activity activity, long v8Object)
  {
    m_Activity = activity;
    m_V8Object = v8Object;
  }

  public void run()
  {
    System.loadLibrary("org_printtech_printaddin");
  }

  public void show()
  {
    m_Activity.runOnUiThread(this);
  }

  public void bluetoothPrint(String address, String data)
  {
    if (!ensureBluetoothPermissions()) {
      return;
    }
    BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
    if (bluetoothAdapter == null) {
      Log.e("PrintApp", "Bluetooth not supported");
      return;
    }
    if (!bluetoothAdapter.isEnabled()) {
      Log.e("PrintApp", "Bluetooth not enabled");
      return;
    }
    BluetoothDevice device = bluetoothAdapter.getRemoteDevice(address);
    try {
      UUID uuid = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB"); // SPP UUID
      BluetoothSocket socket = device.createRfcommSocketToServiceRecord(uuid);
      socket.connect();
      OutputStream outputStream = socket.getOutputStream();
      outputStream.write(data.getBytes());
      outputStream.close();
      socket.close();
    } catch (IOException e) {
      Log.e("PrintApp", "Error printing", e);
    }
  }

  public String getPairedDevices(boolean onlyPrinters)
  {
    if (!ensureBluetoothPermissions()) {
      return "";
    }
    BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
    if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled()) {
      return "";
    }

    Set<BluetoothDevice> devices;
    try {
      devices = bluetoothAdapter.getBondedDevices();
    } catch (SecurityException ex) {
      Log.e("PrintApp", "Bluetooth permission missing", ex);
      return "";
    }
    if (devices == null || devices.isEmpty()) {
      return "";
    }

    StringBuilder builder = new StringBuilder();
    for (BluetoothDevice device : devices) {
      if (onlyPrinters && !isPrinter(device)) {
        continue;
      }
      String name;
      String address;
      try {
        name = device.getName() == null ? "" : device.getName();
        address = device.getAddress() == null ? "" : device.getAddress();
      } catch (SecurityException ex) {
        Log.e("PrintApp", "Bluetooth permission missing", ex);
        continue;
      }
      if (builder.length() > 0) {
        builder.append("\n");
      }
      builder.append(name).append("|").append(address);
    }

    return builder.toString();
  }

  private boolean ensureBluetoothPermissions()
  {
    if (Build.VERSION.SDK_INT < 31) {
      return true;
    }

    if (m_Activity.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
      return true;
    }

    m_Activity.requestPermissions(new String[]{Manifest.permission.BLUETOOTH_CONNECT}, 1001);
    return false;
  }

  private boolean isPrinter(BluetoothDevice device)
  {
    BluetoothClass btClass = device.getBluetoothClass();
    if (btClass == null) {
      return false;
    }

    int major = btClass.getMajorDeviceClass();
    return major == BluetoothClass.Device.Major.IMAGING;
  }

}
