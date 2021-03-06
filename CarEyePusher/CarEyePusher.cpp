/*
* CarEyePusher.cpp
*
* Author: Wgj
* Date: 2018-03-06 21:37
*
* 推流接口测试演示程序
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "..\API\CarEyePusherAPI.h"

#ifdef _WIN32
#include <windows.h>
const char *key = "y0E|9Dvd_|wd.Dw!W!W|WBw!W!Wcx~X!^AX!_!^|9Dvd_|wd.DwEWdwd9DYDwd_e,DwdYDw75";
#else
#include <pthread.h>
#include <ctype.h>
#include <sys/time.h>
const char *key = ")0FE_dWD9EXDYdXfvfWF0A,fvfv>^Bwfxa9fyf_E_dWD9EXDYdX|vDXD_d.dXD956";
#endif

using namespace std;
//#define TEST_NATIVE_FILE
#define TEST_1078_H264
//#define TEST_MULTI_CHN

char ServerIP[] = "120.76.235.109"; // CarEye服务器
const unsigned short ServerPort = 9300;
char TestFile[] = "./test.mp4";
char TestH264[] = "./test.264";

bool threadIsWork = false;
CarEye_MediaInfo MediaInfo;

#define ENUM_CHIP_TYPE_CASE(x)   case x: return(#x);

char* GetEnumString(CarEyePusherType aEnum)
{
	switch (aEnum)
	{
		ENUM_CHIP_TYPE_CASE(PUSHER_RTSP)
			ENUM_CHIP_TYPE_CASE(PUSHER_NATIVEFILE_RTSP)
			ENUM_CHIP_TYPE_CASE(PUSHER_RTMP)
			ENUM_CHIP_TYPE_CASE(PUSHER_NATIVEFILE_RTMP)
	}

	return "Already released";
}

/*
* Comments: 推流器状态更改事件
* Param :
* @Return int
*/
int CarEyePusher_StateChangedEvent(int channel, CarEyeStateType state, CarEyePusherType FrameType)
{
	switch (state)
	{
	case CAREYE_STATE_CONNECTING:
		printf("Chn[%d] %s Connecting...\n", channel, GetEnumString(FrameType));
		break;

	case CAREYE_STATE_CONNECTED:
		printf("Chn[%d] %s Connected\n", channel, GetEnumString(FrameType));
		break;

	case CAREYE_STATE_CONNECT_FAILED:
		printf("Chn[%d] %s Connect failed\n", channel, GetEnumString(FrameType));
		break;

	case CAREYE_STATE_CONNECT_ABORT:
		printf("Chn[%d] %s Connect abort\n", channel, GetEnumString(FrameType));
		break;

	case CAREYE_STATE_PUSHING:

		break;

	case CAREYE_STATE_DISCONNECTED:
		printf("Chn[%d] %s Disconnect.\n", channel, GetEnumString(FrameType));
		break;

	case CAREYE_STATE_FILE_FINISHED:
		printf("Chn[%d] %s Push native file finished.\n", channel, GetEnumString(FrameType));
		CarEye1078RTP_StopNativeFile(channel);
		break;

	default:
		break;
	}

	return 0;
}

// 推流测试线程
#ifdef _WIN32
DWORD WINAPI PushThreadEntry(LPVOID arg)
#else
void *PushThreadEntry(void *arg)
#endif
{
	int chn = *((int *)arg);

	int buf_size = 1024 * 512;
	char *Buffer = (char *)malloc(buf_size);
	FILE *fES = NULL;
	int position = 0;
	int iFrameNo = 0;
	int Timestamp = 0;
	int i = 0;

	fES = fopen(TestH264, "rb");
	if (NULL == fES)
	{
		printf("Test push file has not found.\n");
		return -1;
	}

	while (threadIsWork)
	{
		if (CarEye1078RTP_PusherIsReady(chn))
		{
			break;
#ifndef _WIN32
			usleep(1000);
#else
			Sleep(1);
#endif
		}
	}

	while (threadIsWork)
	{
		int nReadBytes = fread(Buffer + position, 1, 1, fES);
		if (nReadBytes < 1)
		{
			if (feof(fES))
			{
				position = 0;
				fseek(fES, 0, SEEK_SET);
				memset(Buffer, 0x00, buf_size);
				continue;
				
			}
			printf("Chn[%d] Read file error!", chn);
			break;
		}

		position++;

		if (position > 5)
		{
			unsigned char naltype = ((unsigned char)Buffer[position - 1] & 0x1F);

			if ((unsigned char)Buffer[position - 5] == 0x00 &&
				(unsigned char)Buffer[position - 4] == 0x00 &&
				(unsigned char)Buffer[position - 3] == 0x00 &&
				(unsigned char)Buffer[position - 2] == 0x01 &&
				//(((unsigned char)Buffer[position-1] == 0x61) ||
				//((unsigned char)Buffer[position-1] == 0x67) ) )
				(naltype == 0x07 || naltype == 0x01))
			{
				int framesize = position - 5;
				CarEye_AV_Frame avFrame;

				naltype = (unsigned char)Buffer[4] & 0x1F;

				memset(&avFrame, 0x00, sizeof(CarEye_AV_Frame));
				avFrame.FrameFlag = CAREYE_VFRAME_FLAG;
				avFrame.FrameLen = framesize;
				avFrame.Buffer = (unsigned char*)Buffer;
				avFrame.VFrameType = (naltype == 0x07) ? VIDEO_FRAME_I : VIDEO_FRAME_P;
				avFrame.Second = 0;
				avFrame.USecond = 0;
				avFrame.LastFrameInterval = 65;
				avFrame.LastIFrameInterval = 1000;
				CarEye1078RTP_PushData(chn, &avFrame);
				Timestamp += 1000 / MediaInfo.VideoFps;
#ifndef _WIN32
				usleep(40 * 1000);
#else
				Sleep(30);
#endif

				memmove(Buffer, Buffer + position - 5, 5);
				position = 5;

				iFrameNo++;

				if (iFrameNo == 100 || iFrameNo == 200)
				{
					for (i = 0; i < 8 && threadIsWork; i++)
					{
#ifndef _WIN32
						usleep(100000);
#else
						Sleep(100);
#endif
					}
				}
			}
		}
	}

	return NULL;
}

#ifdef TEST_NATIVE_FILE

// 本地MP4文件推流测试程序
int main()
{
	if (CarEye1078RTP_Register((char *)key) != CAREYE_NOERROR)
	{
		printf("Register pusher failed.\n");
		return -1;
	}

	CarEye1078RTP_RegisterStateChangedEvent(CarEyePusher_StateChangedEvent);

	int chn = CarEye1078RTP_StartNativeFile(ServerIP, ServerPort, "013510671870", 1, TestFile,0, 0);
	if (chn < 0)
	{
		printf("Start native file rtsp failed %d.\n", chn);
		return -1;
	}

	printf("Wait key stop channel[%d] rtsp...\n", chn);
	getchar();

	CarEye1078RTP_StopPusher(chn);
	printf("Wait key exit program...\n");
	getchar();

	return 0;
}

#elif defined TEST_1078_H264
// 媒体流推送测试程序
int main()
{
#ifdef _WIN32
	// 解码视频并推送的线程句柄
	HANDLE		thread_id;
#else
	pthread_t	thread_id;
#endif

	if (CarEye1078RTP_Register((char *)key) != CAREYE_NOERROR)
	{
		printf("Register pusher failed.\n");
		return -1;
	}

	MediaInfo.VideoCodec = CAREYE_VCODE_H264;
	MediaInfo.VideoFps = 25;
	MediaInfo.AudioCodec = CAREYE_ACODE_G711U;
	MediaInfo.Channels = 1;
	MediaInfo.Samplerate = 8000;
	strcpy(MediaInfo.SIM, "15720804002");
	MediaInfo.channel = 1;

	int chn = CarEye1078RTP_StartPusher(ServerIP, ServerPort, "12345", MediaInfo,1);
	if (chn < 0)
	{
		printf("Start push rtsp failed %d.\n", chn);
		return -1;
	}

	threadIsWork = true;
#ifdef _WIN32
	thread_id = CreateThread(NULL, 0, PushThreadEntry, &chn, 0, NULL);
	if (thread_id == NULL)
	{
		printf("Create push thread failed.\n");
		return -1;
	}
#else
	if (pthread_create(&thread_id, NULL, PushThreadEntry, &chn) != 0)
	{
		printf("Create push thread failed: %d.\n", errno);
		return -1;
	}
	pthread_detach(thread_id);
#endif

	printf("Wait key exit program...\n");
	getchar();
	threadIsWork = false;

	CarEye1078RTP_StopPusher(chn);
	return 0;
}

#elif defined TEST_MULTI_CHN

// 本地MP4文件与H264多通道推流测试程序
int main()
{
	int chns[4]{ -1, -1, -1, 1 };
	int i;

#ifdef _WIN32
	// 解码视频并推送的线程句柄
	HANDLE		thread_id;
#else
	pthread_t	thread_id;
#endif

	if (CarEye1078RTP_Register((char *)key) != CAREYE_NOERROR)
	{
		printf("Register pusher failed.\n");
		return -1;
	}

	CarEye1078RTP_RegisterStateChangedEvent(CarEyePusher_StateChangedEvent);
	int chn = CarEye1078RTP_StartNativeFile(ServerIP, ServerPort, "013510671870",1, TestFile, 12000, 32000);
	if (chn < 0)
	{
		printf("Start chn0 native file rtsp failed %d.\n", chn);
	}
	chns[0] = chn;

	chn = CarEye1078RTP_StartNativeFile(ServerIP, ServerPort, "013510671870", 2, TestFile,  32000, 0);
	if (chn < 0)
	{
		printf("Start chn1 native file rtsp failed %d.\n", chn);
	}
	chns[1] = chn;
#if 1
	MediaInfo.VideoCodec = CAREYE_VCODE_H264;
	MediaInfo.VideoFps = 25;
	MediaInfo.AudioCodec = CAREYE_ACODE_G711U;
	MediaInfo.Channels = 1;
	MediaInfo.Samplerate = 8000;
	strcpy(MediaInfo.SIM, "13510671870");
	MediaInfo.channel = 3;

	chn = CarEye1078RTP_StartPusher(ServerIP, ServerPort, "12345", MediaInfo);
	if (chn < 0)
	{
		printf("Start chn2 push rtsp failed %d.\n", chn);
	}
	chns[2] = chn;

	threadIsWork = true;
	if (chn >= 0) {
#ifdef _WIN32
		thread_id = CreateThread(NULL, 0, PushThreadEntry, &chn, 0, NULL);
		if (thread_id == NULL)
		{
			printf("Create chn2 push thread failed.\n");
		}
#else
		if (pthread_create(&thread_id, NULL, PushThreadEntry, &chn) != 0)
		{
			printf("Create chn2 push thread failed: %d.\n", errno);
		}
		else
		{
			pthread_detach(thread_id);
		}
#endif
	}
	chn = CarEye1078RTP_StartPusher(ServerIP, ServerPort, "3333", MediaInfo);
	if (chn < 0)
	{
		printf("Start chn3 push rtsp failed %d.\n", chn);
	}
	chns[3] = chn;

	if (chn >= 0)
	{
#ifdef _WIN32
		thread_id = CreateThread(NULL, 0, PushThreadEntry, &chn, 0, NULL);
		if (thread_id == NULL)
		{
			printf("Create chn3 push thread failed.\n");
		}
#else
		if (pthread_create(&thread_id, NULL, PushThreadEntry, &chn) != 0)
		{
			printf("Create chn3 push thread failed: %d.\n", errno);
		}
		else
		{
			pthread_detach(thread_id);
		}
#endif
	}
#endif
	printf("Wait key stop pushing...\n");
	getchar();
	threadIsWork = false;

	for (i = 0; i < 4; i++)
	{
		if (i < 2 && chns[i] >= 0)
		{
			CarEye1078RTP_StopNativeFile(chns[i]);
		}
		else if (chns[i] >= 0)
		{
			CarEye1078RTP_StopPusher(chn);
		}
	}
	printf("Wait key exit program...\n");
	getchar();

	return 0;
}

#endif

