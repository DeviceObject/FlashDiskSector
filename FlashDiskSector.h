#ifndef __FLASH_DISK_SECTOR_H__
#define __FLASH_DISK_SECTOR_H__

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:

#include <windows.h>
#include <winioctl.h>
#include <strsafe.h>
#include "aplib.h"

#pragma pack(push)
#pragma pack(1)
typedef struct _CHSS
{
	LARGE_INTEGER   lStartSector;
	USHORT          uNumberSectors;
	ULONG           ulXorValue;
}CHSS,*PCHSS;
typedef struct _LDRDRV
{
	ULONG ulSignature;
	ULONG ulOffset;
	ULONG ulLength;
	ULONG ulXor;
}LDRDRV,*PLDRDRV;
#pragma pack(pop)

#define		BK_NAME_MAGIC					(ULONG)0x44444444
#define		BK_NAME_LENGTH					200

#define		NTFS_OEM_ID						"NTFS    "
#define		NTFS_LOADER_SIZE				16		// sectors
#define		NTFS_LDR_HEADER_SIZE			0x20	// bytes

#define		OP_JMP_SHORT					0xeb
#define		OP_JMP_NEAR						0xe9

#define		BIOS_DEFAULT_SECTOR_SIZE		0x200		// 512 bytes


extern int __argc;
extern char **__argv;
BOOLEAN GetFileDat(PCHAR pFileName,PCHAR* pDat,PULONG ulSize);

#endif