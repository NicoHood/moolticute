/******************************************************************************
 **  Copyright (c) Raoul Hecky. All Rights Reserved.
 **
 **  Calaos is free software; you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation; either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Calaos is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Foobar; if not, write to the Free Software
 **  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 **
 ******************************************************************************/
#include "MPManager.h"

#if defined(Q_OS_WIN)
#include "UsbMonitor_win.h"
#elif defined(Q_OS_LINUX)
#include "UsbMonitor_linux.h"
#elif defined(Q_OS_MAC)
#include "UsbMonitor_mac.h"
#endif

MPManager::MPManager():
    QObject(nullptr)
{
#if defined(Q_OS_WIN)
    connect(UsbMonitor_win::Instance(), SIGNAL(usbDeviceAdded()), this, SLOT(usbDeviceAdded()));
    connect(UsbMonitor_win::Instance(), SIGNAL(usbDeviceRemoved()), this, SLOT(usbDeviceRemoved()));
#elif defined(Q_OS_MAC)
    connect(UsbMonitor_mac::Instance(), SIGNAL(usbDeviceAdded()), this, SLOT(usbDeviceAdded()));
    connect(UsbMonitor_mac::Instance(), SIGNAL(usbDeviceRemoved()), this, SLOT(usbDeviceRemoved()));
#elif defined(Q_OS_LINUX)
    UsbMonitor_linux::Instance()->filterVendorId(MOOLTIPASS_VENDORID);
    UsbMonitor_linux::Instance()->filterProductId(MOOLTIPASS_PRODUCTID);
    UsbMonitor_linux::Instance()->start();
    connect(UsbMonitor_linux::Instance(), SIGNAL(usbDeviceAdded()), this, SLOT(usbDeviceAdded()));
    connect(UsbMonitor_linux::Instance(), SIGNAL(usbDeviceRemoved()), this, SLOT(usbDeviceRemoved()));
#endif

#if !defined(Q_OS_MAC)
    //This is not needed on osx, the list is updated at startup by UsbMonitor
    checkUsbDevices();
#endif
}

MPManager::~MPManager()
{
#if defined(Q_OS_LINUX)
    stop();
#endif
}

void MPManager::stop()
{
#if defined(Q_OS_LINUX)
    UsbMonitor_linux::Instance()->stop();
#endif
}

void MPManager::usbDeviceAdded()
{
    checkUsbDevices();
}

void MPManager::usbDeviceRemoved()
{
    checkUsbDevices();
}

MPDevice *MPManager::getDevice(int at)
{
    if (at < 0 || at >= devices.count())
        return nullptr;
    auto it = devices.begin();
    while (it != devices.end() && at > 0) { at--; it++; }
    return it.value();
}

QList<MPDevice *> MPManager::getDevices()
{
    QList<MPDevice *> devs;
    auto it = devices.begin();
    while (it != devices.end())
    {
        devs.append(it.value());
        it++;
    }
    return devs;
}

void MPManager::checkUsbDevices()
{
    // discover devices
    QList<MPPlatformDef> devlist;
    QList<QString> detectedDevs;

    qDebug() << "List usb devices...";

#if defined(Q_OS_WIN)
    devlist = MPDevice_win::enumerateDevices();
#elif defined(Q_OS_MAC)
    devlist = MPDevice_mac::enumerateDevices();
#elif defined(Q_OS_LINUX)
    devlist = MPDevice_linux::enumerateDevices();
#endif

    if (devlist.isEmpty())
    {
        //No USB devices found, means all MPs are gone disconnected
        auto it = devices.begin();
        while (it != devices.end())
        {
            emit mpDisconnected(it.value());
            delete it.value();
            it++;
        }
        devices.clear();
        return;
    }

    foreach (const MPPlatformDef &def, devlist)
    {
        //This is a new connected mooltipass
        if (!devices.contains(def.id))
        {
            MPDevice *device;

            //Create our platform device object
#if defined(Q_OS_WIN)
            device = new MPDevice_win(this, def);
#elif defined(Q_OS_MAC)
            device = new MPDevice_mac(this, def);
#elif defined(Q_OS_LINUX)
            device = new MPDevice_linux(this, def);
#endif

            devices[def.id] = device;
            emit mpConnected(device);
        }
        detectedDevs.append(def.id);
    }

    //Clear disconnected devices
    auto it = devices.begin();
    while (it != devices.end())
    {
        if (!detectedDevs.contains(it.key()))
        {
            emit mpDisconnected(it.value());
            delete it.value();
            devices.remove(it.key());
            it = devices.begin();
        }
        else
            it++;
    }
}
