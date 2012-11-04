/**
 * OpenAL cross platform audio library
 * Copyright (C) 2010 by Chris Robinson
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */

#include "config.h"

#include <stdlib.h>
#include <jni.h>
#include <pthread.h>
#include "alMain.h"
#include "AL/al.h"
#include "AL/alc.h"
#include "klaatu_api.h"

static const ALCchar android_device[] = "Android Default";

static ALCboolean android_open_playback(ALCdevice *device, const ALCchar *deviceName)
{
    if (!deviceName)
    {
        deviceName = android_device;
    }
    else if (strcmp(deviceName, android_device) != 0)
    {
        return ALC_FALSE;
    }

    device->szDeviceName = strdup(deviceName);
    device->ExtraData = NULL;
    return ALC_TRUE;
}

static void android_close_playback(ALCdevice *device)
{
    KlaatuAudioDelete(device->ExtraData);
    device->ExtraData = NULL;
}

static ALCboolean android_reset_playback(ALCdevice *device)
{

    if (aluChannelsFromFormat(device->Format) >= 2)
    {
        device->Format = aluBytesFromFormat(device->Format) >= 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_STEREO8;
    }
    else
    {
        device->Format = aluBytesFromFormat(device->Format) >= 2 ? AL_FORMAT_MONO16 : AL_FORMAT_MONO8;
    }

    SetDefaultChannelOrder(device);

    device->ExtraData = KlaatuAudioOpen(device, device->Frequency, aluBytesFromFormat(device->Format),
        aluChannelsFromFormat(device->Format), aluFrameSizeFromFormat(device->Format),
        device->NumUpdates, (getdata_t) aluMixData);

    return ALC_TRUE;
}

static void android_stop_playback(ALCdevice *device)
{
    KlaatuAudioStop(device->ExtraData);
}

static ALCboolean android_open_capture(ALCdevice *pDevice, const ALCchar *deviceName)
{
    (void)pDevice;
    (void)deviceName;
    return ALC_FALSE;
}

static void android_close_capture(ALCdevice *pDevice)
{
    (void)pDevice;
}

static void android_start_capture(ALCdevice *pDevice)
{
    (void)pDevice;
}

static void android_stop_capture(ALCdevice *pDevice)
{
    (void)pDevice;
}

static void android_capture_samples(ALCdevice *pDevice, ALCvoid *pBuffer, ALCuint lSamples)
{
    (void)pDevice;
    (void)pBuffer;
    (void)lSamples;
}

static ALCuint android_available_samples(ALCdevice *pDevice)
{
    (void)pDevice;
    return 0;
}

static const BackendFuncs android_funcs = {
    android_open_playback,
    android_close_playback,
    android_reset_playback,
    android_stop_playback,
    android_open_capture,
    android_close_capture,
    android_start_capture,
    android_stop_capture,
    android_capture_samples,
    android_available_samples
};

void alc_android_init(BackendFuncs *func_list)
{
    *func_list = android_funcs;
}

void alc_android_deinit(void)
{
    /* release cached AudioTrack class */
}

void alc_android_probe(int type)
{
    if (type == DEVICE_PROBE)
    {
        AppendDeviceList(android_device);
    }
    else if (type == ALL_DEVICE_PROBE)
    {
        AppendAllDeviceList(android_device);
    }
}

void console_print( const char *str, ... )
{
    char tmp[ 1024 ] = {""};
    va_list ap;
    va_start( ap, str );
    vsprintf( tmp, str, ap );
    va_end( ap );
    __android_log_print( ANDROID_LOG_INFO, "", "%s", tmp );
}
