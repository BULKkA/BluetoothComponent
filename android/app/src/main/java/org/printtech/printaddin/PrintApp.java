
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
import java.nio.charset.StandardCharsets;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

// SAMPLE 3

public class PrintApp implements Runnable {

  private long m_V8Object; // 1C application context
  private Activity m_Activity; // custom activity of 1C:Enterprise
  private final Object ioLock = new Object();
  private final ExecutorService ioExecutor = Executors.newSingleThreadExecutor();
  private final ScheduledExecutorService idleExecutor = Executors.newSingleThreadScheduledExecutor();
  private ScheduledFuture<?> idleCloseFuture;
  private BluetoothSocket socket;
  private OutputStream outputStream;
  private String connectedAddress;
  private static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");
  private static final long CONNECT_TIMEOUT_MS = 8000;
  private static final long IDLE_DISCONNECT_MS = 10000;
  private volatile long idleDisconnectMs = 0;
  private volatile boolean useInsecureSocket = true;

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
    Log.d("PrintApp", "bluetoothPrint called, address=" + address + ", dataLength=" + (data == null ? 0 : data.length()));
    if (!ensureBluetoothPermissions()) {
      Log.w("PrintApp", "Bluetooth permission missing, request issued");
      return;
    }
    byte[] payload = data == null ? new byte[0] : data.getBytes(StandardCharsets.UTF_8);
    enqueuePrint(address, payload);
  }

  public boolean connectPrinter(String address)
  {
    Log.d("PrintApp", "connectPrinter called, address=" + address);
    if (!ensureBluetoothPermissions()) {
      Log.w("PrintApp", "Bluetooth permission missing, request issued");
      return false;
    }
    Callable<Boolean> task = () -> {
      BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
      if (bluetoothAdapter == null) {
        Log.e("PrintApp", "Bluetooth not supported");
        return false;
      }
      if (!bluetoothAdapter.isEnabled()) {
        Log.e("PrintApp", "Bluetooth not enabled");
        return false;
      }
      return ensureConnected(bluetoothAdapter, address);
    };
    return getWithTimeout(ioExecutor.submit(task), CONNECT_TIMEOUT_MS + 1000, false);
  }

  public void disconnectPrinter()
  {
    Log.d("PrintApp", "disconnectPrinter called");
    Future<?> future = ioExecutor.submit(() -> {
      synchronized (ioLock) {
        closeConnectionLocked();
      }
    });
    getWithTimeout(future, 2000, null);
  }

  public void setIdleDisconnectMs(long ms)
  {
    if (ms < 0) {
      ms = 0;
    }
    idleDisconnectMs = ms;
    Log.d("PrintApp", "setIdleDisconnectMs=" + ms);
    synchronized (ioLock) {
      scheduleIdleDisconnectLocked();
    }
  }

  public void setUseInsecureSocket(boolean value)
  {
    useInsecureSocket = value;
    Log.d("PrintApp", "setUseInsecureSocket=" + value);
  }

  public String getPairedDevices(boolean onlyPrinters)
  {
    if (!ensureBluetoothPermissions()) {
      Log.w("PrintApp", "Bluetooth permission missing, request issued");
      return "";
    }
    BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
    if (bluetoothAdapter == null || !bluetoothAdapter.isEnabled()) {
      Log.w("PrintApp", "Bluetooth not available or disabled");
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

  private void enqueuePrint(String address, byte[] payload)
  {
    ioExecutor.execute(() -> {
      BluetoothAdapter bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
      if (bluetoothAdapter == null) {
        Log.e("PrintApp", "Bluetooth not supported");
        return;
      }
      if (!bluetoothAdapter.isEnabled()) {
        Log.e("PrintApp", "Bluetooth not enabled");
        return;
      }
      if (!ensureConnected(bluetoothAdapter, address)) {
        return;
      }
      if (!writeData(payload)) {
        Log.w("PrintApp", "Write failed, retrying after reconnect");
        if (ensureConnected(bluetoothAdapter, address) && writeData(payload)) {
          Log.i("PrintApp", "Print completed after reconnect");
        }
        return;
      }
      Log.i("PrintApp", "Print completed");
    });
  }

  private boolean ensureBluetoothPermissions()
  {
    if (Build.VERSION.SDK_INT < 31) {
      return true;
    }

    if (m_Activity.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
      Log.d("PrintApp", "Bluetooth permission granted");
      return true;
    }

    Log.i("PrintApp", "Requesting BLUETOOTH_CONNECT permission");
    m_Activity.requestPermissions(new String[]{Manifest.permission.BLUETOOTH_CONNECT}, 1001);
    return false;
  }

  private boolean ensureConnected(BluetoothAdapter bluetoothAdapter, String address)
  {
    if (address == null || address.isEmpty()) {
      Log.e("PrintApp", "Bluetooth address is empty");
      return false;
    }

    synchronized (ioLock) {
      if (socket != null && socket.isConnected() && address.equalsIgnoreCase(connectedAddress)) {
        return true;
      }

      closeConnectionLocked();
      try {
        boolean canScan = Build.VERSION.SDK_INT < 31
            || m_Activity.checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED;
        if (canScan) {
          if (bluetoothAdapter.isDiscovering()) {
            bluetoothAdapter.cancelDiscovery();
          }
        } else {
          Log.w("PrintApp", "BLUETOOTH_SCAN not granted, skip discovery checks");
        }
        BluetoothDevice device = bluetoothAdapter.getRemoteDevice(address);
        BluetoothSocket newSocket = createSocket(device);
        if (!connectWithTimeout(newSocket, CONNECT_TIMEOUT_MS)) {
          Log.e("PrintApp", "Bluetooth connect timeout");
          closeQuietly(newSocket);
          return false;
        }
        OutputStream newStream = newSocket.getOutputStream();
        socket = newSocket;
        outputStream = newStream;
        connectedAddress = address;
        scheduleIdleDisconnectLocked();
        Log.i("PrintApp", "Bluetooth connected to " + address);
        return true;
      } catch (IllegalArgumentException e) {
        Log.e("PrintApp", "Invalid bluetooth address: " + address, e);
      } catch (IOException e) {
        Log.e("PrintApp", "Bluetooth connect failed", e);
      }
      closeConnectionLocked();
      return false;
    }
  }

  private boolean writeData(byte[] payload)
  {
    synchronized (ioLock) {
      if (outputStream == null) {
        Log.e("PrintApp", "Output stream is null");
        return false;
      }
      try {
        outputStream.write(payload);
        outputStream.flush();
        scheduleIdleDisconnectLocked();
        return true;
      } catch (IOException e) {
        Log.e("PrintApp", "Error printing", e);
        closeConnectionLocked();
        return false;
      }
    }
  }

  private void closeConnectionLocked()
  {
    if (idleCloseFuture != null) {
      idleCloseFuture.cancel(false);
      idleCloseFuture = null;
    }
    if (outputStream != null) {
      try {
        outputStream.close();
      } catch (IOException ignored) {
        Log.w("PrintApp", "Error closing output stream", ignored);
      }
      outputStream = null;
    }
    if (socket != null) {
      try {
        socket.close();
      } catch (IOException ignored) {
        Log.w("PrintApp", "Error closing socket", ignored);
      }
      socket = null;
    }
    connectedAddress = null;
  }

  private void scheduleIdleDisconnectLocked()
  {
    if (idleDisconnectMs <= 0) {
      if (idleCloseFuture != null) {
        idleCloseFuture.cancel(false);
        idleCloseFuture = null;
      }
      return;
    }
    if (idleCloseFuture != null) {
      idleCloseFuture.cancel(false);
    }
    idleCloseFuture = idleExecutor.schedule(() -> {
      synchronized (ioLock) {
        closeConnectionLocked();
      }
    }, idleDisconnectMs, TimeUnit.MILLISECONDS);
  }

  private boolean connectWithTimeout(BluetoothSocket socketToConnect, long timeoutMs)
  {
    java.util.concurrent.FutureTask<Boolean> task = new java.util.concurrent.FutureTask<>(() -> {
      try {
        socketToConnect.connect();
        return true;
      } catch (IOException e) {
        Log.e("PrintApp", "Bluetooth connect failed", e);
        return false;
      }
    });
    Thread thread = new Thread(task, "PrintApp-Connect");
    thread.start();
    return getWithTimeout(task, timeoutMs, false);
  }

  private BluetoothSocket createSocket(BluetoothDevice device) throws IOException
  {
    if (useInsecureSocket && Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD_MR1) {
      return device.createInsecureRfcommSocketToServiceRecord(SPP_UUID);
    }
    return device.createRfcommSocketToServiceRecord(SPP_UUID);
  }

  private void closeQuietly(BluetoothSocket socketToClose)
  {
    try {
      socketToClose.close();
    } catch (IOException ignored) {
      Log.w("PrintApp", "Error closing socket", ignored);
    }
  }

  private <T> T getWithTimeout(Future<T> future, long timeoutMs, T fallback)
  {
    try {
      return future.get(timeoutMs, TimeUnit.MILLISECONDS);
    } catch (Exception ex) {
      Log.e("PrintApp", "Async task timeout", ex);
      return fallback;
    }
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
