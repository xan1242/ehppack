// Konami Yu-Gi-Oh! Tag Force EHP packer/unpacker tool
// by Xan

#include "stdafx.h"
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#include <strsafe.h>
#endif

#define EHP_MAGIC 0x03504845
#define EHP_MAGIC2 0x20544F4E

struct EHPHead
{
	int Magic; // 0x03504845 (EHP 0x3)
	int TotalFileSize;
	int Magic2; // 0x20544F4E (NOT )
	int FileCount;
}MainHeader = { EHP_MAGIC, 0, EHP_MAGIC2, 0 };

struct EHPFileEntry
{
	int FileInfoPointer;
	int FileOffset; // absolute
}*FileEntry;

char FileNameBuffer[1024];
char TempStringBuffer[1024];
wchar_t MkDirPath[1024];

char* OutputFileName; // used only in main

struct stat st = { 0 };

// pack mode stuff
char** FileDirectoryListing;
unsigned int* PackerFileSizes;

#ifdef WIN32
DWORD GetDirectoryListing(const char* FolderPath) // platform specific code, using Win32 here, GNU requires use of dirent which MSVC doesn't have
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
	PackerFileSizes = (unsigned int*)calloc(MainHeader.FileCount, sizeof(unsigned int*));
	
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
int PreCalcFinalSize(const char* InPath)
{
	int result = sizeof(EHPHead) + (sizeof(EHPFileEntry) * (MainHeader.FileCount + 1)) + (MainHeader.FileCount * sizeof(int)); // Main Header size + (entry * (count + null terminator)) + filesize integers
	int FileInfoPoint = sizeof(EHPHead) + (sizeof(EHPFileEntry) * (MainHeader.FileCount + 1)); // Main Header size + (entry * (count + null terminator))

	unsigned int AlignedFileStart;
	unsigned int AlignmentZeroes;
	
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
		FileInfoPoint += strlen(FileDirectoryListing[i]) + 1 + sizeof(int);


		sprintf(TempStringBuffer, "%s\\%s", InPath, FileDirectoryListing[i]);

		if (stat(TempStringBuffer, &st))
		{
			printf("ERROR: Can't find %s during size calculation!\n", TempStringBuffer);
			return -1;
		}
		PackerFileSizes[i] = st.st_size;
		if (st.st_size & 0xF)
		{
			AlignedFileStart = (st.st_size + 0x10) & 0xFFFFFFF0;
			printf("Size of %s: 0x%x (aligned to: 0x%x)\n", TempStringBuffer, st.st_size, AlignedFileStart);
			result += AlignedFileStart;
		}
		else
		{
			printf("Size of %s: 0x%x\n", TempStringBuffer, st.st_size);
			result += st.st_size;
		}
	}

	printf("Total file size: 0x%x bytes\n", result);

	return result;
}

int EHPPack(const char* InPath, const char* OutFilename)
{
	FILE *fin = NULL;
	// open the output file
	FILE *fout = fopen(OutFilename, "wb");
	void* FileBuffer = NULL;
	int AlignmentBytes = 0;

	if (!fout)
	{
		printf("ERROR: Can't open file %s for writing!\n", OutFilename);
		perror("ERROR");
		return -1;
	}

	// parse the directory listing (one level deep), also writes to the MainHeader.FileCount
	GetDirectoryListing(InPath);
	// pre-calculate the final file size and prepare file entries
	MainHeader.TotalFileSize = PreCalcFinalSize(InPath);

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
		fwrite(&PackerFileSizes[i], sizeof(int), 1, fout);
	}

	// copy file data to the file
	for (unsigned int i = 0; i < MainHeader.FileCount; i++)
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

	// lastly add alignment bytes to the end to make it aligned by 0x10
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

int EHPExtract(const char* InFilename, const char* OutPath)
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
	fread(FileEntry, sizeof(EHPFileEntry), MainHeader.FileCount, fin);

	// start file extraction
	for (unsigned int i = 0; i < MainHeader.FileCount; i++)
	{
		// get filename
		fseek(fin, FileEntry[i].FileInfoPointer, SEEK_SET);
		fgets(FileNameBuffer, 1024, fin);
		
		// get filesize
		FilenameSize = strlen(FileNameBuffer) + 1; 
		fseek(fin, FileEntry[i].FileInfoPointer + FilenameSize, SEEK_SET); // go to the end of the string, there lies the filesize
		fread(&FileSize, sizeof(int), 1, fin);

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
	printf("Yu-Gi-Oh! Tag Force EHP Tool\n");

	if (argc < 2)
	{
		printf("USAGE (extract): %s InFileName [OutFolder]\n", argv[0]);
		printf("USAGE (pack): %s -p InFolder [OutFileName]\n", argv[0]);
		printf("If the optional (in []) parameter isn't specified, it'll reuse the input name.\n");
		return -1;
	}

	if (argv[1][0] == '-' && argv[1][1] == 'p') // Pack mode
	{
		printf("Packing mode\n");
		if (argc == 3)
		{
			OutputFileName = (char*)calloc(strlen(argv[2]), sizeof(char) + 8);
			strcpy(OutputFileName, argv[2]);
			strcat(OutputFileName, ".ehp");
		}
		else
			OutputFileName = argv[3];

		return EHPPack(argv[2], OutputFileName);
	}

	
	if (argc == 2) // Extraction mode
	{
		printf("Extraction mode\n");
		char* PatchPoint;
		OutputFileName = (char*)calloc(strlen(argv[1]), sizeof(char));
		strcpy(OutputFileName, argv[1]);
		PatchPoint = strrchr(OutputFileName, '.');
		*PatchPoint = 0;
	}
	else
		OutputFileName = argv[2];

    return EHPExtract(argv[1], OutputFileName);
}

