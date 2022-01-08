// Konami Yu-Gi-Oh! Tag Force EHP/EhFolder packer/unpacker tool
// by Xan
// TODO: add EhFolder include.txt support (file inclusion/linking through txt file)
// TODO: ".efs" support? No idea what it is used for but the official tool supports it... It also requires efscrcmp.exe which we don't even have so there's that.
// TODO: test with GNU C compiler...

#include "stdafx.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// C++
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#if defined (_WIN32) || defined (_WIN64)
#include <windows.h>
#include <strsafe.h>
#include <ctype.h>
#endif

#define EHP_MAGIC 0x03504845
#define EHP_MAGIC2 0x20544F4E

struct EHPHead
{
	int32_t Magic; // 0x03504845 (EHP 0x3) - game checks only the first 3 bytes for the magic
	int32_t TotalFileSize;
	int32_t Magic2; // 0x20544F4E (NOT ) - this gets changed in memory to "BIND" when the file is in use
	int32_t FileCount;
}MainHeader = { EHP_MAGIC, 0, EHP_MAGIC2, 0 };

struct EHPFileEntry
{
	int32_t FileInfoPointer;
	int32_t FileOffset; // absolute
}*FileEntry;

char FileNameBuffer[1024];
char TempStringBuffer[1024];
char TempStringBuffer2[1024];
char TempStringBuffer3[1024];
wchar_t MkDirPath[1024];

char* OutputFileName; // used only in main

struct stat st = { 0 };

// pack mode stuff
char** FileDirectoryListing;
int32_t* PackerFileSizes;

// Cutin Model stuff
// _kao-ptrs.txt = contains a pointer to the mini_bu GIM file in the EHP (only seen in TF1)
// _info-ptrs.txt = contains pointers to the TMS, also optionally to filename-ptrs and filenameetc-ptrs if they exist (the filename-ptrs stuff only seen in TF1)
// all-ptrs.txt = pointers to all animations
// alletc-ptrs.txt = pointers to all-etc animations
// each game seems to have fixed indexes for which animation goes where

// cutin anim ptrs sizes are fixed per game
// used in: all-ptrs.txt, alletc-ptrs.txt
#define TF6_ANIMPTRS_SIZE 0xAB4
#define TF5_ANIMPTRS_SIZE 0x724
#define TF4_ANIMPTRS_SIZE 0x3B0
#define TF3_ANIMPTRS_SIZE 0xAD4
#define TF2_ANIMPTRS_SIZE 0xA38
#define TF1_ANIMPTRS_SIZE 0x5EC

// TF1 extra stuff
// used in filename-ptrs.txt, example: cutin_jyudai01-ptrs.txt
// currently untouched by the app, doesn't seem very important to the game as it works without that file anyways...
#define TF1_FILENAMEPTRS_SIZE 0x374 

// basic definitions to avoid confusion later on
#define TF6_ANIMPTRS 6
#define TF5_ANIMPTRS 5
#define TF4_ANIMPTRS 4
#define TF3_ANIMPTRS 3
#define TF2_ANIMPTRS 2
#define TF1_ANIMPTRS 1
#define UNK_ANIMPTRS 0

bool bHasAnimPtrs = false;
bool bHasAnimEtcPtrs = false;
unsigned int AnimCount = 0;
unsigned int AnimPtrsSize = 0;
unsigned int AnimPtrsMode = UNK_ANIMPTRS;
int32_t* AnimPointers = NULL;
unsigned int AnimPtrsOffset = 0;
unsigned int AnimEtcPtrsOffset = 0;

// these pointer defs are updated differently than anim ones
// due to their simplicity, they are updated automatically by the application after packing finishes
bool bHasInfoPtrs = false;
bool bHasKaoPtrs = false;
bool bHasFilenamePtrs = false;
bool bHasFilenameEtcPtrs = false;
unsigned int InfoPtrsIndex = 0;
unsigned int KaoPtrsIndex = 0;
unsigned int FilenamePtrsIndex = 0;
unsigned int FilenameEtcPtrsIndex = 0;


// dir list sorting - IMPORTANT FOR MODELS - files with '_' character in front of the filename go after numeric names but before alphabetic ones!
// credit to Philip Nicoletti for insensitive algo
namespace
{
	struct case_insensitive_less : public std::binary_function< char, char, bool >
	{
		bool operator () (char x, char y) const
		{
			if (y == '_') // '_' char takes precedence over the alphabet but not over numbers, hack to override the character
				y = 0x40;
			if (x == '_')
				x = 0x40;
			return toupper(static_cast<unsigned char>(x)) <
				toupper(static_cast<unsigned char>(y));
		}
	};

	bool NoCaseLess(const std::string& a, const std::string& b)
	{
		return std::lexicographical_compare(a.begin(), a.end(),
			b.begin(), b.end(), case_insensitive_less());
	}

}

int SortDirList()
{
	// i give up, i'll use C++ here to fix the sorting 
	// (maybe even reconstruct the entire string system with C++ later on, but for now, only sorting and nothing more)

	std::vector<std::string> sorter;
	int CppSorterCounter = 0;

	for (unsigned int i = 0; i < MainHeader.FileCount; i++)
	{
		sorter.push_back(FileDirectoryListing[i]);
		// release strings from memory to assign new ones later
		free(FileDirectoryListing[i]);
	}

	std::sort(sorter.begin(), sorter.end(), NoCaseLess);

	// C++ newbie here, stealing code from:
	// https://stackoverflow.com/questions/10750057/how-do-i-print-out-the-contents-of-a-vector
	for (std::vector<std::string>::const_iterator i = sorter.begin(); i != sorter.end(); ++i)
	{
		FileDirectoryListing[CppSorterCounter] = (char*)calloc(i->length() + 1, sizeof(char));
		strcpy(FileDirectoryListing[CppSorterCounter], i->c_str());
		CppSorterCounter++;
	}

	return 0;
}

unsigned int CountLinesInFile(const char* InFilename)
{
	FILE* finput = fopen(InFilename, "r");
	unsigned long int OldOffset = ftell(finput);
	unsigned int LineCount = 0;
	char ReadCh;

	while (!feof(finput))
	{
		ReadCh = fgetc(finput);
		if (ReadCh == '\n')
			LineCount++;
	}
	
	fclose(finput);
	return LineCount;
}

#ifdef WIN32
DWORD GetDirectoryListing(const char* FolderPath) // platform specific code, using Win32 here, GNU requires use of dirent which MSVC doesn't have -- TODO - make crossplat variant
{
	WIN32_FIND_DATA ffd = { 0 };
	TCHAR  szDir[MAX_PATH];
	char MBFilename[MAX_PATH];
	HANDLE hFind = INVALID_HANDLE_VALUE;
	DWORD dwError = 0;
	unsigned int NameCounter = 0;

	mbstowcs(szDir, FolderPath, MAX_PATH);
	StringCchCat(szDir, MAX_PATH, TEXT("\\*"));

	if (strlen(FolderPath) > (MAX_PATH - 3))
	{
		_tprintf(TEXT("Directory path is too long.\n"));
		return -1;
	}

	hFind = FindFirstFile(szDir, &ffd);

	if (INVALID_HANDLE_VALUE == hFind)
	{
		printf("FindFirstFile error\n");
		return dwError;
	}

	// count the files up first
	do
	{
		if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			MainHeader.FileCount++;
		}
	} while (FindNextFile(hFind, &ffd) != 0);

	dwError = GetLastError();
	if (dwError != ERROR_NO_MORE_FILES)
	{
		printf("FindFirstFile error\n");
	}
	FindClose(hFind);

	// then create a file list in an array, redo the code
	FileDirectoryListing = (char**)calloc(MainHeader.FileCount, sizeof(char*));
	PackerFileSizes = (int32_t*)calloc(MainHeader.FileCount, sizeof(int32_t*));
	
	ffd = { 0 };
	hFind = FindFirstFile(szDir, &ffd);
	if (INVALID_HANDLE_VALUE == hFind)
	{
		printf("FindFirstFile error\n");
		return dwError;
	}

	do
	{
		if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			wcstombs(MBFilename, ffd.cFileName, MAX_PATH);
			FileDirectoryListing[NameCounter] = (char*)calloc(strlen(MBFilename) + 1, sizeof(char));
			strcpy(FileDirectoryListing[NameCounter], MBFilename);
			NameCounter++;
		}
	} while (FindNextFile(hFind, &ffd) != 0);

	dwError = GetLastError();
	if (dwError != ERROR_NO_MORE_FILES)
	{
		printf("FindFirstFile error\n");
	}

	FindClose(hFind);
	return dwError;
}
#else
void GetDirectoryListing(const char* FolderPath)
{
	printf("Directory listing unimplemented for non-Win32 platforms.\n");
}
#endif

// precalc and prepare file entries
int32_t PreCalcFinalSize(const char* InPath, bool bOfficial)
{
	int32_t result = sizeof(EHPHead) + (sizeof(EHPFileEntry) * (MainHeader.FileCount + 1)) + (MainHeader.FileCount * sizeof(int32_t)); // Main Header size + (entry * (count + null terminator)) + filesize integers
	int32_t FileInfoPoint = sizeof(EHPHead) + (sizeof(EHPFileEntry) * (MainHeader.FileCount + 1)); // Main Header size + (entry * (count + null terminator))

	int32_t AlignedFileStart;

	char* PadNamePoint = NULL;
	
	FileEntry = (EHPFileEntry*)calloc(MainHeader.FileCount + 1, sizeof(EHPFileEntry));

	// add string sizes and add actual file sizes
	for (unsigned int i = 0; i < MainHeader.FileCount; i++)
	{
		result += strlen(FileDirectoryListing[i]) + 1;
	}

	if (result & 0xF)
	{
		AlignedFileStart = (result + 0x10) & 0xFFFFFFF0;
		printf("Size of header: 0x%x (aligned to: 0x%x)\n", result, AlignedFileStart);
		result = AlignedFileStart;
	}
	else
	{
		printf("Size of header: 0x%x\n", result);
	}
	

	for (unsigned int i = 0; i < MainHeader.FileCount; i++)
	{
		// update file entry stuff immediately
		FileEntry[i].FileInfoPointer = FileInfoPoint;
		FileEntry[i].FileOffset = result;
		FileInfoPoint += strlen(FileDirectoryListing[i]) + 1 + sizeof(int32_t);


		sprintf(TempStringBuffer, "%s\\%s", InPath, FileDirectoryListing[i]);

		if (stat(TempStringBuffer, &st))
		{
			printf("ERROR: Can't find %s during size calculation!\n", TempStringBuffer);
			return -1;
		}

		if (bOfficial)
			PadNamePoint = strchr(FileDirectoryListing[i], '#');

		if (bOfficial && strstr(FileDirectoryListing[i], "-ptrs.txt")) // ptrs files get turned into binary files later
			PackerFileSizes[i] = CountLinesInFile(TempStringBuffer) * sizeof(int32_t);
		else if (PadNamePoint) // txt files with # are padding files which in their name are followed by a size in hex format
			sscanf(PadNamePoint, "#%x.txt", &PackerFileSizes[i]);
		else
			PackerFileSizes[i] = st.st_size;

		if (PackerFileSizes[i] & 0xF)
		{
			AlignedFileStart = (PackerFileSizes[i] + 0x10) & 0xFFFFFFF0;
			printf("Size of %s: 0x%x (aligned to: 0x%x)\n", TempStringBuffer, PackerFileSizes[i], AlignedFileStart);
			result += AlignedFileStart;
		}
		else
		{
			if (PadNamePoint)
				printf("Padding %s: 0x%x\n", TempStringBuffer, PackerFileSizes[i]);
			else
				printf("Size of %s: 0x%x\n", TempStringBuffer, PackerFileSizes[i]);

			result += st.st_size;
		}
		PadNamePoint = NULL;
	}

	printf("Total file size: 0x%x bytes\n", result);

	return result;
}

int EHPPack(const char* InPath, const char* OutFilename, bool bOfficial)
{
	FILE *fin = NULL;
	// open the output file
	FILE *fout = fopen(OutFilename, "wb");
	void* FileBuffer = NULL;
	int AlignmentBytes = 0;
	char* PadNamePoint = NULL;

	if (!fout)
	{
		printf("ERROR: Can't open file %s for writing!\n", OutFilename);
		perror("ERROR");
		return -1;
	}

	if (!bOfficial)
	{
		// generate the ptrs names for later checking
		sprintf(TempStringBuffer2, "%s-ptrs.txt", InPath);
		sprintf(TempStringBuffer3, "%setc-ptrs.txt", InPath);
	}

	// parse the directory listing (one level deep), also writes to the MainHeader.FileCount
	GetDirectoryListing(InPath);
	// sort the dir list due to models
	SortDirList();
	// pre-calculate the final file size and prepare file entries
	MainHeader.TotalFileSize = PreCalcFinalSize(InPath, bOfficial);

	// start the writing process
	// write header information (16 bytes)
	fwrite(&MainHeader, sizeof(EHPHead), 1, fout);

	// write file entry pointers
	fwrite(FileEntry, sizeof(EHPFileEntry), MainHeader.FileCount + 1, fout);

	// write file entry infos (filename & size)
	for (unsigned int i = 0; i < MainHeader.FileCount; i++)
	{
		// filename
		fwrite(FileDirectoryListing[i], sizeof(char), strlen(FileDirectoryListing[i]) + 1, fout);

		// size
		fwrite(&PackerFileSizes[i], sizeof(int32_t), 1, fout);
	}

	// copy file data to the file
	for (unsigned int i = 0; i < MainHeader.FileCount; i++)
	{
		if (bOfficial)
			PadNamePoint = strchr(FileDirectoryListing[i], '#');

		if (!(bOfficial && (strstr(FileDirectoryListing[i], "-ptrs.txt")) || strchr(FileDirectoryListing[i], '#'))) // skip writing ptrs file and update it later, also skip padding files entirely
		{
			// open the file and copy it to buffer
			sprintf(TempStringBuffer, "%s\\%s", InPath, FileDirectoryListing[i]);
			fin = fopen(TempStringBuffer, "rb");
			printf("Writing: %s @ 0x%x (size: 0x%x)\n", TempStringBuffer, FileEntry[i].FileOffset, PackerFileSizes[i]);
			if (!fin)
			{
				printf("ERROR: Can't open file %s for reading!\n", TempStringBuffer);
				perror("ERROR");
				return -1;
			}
			FileBuffer = malloc(PackerFileSizes[i]);
			fread(FileBuffer, PackerFileSizes[i], 1, fin);
			fclose(fin);

			// seek to the correct spot in the file and write there
			fseek(fout, FileEntry[i].FileOffset, SEEK_SET);
			fwrite(FileBuffer, PackerFileSizes[i], 1, fout);
			free(FileBuffer);
		}

		if (!bOfficial) // we don't care about the custom ptrs format in official mode
		{
			// cutin anim pointer check
			if (bHasAnimPtrs && strcmp(FileDirectoryListing[i], "all-ptrs.txt") == 0)
				AnimPtrsOffset = FileEntry[i].FileOffset;
			if (bHasAnimEtcPtrs && strcmp(FileDirectoryListing[i], "alletc-ptrs.txt") == 0)
				AnimEtcPtrsOffset = FileEntry[i].FileOffset;

			// cutin other pointer check
			if (!bHasInfoPtrs && strcmp(FileDirectoryListing[i], "_info-ptrs.txt") == 0)
			{
				bHasInfoPtrs = true;
				InfoPtrsIndex = i;
			}
			if (!bHasKaoPtrs && strcmp(FileDirectoryListing[i], "_kao-ptrs.txt") == 0)
			{
				bHasKaoPtrs = true;
				KaoPtrsIndex = i;
			}
			if (!bHasFilenamePtrs && strcmp(FileDirectoryListing[i], TempStringBuffer2) == 0)
			{
				bHasFilenamePtrs = true;
				FilenamePtrsIndex = i;
			}
			if (!bHasFilenameEtcPtrs && strcmp(FileDirectoryListing[i], TempStringBuffer3) == 0)
			{
				bHasFilenameEtcPtrs = true;
				FilenameEtcPtrsIndex = i;
			}
		}
	}

	// lastly add alignment bytes to the end to make it sector aligned by 0x10
	if (ftell(fout) & 0xF)
	{
		AlignmentBytes = (ftell(fout) + 0x10) & 0xFFFFFFF0;
		AlignmentBytes -= ftell(fout);
		while (AlignmentBytes)
		{
			fputc(0, fout);
			AlignmentBytes--;
		}
	}

	fclose(fout);
	
	return 0;
}

int EHPCutin_UpdateInfoPtrs(const char* OutFilename)
{
	FILE* fout = fopen(OutFilename, "rb+");

	bool bFoundTMSFile = false;

	if (!fout)
	{
		printf("ERROR: Can't open file %s for writing!\n", OutFilename);
		perror("ERROR");
		return -1;
	}

	// search for & write the TMS index (and optionally the filename ptrs thingy)
	for (unsigned int i = 0; i < MainHeader.FileCount; i++)
	{
		if (strcmp(strrchr(FileDirectoryListing[i], '.'), ".tms") == 0)
		{
			bFoundTMSFile = true;
			fseek(fout, FileEntry[InfoPtrsIndex].FileOffset, SEEK_SET);
			fwrite(&FileEntry[i].FileOffset, sizeof(int32_t), 1, fout);
			if (bHasFilenamePtrs)
				fwrite(&FileEntry[FilenamePtrsIndex].FileOffset, sizeof(int32_t), 1, fout);
			if (bHasFilenameEtcPtrs)
				fwrite(&FileEntry[FilenameEtcPtrsIndex].FileOffset, sizeof(int32_t), 1, fout);
			break;
		}
	}

	if (!bFoundTMSFile)
		printf("WARNING: _info-ptrs.txt was not updated - accompanying .tms file is missing!\n");

	fclose(fout);

	return 0;
}

int EHPCutin_UpdateKaoPtrs(const char* OutFilename)
{
	FILE* fout = fopen(OutFilename, "rb+");

	bool bFoundMiniBu = false;

	if (!fout)
	{
		printf("ERROR: Can't open file %s for writing!\n", OutFilename);
		perror("ERROR");
		return -1;
	}

	// search for & write the mini_bu
	for (unsigned int i = 0; i < MainHeader.FileCount; i++)
	{
		if (strncmp(FileDirectoryListing[i], "mini_bu", 7) == 0)
		{
			bFoundMiniBu = true;
			fseek(fout, FileEntry[KaoPtrsIndex].FileOffset, SEEK_SET);
			fwrite(&FileEntry[i].FileOffset, sizeof(int32_t), 1, fout);
			break;
		}
	}

	if (!bFoundMiniBu)
		printf("WARNING: _kao-ptrs.txt was not updated - mini_bu texture is missing!\n");

	fclose(fout);

	return 0;
}

int EHPAnimReference(const char* OutFilename, const char* InFolder, const char* InPtrsFile, bool bIsEtc)
{
	FILE* fout = fopen(OutFilename, "rb+");
	FILE* fptrs = fopen(InPtrsFile, "rb");
	unsigned int AnimIndex = 0;

	if (!fout)
	{
		printf("ERROR: Can't open file %s for read/write!\n", OutFilename);
		perror("ERROR");
		return -1;
	}

	if (!fptrs)
	{
		printf("ERROR: Can't open file %s for reading!\n", InPtrsFile);
		perror("ERROR");
		return -1;
	}

	// read the version from the file itself first to determine the size of the array
	fscanf(fptrs, "TF%d\n", &AnimPtrsMode);

	switch (AnimPtrsMode)
	{
	case TF6_ANIMPTRS:
		AnimPtrsSize = TF6_ANIMPTRS_SIZE;
		break;
	case TF5_ANIMPTRS:
		AnimPtrsSize = TF5_ANIMPTRS_SIZE;
		break;
	case TF4_ANIMPTRS:
		AnimPtrsSize = TF4_ANIMPTRS_SIZE;
		break;
	case TF3_ANIMPTRS:
		AnimPtrsSize = TF3_ANIMPTRS_SIZE;
		break;
	case TF2_ANIMPTRS:
		AnimPtrsSize = TF2_ANIMPTRS_SIZE;
		break;
	case TF1_ANIMPTRS:
		AnimPtrsSize = TF1_ANIMPTRS_SIZE;
		break;
	default:
		AnimPtrsSize = 0;
		break;
	}
	// init array
	AnimCount = AnimPtrsSize / 4;
	AnimPointers = (int32_t*)calloc(AnimCount, sizeof(int32_t));
	AnimPointers[AnimCount - 1] = 0xFFFFFFFF;

	// get pointers
	while (!feof(fptrs))
	{
		// parse string from txt
		if (fgets(TempStringBuffer, 1024, fptrs))
		{
			if (TempStringBuffer[strlen(TempStringBuffer) - 1] == '\n')
				TempStringBuffer[strlen(TempStringBuffer) - 1] = 0;
			sscanf(TempStringBuffer, "%X = %s", &AnimIndex, TempStringBuffer2);


			// before searching the EHP, check if the file even exists!
			strcpy(FileNameBuffer, InFolder);
			sprintf(TempStringBuffer, "%s\\%s", FileNameBuffer, TempStringBuffer2);
			if (stat(TempStringBuffer, &st))
			{
				printf("ERROR: Can't find %s during animation referencing!\n", TempStringBuffer);
				return -1;
			}

			for (unsigned int j = 0; j < MainHeader.FileCount; j++)
			{
				// get EHP filename
				fseek(fout, FileEntry[j].FileInfoPointer, SEEK_SET);
				fgets(FileNameBuffer, 1024, fout);

				// check if filename matches the parsed txt
				if (strcmp(FileNameBuffer, TempStringBuffer2) == 0)
				{
					printf("Anim[%X] assigned to: %s (@ 0x%x)\n", AnimIndex, FileNameBuffer, FileEntry[j].FileOffset);
					AnimPointers[AnimIndex] = FileEntry[j].FileOffset;
					break;
				}
			}
		}
	}
	
	// write array to EHP's appropriate location
	if (bIsEtc)
	{
		fseek(fout, AnimEtcPtrsOffset, SEEK_SET);
		fwrite(AnimPointers, sizeof(int32_t), AnimCount, fout);
	}
	else
	{
		fseek(fout, AnimPtrsOffset, SEEK_SET);
		fwrite(AnimPointers, sizeof(int32_t), AnimCount, fout);
	}

	free(AnimPointers);
	fclose(fptrs);
	fclose(fout);

	return 0;
}

int EHPDereference(const char* InFilename, const char* InPtrsFile, const char* OutFilename)
{
	FILE *fin = fopen(InFilename, "rb");
	FILE *fptrs = fopen(InPtrsFile, "rb");
	FILE* fout = fopen(OutFilename, "wb");
	//int ptrchk = 0;
	//int counter = 1;
	int32_t* LocalAnimPointers = NULL;


	if (!fin)
	{
		printf("ERROR: Can't open file %s for reading!\n", InFilename);
		perror("ERROR");
		return -1;
	}

	if (!fptrs)
	{
		printf("ERROR: Can't open file %s for reading!\n", InPtrsFile);
		perror("ERROR");
		return -1;
	}

	// read main header
	fread(&MainHeader, sizeof(EHPHead), 1, fin);

	// wrong magic error handling
	if (MainHeader.Magic != EHP_MAGIC)
	{
		printf("ERROR: Wrong file magic! File does not start with EHP!\nFile is either corrupt or in wrong format.\n");
		return -1;
	}

	if (MainHeader.Magic2 != EHP_MAGIC2)
	{
		printf("WARNING: Wrong file magic (2)! Second magic isn't \"NOT \"!\n");
	}

	// read file entries
	FileEntry = (EHPFileEntry*)calloc(MainHeader.FileCount, sizeof(EHPFileEntry));
	fread(FileEntry, sizeof(EHPFileEntry), MainHeader.FileCount, fin);

	// get ptrs file size
	if (stat(InPtrsFile, &st))
	{
		printf("ERROR: Can't find %s during dereferencing!\n", InPtrsFile);
		return -1;
	}

	LocalAnimPointers = (int32_t*)malloc(st.st_size);
	AnimCount = st.st_size / 4;

	fread(LocalAnimPointers, sizeof(char), st.st_size, fptrs);
	fclose(fptrs);

	// start file dereferencing

	if (bHasAnimPtrs || bHasAnimEtcPtrs)
		fprintf(fout, "TF%d\n", AnimPtrsMode);

	for (unsigned int i = 0; i < AnimCount; i++)
	{
		for (unsigned int j = 0; j < MainHeader.FileCount; j++)
		{
			if (FileEntry[j].FileOffset == LocalAnimPointers[i])
			{
				// get filename
				fseek(fin, FileEntry[j].FileInfoPointer, SEEK_SET);
				fgets(FileNameBuffer, 1024, fin);
				fprintf(fout, "%X = %s\n", i, FileNameBuffer);
				//printf("%d (file ptr: %X ingame ptr: %X): Pointer: %.8X\tFile: %s\n", counter, ftell(fptrs) - 4, (ftell(fptrs) - 4) / 4, ptrchk, FileNameBuffer);
			}
		}
	}

	// start file dereferencing
	//while (!feof(fptrs))
	//{
	//	fread(&ptrchk, sizeof(int), 1, fptrs);
	//	if (ptrchk == -1)
	//		break;
	//	for (unsigned int i = 0; i < MainHeader.FileCount; i++)
	//	{
	//		if (FileEntry[i].FileOffset == ptrchk)
	//		{
	//			// get filename
	//			fseek(fin, FileEntry[i].FileInfoPointer, SEEK_SET);
	//			fgets(FileNameBuffer, 1024, fin);
	//
	//			printf("%d (file ptr: %X ingame ptr: %X): Pointer: %.8X\tFile: %s\n", counter, ftell(fptrs) - 4, (ftell(fptrs) - 4) / 4, ptrchk, FileNameBuffer);
	//			counter++;
	//		}
	//	}
	//}

	free(LocalAnimPointers);
	fclose(fout);
	fclose(fin);

	return 0;
}

// Update EhFolder ptrs.txt files in an EHP/EhFolder
int EhFolder_UpdatePtrs(const char* InFolder, const char* OutFilename)
{
	FILE* fin = NULL;
	FILE* fout = fopen(OutFilename, "rb+");
	int32_t* PtrsFileBuffer = NULL;
	unsigned int PtrsCount = 0;

	if (!fout)
	{
		printf("ERROR: Can't open file %s for reading + writing!\n", OutFilename);
		perror("ERROR");
		return -1;
	}

	for (unsigned int i = 0; i < MainHeader.FileCount; i++)
	{
		if (strstr(FileDirectoryListing[i], "-ptrs.txt"))
		{
			sprintf(TempStringBuffer2, "%s\\%s", InFolder, FileDirectoryListing[i]);
			printf("Updating ptrs: %s\n", TempStringBuffer2);
			fin = fopen(TempStringBuffer2, "r");
			if (!fin)
			{
				printf("ERROR: Can't open file %s for reading!\n", TempStringBuffer2);
				perror("ERROR");
				return -1;
			}

			PtrsCount = PackerFileSizes[i] / 4;
			PtrsFileBuffer = (int32_t*)malloc(PackerFileSizes[i]);

			for (unsigned int j = 0; j < PtrsCount; j++)
			{
				fgets(TempStringBuffer3, 1024, fin);
				if (TempStringBuffer3[strlen(TempStringBuffer3) - 2] == '\r')
					TempStringBuffer3[strlen(TempStringBuffer3) - 2] = 0;
				else if (TempStringBuffer3[strlen(TempStringBuffer3) - 1] == '\n')
					TempStringBuffer3[strlen(TempStringBuffer3) - 1] = 0;

				if (strcmp(TempStringBuffer3, "END") == 0)
				{
					PtrsFileBuffer[j] = 0xFFFFFFFF;
					break;
				}
				else if (strcmp(TempStringBuffer3, "NULL") == 0)
					PtrsFileBuffer[j] = 0;
				else
				{
					for (unsigned int k = 0; k < MainHeader.FileCount; k++)
					{
						if (strcmp(TempStringBuffer3, FileDirectoryListing[k]) == 0)
						{
							PtrsFileBuffer[j] = FileEntry[k].FileOffset;
							break;
						}
					}
				}

			}
			fclose(fin);
			
			fseek(fout, FileEntry[i].FileOffset, SEEK_SET);
			fwrite(PtrsFileBuffer, PackerFileSizes[i], 1, fout);

			free(PtrsFileBuffer);
		}
	}

	fclose(fout);

	return 0;
}

// Dereference EhFolder ptrs.txt files from an EHP/EhFolder
int EhFolder_ReversePtrs(const char* InFolder)
{
	FILE* fin = NULL;
	FILE* fout = NULL;
	int32_t* PtrsFileBuffer = NULL;
	unsigned int PtrsCount = 0;
	struct stat st = {0};

	for (unsigned int i = 0; i < MainHeader.FileCount; i++)
	{
		if (strstr(FileDirectoryListing[i], "-ptrs.txt"))
		{
			sprintf(TempStringBuffer2, "%s\\%s", InFolder, FileDirectoryListing[i]);
			printf("Dereferencing ptrs: %s\n", TempStringBuffer2);
			fin = fopen(TempStringBuffer2, "rb");
			if (!fin)
			{
				printf("ERROR: Can't open file %s for reading!\n", TempStringBuffer2);
				perror("ERROR");
				return -1;
			}
			if (stat(TempStringBuffer2, &st))
			{
				printf("ERROR: Can't find %s during size reading!\n", TempStringBuffer2);
				fclose(fin);
				return -1;
			}
			PtrsCount = st.st_size / 4;
			PtrsFileBuffer = (int32_t*)malloc(st.st_size);
			fread(PtrsFileBuffer, st.st_size, 1, fin);
			fclose(fin);
			fout = fopen(TempStringBuffer2, "w");
			if (!fin)
			{
				printf("ERROR: Can't open file %s for writing!\n", TempStringBuffer2);
				perror("ERROR");
				return -1;
			}


			for (unsigned int j = 0; j < PtrsCount; j++)
			{
				if (PtrsFileBuffer[j] == 0xFFFFFFFF)
				{
					fprintf(fout, "END\n");
					break;
				}
				else if (!PtrsFileBuffer[j])
					fprintf(fout, "NULL\n");
				else
				{
					for (unsigned int k = 0; k < MainHeader.FileCount; k++)
					{
						if (FileEntry[k].FileOffset == PtrsFileBuffer[j])
						{
							fprintf(fout, "%s\n", FileDirectoryListing[k]);
							break;
						}
					}
				}
			}
			free(PtrsFileBuffer);
			fclose(fout);

			PtrsCount = 0;
		}
	}

	return 0;
}

int EHPExtract(const char* InFilename, const char* OutPath, bool bOfficial)
{
	FILE *fin = fopen(InFilename, "rb");
	FILE *fout = NULL;
	void* FileBuffer = NULL;
	int FilenameSize = 0;
	int FileSize = 0;

	if (!fin)
	{
		printf("ERROR: Can't open file %s for reading!\n", InFilename);
		perror("ERROR");
		return -1;
	}
	
	// read main header
	fread(&MainHeader, sizeof(EHPHead), 1, fin);

	// wrong magic error handling
	if (MainHeader.Magic != EHP_MAGIC)
	{
		printf("ERROR: Wrong file magic! File does not start with EHP!\nFile is either corrupt or in wrong format.\n");
		return -1;
	}

	if (MainHeader.Magic2 != EHP_MAGIC2)
	{
		printf("WARNING: Wrong file magic (2)! Second magic isn't \"NOT \"!\n");
	}

	// read file entries
	FileEntry = (EHPFileEntry*)calloc(MainHeader.FileCount, sizeof(EHPFileEntry));
	FileDirectoryListing = (char**)calloc(MainHeader.FileCount, sizeof(char*));
	fread(FileEntry, sizeof(EHPFileEntry), MainHeader.FileCount, fin);

	// start file extraction
	for (unsigned int i = 0; i < MainHeader.FileCount; i++)
	{
		// get filename
		fseek(fin, FileEntry[i].FileInfoPointer, SEEK_SET);
		fgets(FileNameBuffer, 1024, fin);

		// get filesize
		FilenameSize = strlen(FileNameBuffer) + 1; 
		FileDirectoryListing[i] = (char*)calloc(FilenameSize, sizeof(char));
		fseek(fin, FileEntry[i].FileInfoPointer + FilenameSize, SEEK_SET); // go to the end of the string, there lies the filesize
		fread(&FileSize, sizeof(int32_t), 1, fin);

		if (!bOfficial)
		{
			// detect cutin anim pointers file
			if (strcmp(FileNameBuffer, "all-ptrs.txt") == 0)
			{
				printf("INFO: Will generate anim definition file!\n");
				bHasAnimPtrs = true;
				AnimPtrsSize = FileSize;

				switch (FileSize)
				{
				case TF6_ANIMPTRS_SIZE:
					printf("INFO: Detected Tag Force 6!\n");
					AnimPtrsMode = TF6_ANIMPTRS;
					break;
				case TF5_ANIMPTRS_SIZE:
					printf("INFO: Detected Tag Force 5!\n");
					AnimPtrsMode = TF5_ANIMPTRS;
					break;
				case TF4_ANIMPTRS_SIZE:
					printf("INFO: Detected Tag Force 4!\n");
					AnimPtrsMode = TF4_ANIMPTRS;
					break;
				case TF3_ANIMPTRS_SIZE:
					printf("INFO: Detected Tag Force 3!\n");
					AnimPtrsMode = TF3_ANIMPTRS;
					break;
				case TF2_ANIMPTRS_SIZE:
					printf("INFO: Detected Tag Force 2!\n");
					AnimPtrsMode = TF2_ANIMPTRS;
					break;
				case TF1_ANIMPTRS_SIZE:
					printf("INFO: Detected Tag Force 1!\n");
					AnimPtrsMode = TF1_ANIMPTRS;
					break;
				default:
					printf("INFO: Anim-ptrs game unknown!\n");
					break;
				}
			}

			if (strcmp(FileNameBuffer, "alletc-ptrs.txt") == 0)
			{
				printf("INFO: Will generate anim-etc definition file!\n");
				bHasAnimEtcPtrs = true;
			}
		}

		// create a buffer & copy the file to buffer
		FileBuffer = malloc(FileSize);
		fseek(fin, FileEntry[i].FileOffset, SEEK_SET);
		fread(FileBuffer, FileSize, 1, fin);

		// check for folder existence, if it doesn't exist, make it
		if (stat(OutPath, &st))
		{
			printf("Creating folder: %s\n", OutPath);
			mbstowcs(MkDirPath, OutPath, 1024);
			_wmkdir(MkDirPath);
		}

		// write buffer to a new file at the output path
		strcpy(FileDirectoryListing[i], FileNameBuffer);
		sprintf(TempStringBuffer, "%s\\%s", OutPath, FileNameBuffer);
		printf("Extracting: %s @ 0x%x (size 0x%x)\n", TempStringBuffer, FileEntry[i].FileOffset, FileSize);
		fout = fopen(TempStringBuffer, "wb");
		if (!fout)
		{
			printf("ERROR: Can't open file %s for writing!\n", TempStringBuffer);
			perror("ERROR");
			return -1;
		}
		fwrite(FileBuffer, FileSize, 1, fout);
		fclose(fout);

		// release buffer memory
		free(FileBuffer);
		FileSize = 0;
		FilenameSize = 0;
		memset(FileNameBuffer, 0, FilenameSize);
	}
	fclose(fin);

	return 0;
}


int main(int argc, char *argv[])
{
	char* CharPatchPoint = NULL;
	printf("Yu-Gi-Oh! Tag Force EHP/EhFolder Tool\n\n");


	if (argc < 2)
	{
		printf("USAGE (extract): %s InFileName [OutFolder]\n", argv[0]);
		printf("USAGE (pack): %s -p InFolder [OutFileName]\n", argv[0]);
		printf("USAGE (extract (custom)): %s -c InFileName [OutFolder]\n", argv[0]);
		printf("USAGE (pack (custom)): %s -pc InFolder [OutFileName]\n", argv[0]);
		printf("\nIf the optional (in []) parameter isn't specified, it'll reuse the input name.\nCustom mode uses a custom format for ptrs files which is easier to edit, useful for porting models.\nNOTE: You can only repack custom mode files in custom packing mode if they used ptrs\n");
		return -1;
	}

	if (argv[1][0] == '-' && argv[1][1] == 'p') // Pack mode
	{

		if (argv[1][2] == 'c')
		{
			printf("Packing (custom) mode\n");
			if (argc == 3)
			{
				OutputFileName = (char*)calloc(strlen(argv[2]), sizeof(char) + 8);
				strcpy(OutputFileName, argv[2]);
				strcat(OutputFileName, ".ehp");
			}
			else
				OutputFileName = argv[3];

			strcpy(FileNameBuffer, argv[2]);
			strcat(FileNameBuffer, "_anim-ptrs.txt");
			if (!(stat(FileNameBuffer, &st)))
			{
				printf("Detected animation pointers!\n");
				bHasAnimPtrs = true;
			}

			strcpy(FileNameBuffer, argv[2]);
			strcat(FileNameBuffer, "_animetc-ptrs.txt");
			if (!(stat(FileNameBuffer, &st)))
			{
				printf("Detected animation-etc pointers!\n");
				bHasAnimEtcPtrs = true;
			}
			EHPPack(argv[2], OutputFileName, false);

			if (bHasAnimPtrs)
			{
				printf("Writing animation pointers to EHP!\n");
				strcpy(FileNameBuffer, argv[2]);
				strcat(FileNameBuffer, "_anim-ptrs.txt");
				EHPAnimReference(OutputFileName, argv[2], FileNameBuffer, false);
			}

			if (bHasAnimEtcPtrs)
			{
				printf("Writing animation-etc pointers to EHP!\n");
				strcpy(FileNameBuffer, argv[2]);
				strcat(FileNameBuffer, "_animetc-ptrs.txt");
				EHPAnimReference(OutputFileName, argv[2],FileNameBuffer, true);
			}

			if (bHasInfoPtrs)
			{
				printf("Writing _info-ptrs.txt to EHP!\n");
				EHPCutin_UpdateInfoPtrs(OutputFileName);
			}

			if (bHasKaoPtrs)
			{
				printf("Writing _kao-ptrs.txt to EHP!\n");
				EHPCutin_UpdateKaoPtrs(OutputFileName);
			}

			// TODO: if neccessary, update the filename-ptrs.txt file too, it's used in TF1 only...

			return 0;

		}

		printf("Packing mode\n");
		if (argc == 3)
		{
			OutputFileName = (char*)calloc(strlen(argv[2]), sizeof(char) + 8);
			strcpy(OutputFileName, argv[2]);
			strcat(OutputFileName, ".ehp");
		}
		else
			OutputFileName = argv[3];

		EHPPack(argv[2], OutputFileName, true);
		EhFolder_UpdatePtrs(argv[2], OutputFileName);
		return 0;
	}

	if (argv[1][0] == '-' && argv[1][1] == 'c') // Extraction mode (official format)
	{
		printf("Extraction (custom) mode\n");
		if (argc == 3)
		{
			char* PatchPoint;
			OutputFileName = (char*)calloc(strlen(argv[2]), sizeof(char));
			strcpy(OutputFileName, argv[2]);
			PatchPoint = strrchr(OutputFileName, '.');
			*PatchPoint = 0;
		}
		else
			OutputFileName = argv[3];

		EHPExtract(argv[2], OutputFileName, false);

		if (bHasAnimPtrs)
		{
			sprintf(TempStringBuffer, "%s\\%s", OutputFileName, "all-ptrs.txt");
			sprintf(FileNameBuffer, "%s_anim-ptrs.txt", OutputFileName);
			EHPDereference(argv[2], TempStringBuffer, FileNameBuffer);
		}

		if (bHasAnimEtcPtrs)
		{
			sprintf(TempStringBuffer, "%s\\%s", OutputFileName, "alletc-ptrs.txt");
			sprintf(FileNameBuffer, "%s_animetc-ptrs.txt", OutputFileName);
			EHPDereference(argv[2], TempStringBuffer, FileNameBuffer);
		}

		return 0;
	}

	printf("Extraction mode\n");
	if (argc == 2)
	{
		char* PatchPoint;
		OutputFileName = (char*)calloc(strlen(argv[1]), sizeof(char));
		strcpy(OutputFileName, argv[1]);
		PatchPoint = strrchr(OutputFileName, '.');
		*PatchPoint = 0;
	}
	else
		OutputFileName = argv[2];

	EHPExtract(argv[1], OutputFileName, true);
	EhFolder_ReversePtrs(OutputFileName);

	return 0;
}

