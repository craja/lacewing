
/*
 * Copyright (c) 2011 James McLaughlin.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "Common.h"

Lacewing::Sync Sync_DebugOutput;
ostringstream DebugOutput;

#ifdef LacewingWindows
    Lacewing::Sync Sync_GMTime;
#endif

const char * Lacewing::Version()
{
    static bool GotVersion = false;
    static char VersionString[64];

    if(!GotVersion)
    {
        const char * Version, * Platform;

        #ifdef LacewingWindows
            Platform = "Windows";
        #else
            utsname name;
            uname(&name);
            Platform = name.sysname;
        #endif

        #ifdef COXSDK
            Version = "Build #18 Alpha 4";
        #else
            Version = "0.1.0";
        #endif

        sprintf(VersionString, "Lacewing %s (%s/%s)", Version, Platform, sizeof(void *) == 8 ? "x64" : "x86");
        GotVersion = true;
    }

    return VersionString;
}

bool Initialised = false;

void LacewingInitialise()
{
    if(Initialised)
        return;

    #ifdef LacewingWindows

        WSADATA WinsockData;

        if(WSAStartup(MAKEWORD(2, 2), &WinsockData))
            return;
    
    #else

        SSL_library_init();

        STACK_OF(SSL_COMP) * comp_methods = SSL_COMP_get_compression_methods();
        sk_SSL_COMP_zero(comp_methods);

    #endif

    Initialised = true;
}

void Lacewing::Pause(int Milliseconds)
{
    Sleep(Milliseconds);
}

void Lacewing::Yield()
{
    LacewingYield();
}

bool Lacewing::FileExists(const char * Filename)
{
    #ifdef LacewingWindows

        return (GetFileAttributesA(Filename) & FILE_ATTRIBUTE_DIRECTORY) == 0;
        
    #else

        struct stat Attributes;

        if(stat(Filename, &Attributes) == 0)
            return !S_ISDIR(Attributes.st_mode);

        return false;

    #endif

    return false;
}

bool Lacewing::PathExists(const char * Filename)
{
    #ifdef LacewingWindows

        int attr = GetFileAttributesA(Filename);
        return attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
        
    #else

        struct stat Attributes;

        if(stat(Filename, &Attributes) == 0)
            return S_ISDIR(Attributes.st_mode);

        return false;

    #endif

    return false;
}

lw_i64 Lacewing::FileSize(const char * Filename)
{
    #ifdef LacewingWindows
    
        WIN32_FILE_ATTRIBUTE_DATA FileInformation;

        if(!GetFileAttributesExA(Filename, GetFileExInfoStandard, &FileInformation))
            return 0;

        LARGE_INTEGER Size;
        
        Size.LowPart = FileInformation.nFileSizeLow;
        Size.HighPart = FileInformation.nFileSizeHigh;

        return Size.QuadPart;
        
    #else
        
        struct stat Attributes;

        if(stat(Filename, &Attributes) != 0)
            return 0;

        return Attributes.st_size;
        
    #endif
    
}

lw_i64 Lacewing::LastModified(const char * Filename)
{   
    #ifdef LacewingWindows
    
        WIN32_FILE_ATTRIBUTE_DATA FileInformation;

        if(!GetFileAttributesExA(Filename, GetFileExInfoStandard, &FileInformation))
            return 0;
    
        return FileTimeToUnixTime(FileInformation.ftLastWriteTime);
    
    #else
    
        struct stat Attributes;

        if(stat(Filename, &Attributes) != 0)
            return 0;

        return Attributes.st_mtime;
        
    #endif
}

bool Lacewing::StartThread(void * Function, void * Parameter)
{
    #ifdef LacewingWindows

        HANDLE Thread = (HANDLE) _beginthreadex(0, 0, (unsigned (__stdcall *) (void *)) Function, Parameter, 0, 0);
        CloseHandle(Thread);

        return Thread != INVALID_HANDLE_VALUE;

    #else
    
        pthread_t Thread = 0;

        if(pthread_create(&Thread, 0, (void * (*) (void *)) Function, Parameter))
            return false;
        
        return true;

    #endif

    return false;
}

/* TODO : Returning pthread_self as a lw_i64 is a hack, because pthread_t is opaque
          We should assign our own thread IDs (which should be 32-bit anyway) */
    
lw_i64 Lacewing::CurrentThreadID()
{
    #ifdef LacewingWindows

        return GetCurrentThreadId();

    #else

        return (lw_i64) pthread_self();

    #endif

    return -1;
}

void Lacewing::SetCurrentThreadName(const char * Name)
{
    #ifdef LacewingWindows
        
        struct
        {
              DWORD dwType;
              LPCSTR szName;
              DWORD dwThreadID;
              DWORD dwFlags;

        } ThreadNameInfo;

        ThreadNameInfo.dwType      = 0x1000;
        ThreadNameInfo.szName      =   Name;
        ThreadNameInfo.dwThreadID  =     -1;
        ThreadNameInfo.dwFlags     =      0;

        __try
        {
            RaiseException(0x406D1388, 0, sizeof(ThreadNameInfo) / sizeof(ULONG), (ULONG *) &ThreadNameInfo);
        }
        __except (EXCEPTION_CONTINUE_EXECUTION)
        {
        }

    #else

        #if HAVE_DECL_PR_SET_NAME != 0
            prctl(PR_SET_NAME, (unsigned long) Name, 0, 0, 0);
        #endif
        
    #endif
}

int Lacewing::CountProcessors()
{

#ifdef LacewingWindows

    static int ProcessorCount = 0;

    if(!ProcessorCount)
    {
        SYSTEM_INFO SystemInformation;
        GetSystemInfo(&SystemInformation);
        ProcessorCount = SystemInformation.dwNumberOfProcessors;
    }

    return ProcessorCount;

#else

    #ifdef HAVE_CORESERVICES_CORESERVICES_H
        return MPProcessors();
    #endif
    
    /* TODO : Unix, and not Darwin */
    
    return 2;

#endif

}

void Lacewing::Int64ToString(lw_i64 Value, char * Output)
{
    sprintf(Output, I64Format, Value);
}

void Lacewing::TempPath(char * Buffer, int Length)
{
    #ifdef LacewingWindows

        GetTempPathA(Length, Buffer);

        for(char * i = Buffer; *i; ++ i)
            if(*i == '\\')
                *i = '/';

    #else

        char * Temp = getenv("TMPDIR");

        if(Temp)
        {
            strcpy(Buffer, Temp);
            return;
        }

        if(P_tmpdir)
        {
            strcpy(Buffer, P_tmpdir);
            return;
        }

        strcpy(Buffer, "/tmp/");
        return;
            
    #endif
}

void Lacewing::NewTempFile(char * Buffer, int Length)
{
    FILE * File;

    do
    {   char TempName[64];

        for(int i = 0; i < sizeof(TempName); i += sizeof(lw_i64))
            *(lw_i64 *) (TempName + i) = i % 2 == 0 ? (lw_i64) time(0) : (lw_i64) rand();

        Lacewing::MD5_Base64 (TempName, TempName, sizeof(TempName));

        char Path[MAX_PATH];
        TempPath(Path, sizeof(Path));

        if(Path[strlen(Path) - 1] != '/')
        {
            Path[strlen(Path) + 1] =   0;
            Path[strlen(Path)]     = '/';
        }

        lw_snprintf(Buffer, Length, "%slw-temp-%s", Path, TempName);
    }
    while(Lacewing::FileExists(Buffer) || !(File = fopen(Buffer, "wb")));

    fclose(File);
}

void Lacewing::MD5 (char * Output, const char * Input, int Length)
{
    if (Length == -1)
        Length = strlen(Input);

    #ifdef LacewingWindows

        static HCRYPTPROV Context = 0;

        if (!Context)
            CryptAcquireContext(&Context, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);

        HCRYPTPROV CryptProv;

        CryptCreateHash (Context, CALG_MD5, 0, 0, &CryptProv);
        CryptHashData (CryptProv, (BYTE *) Input, Length, 0);

        DWORD HashLength = 16;
        CryptGetHashParam (CryptProv, HP_HASHVAL, (BYTE *) Output, &HashLength, 0);

        CryptDestroyHash (CryptProv);

    #else

        MD5_CTX Context;
        MD5_Init(&Context);

        MD5_Update(&Context, Input, Length);

        MD5_Final((unsigned char *) Output, &Context);

    #endif
}

void Lacewing::MD5_Base64 (char * Output, const char * Input, int Length)
{
    MD5 (Output, Input, Length);

    char Base64 [40];
    
    for(int i = 0; i < 16; ++ i)
        sprintf(Base64 + (i * 2), "%02x", ((unsigned char *) Output) [i]);

    strcpy(Output, Base64);
}

void Lacewing::SHA1 (char * Output, const char * Input, int Length)
{
    if (Length == -1)
        Length = strlen(Input);

    #ifdef LacewingWindows

        static HCRYPTPROV Context = 0;

        if (!Context)
            CryptAcquireContext(&Context, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);

        HCRYPTPROV CryptProv;

        CryptCreateHash (Context, CALG_SHA1, 0, 0, &CryptProv);
        CryptHashData (CryptProv, (BYTE *) Input, Length, 0);

        DWORD HashLength = 20;
        CryptGetHashParam (CryptProv, HP_HASHVAL, (BYTE *) Output, &HashLength, 0);

        CryptDestroyHash (CryptProv);

    #else

        SHA_CTX Context;
        SHA1_Init(&Context);

        SHA1_Update(&Context, Input, Length);

        SHA1_Final((unsigned char *) Output, &Context);

    #endif
}

void Lacewing::SHA1_Base64 (char * Output, const char * Input, int Length)
{
    SHA1 (Output, Input, Length);

    char Base64 [48];
    
    for(int i = 0; i < 16; ++ i)
        sprintf(Base64 + (i * 2), "%02x", ((unsigned char *) Output) [i]);

    strcpy(Output, Base64);
}

