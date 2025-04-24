
#include "TRACE/tracemf.h"
#define TRACE_NAME "CFOandDTC_DMAs.cpp"

#include "CFOandDTC_DMAs.h"
// #define TLVL_GetData TLVL_DEBUG + 5
// #define TLVL_GetJSONData TLVL_DEBUG + 6
// #define TLVL_ReadBuffer TLVL_DEBUG + 7
#define TLVL_ReadNextDAQPacket TLVL_DEBUG + 8
// #define TLVL_ReadNextDCSPacket TLVL_DEBUG + 9
// #define TLVL_SendDCSRequestPacket TLVL_DEBUG + 10
// #define TLVL_SendHeartbeatPacket TLVL_DEBUG + 11
// #define TLVL_VerifySimFileInDTC TLVL_DEBUG + 12
// #define TLVL_VerifySimFileInDTC2 TLVL_DEBUG + 13
// #define TLVL_VerifySimFileInDTC3 TLVL_DEBUG + 14
// #define TLVL_WriteSimFileToDTC TLVL_DEBUG + 15
// #define TLVL_WriteSimFileToDTC2 TLVL_DEBUG + 16
// #define TLVL_WriteSimFileToDTC3 TLVL_DEBUG + 17
// #define TLVL_WriteDetectorEmulatorData TLVL_DEBUG + 18
// #define TLVL_WriteDataPacket TLVL_DEBUG + 19
// #define TLVL_ReleaseBuffers TLVL_DEBUG + 20
#define TLVL_GetCurrentBuffer TLVL_DEBUG + 21

#include "dtcInterfaceLib/otsStyleCoutMacros.h"

#include <unistd.h>
#include <fstream>
#include <iostream>
#include <sstream>  // Convert uint to hex string

int DTCLib::CFOandDTC_DMAs::GetCurrentBuffer(DMAInfo* info)
{
	TLOG(TLVL_GetCurrentBuffer) << "GetCurrentBuffer BEGIN currentReadPtr=" << (void*)info->currentReadPtr
								<< " buffer.size()=" << info->buffer.size();

	if (info->buffer.size())  // might crash in ReleaseBuffers if currentReadPtr = nullptr ????????????????????????????????
	{
		TLOG(TLVL_GetCurrentBuffer) << "GetCurrentBuffer returning info->buffer.size()=" << info->buffer.size();
		return info->buffer.size();
	}

	if (info->currentReadPtr == nullptr || info->buffer.size() == 0)
	{
		TLOG(TLVL_GetCurrentBuffer) << "GetCurrentBuffer returning -1 because not currently reading a buffer";
		return -1;
	}

#if 0
	for (size_t ii = 0; ii < info->buffer.size(); ++ii)
	{
		auto bufferptr = *info->buffer[ii];
		uint16_t bufferSize = *reinterpret_cast<uint16_t*>(bufferptr);
		TLOG(TLVL_GetCurrentBuffer) << "GetCurrentBuffer bufferptr="<<(void*)bufferptr<<" bufferSize="<<bufferSize;
		if (info->currentReadPtr > bufferptr &&
			info->currentReadPtr < bufferptr + bufferSize)
		{
			TLOG(TLVL_GetCurrentBuffer) << "Found matching buffer at index " << ii << ".";
			return ii;
		}
	}
#endif
	TLOG(TLVL_GetCurrentBuffer) << "GetCurrentBuffer returning -2: Have buffers but none match read ptr position, need new";
	return -2;
}

uint16_t DTCLib::CFOandDTC_DMAs::GetBufferByteCount(DMAInfo* info, size_t index)
{
	if (index >= info->buffer.size()) return 0;
	auto bufferptr = *info->buffer[index];
	uint16_t bufferSize = *reinterpret_cast<uint16_t*>(bufferptr);
	TLOG(TLVL_ReadNextDAQPacket) << "bufferSize = " << bufferSize;
	return bufferSize;
}
