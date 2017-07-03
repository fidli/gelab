    extern "C" void * __cdecl memset(void *, int, size_t);
#pragma intrinsic(memset)
    extern "C" void * __cdecl memcpy(void *, const void *, size_t);
#pragma intrinsic(memcpy)
    
    extern "C"{
#pragma function(memset)
        void * memset(void * dest, int c, size_t count)
        {
            char * bytes = (char *) dest;
            while (count--)
            {
                (*bytes) = (char) c;
                (*bytes++);
            }
            return dest;
        }
        
#pragma function(memcpy)
        void *memcpy(void *dest, const void *src, size_t count)
        {
            char *dest8 = (char *)dest;
            const char *src8 = (const char *)src;
            while (count--)
            {
                *dest8++ = *src8++;
            }
            return dest;
        }
    }
    extern "C"{
        int _fltused;
    };
#include "windows32_64inst.cpp"
    
#include <Windows.h>
    
#include "windows_types.h"
#include "common.h"
    
    
    
#define PERSISTENT_MEM MEGABYTE(0)
#define TEMP_MEM MEGABYTE(200)
#define STACK_MEM MEGABYTE(10)
    
    
#include "util_mem.h"
#include "util_io.cpp"
#include "util_string.cpp"
#include "windows_io.cpp"
#include "windows_filesystem.cpp"
#include "util_image.cpp"
#include "util_conv.cpp"
    
    
    static inline DWORD jettisonAllPrivileges() {
        DWORD result = ERROR_SUCCESS;
        HANDLE processToken  = NULL;
        if(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &processToken)){
            DWORD privsize = 0;
            GetTokenInformation(processToken, TokenPrivileges, NULL, 0, &privsize);
            TOKEN_PRIVILEGES * priv = (TOKEN_PRIVILEGES *) &PUSHS(byte, privsize);
            if(GetTokenInformation(processToken, TokenPrivileges, priv , privsize, &privsize)){
                
                for(DWORD i = 0; i < priv->PrivilegeCount; ++i ){ 
                    priv->Privileges[i].Attributes = SE_PRIVILEGE_REMOVED;
                }
                if(AdjustTokenPrivileges(processToken, TRUE, priv, NULL, NULL, NULL) == 0){
                    result = GetLastError();
                }
            }else{
                result = GetLastError();
            }
            POP;
        }else{
            result = GetLastError();
        }
        CloseHandle(processToken);
        return result;
    }
    
#include "domaincode.cpp"
    
    static inline int main(LPWSTR * argvW, int argc) {
        
        LPVOID memoryStart = VirtualAlloc(NULL, TEMP_MEM + PERSISTENT_MEM + STACK_MEM, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        
        if (memoryStart) {
            initMemory(memoryStart);
            bool ioResult = initIo();
            ASSERT(ioResult);
            HRESULT jettisonPrivilegesResult = jettisonAllPrivileges();
            ASSERT(jettisonPrivilegesResult == ERROR_SUCCESS);
            
            
            char ** argv = &PUSHA(char *, argc);
            char ** argvUTF8 = &PUSHA(char *, argc);
            
            for(int i = 0; i < argc; i++){
                int toAlloc = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);
                ASSERT(toAlloc != 0);
                argvUTF8[i] = &PUSHA(char, toAlloc);
                int res = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, argvUTF8[i], toAlloc, NULL, NULL);
                ASSERT(res != 0);
                uint32 finalLen;
                if(convUTF8toAscii((byte *)argvUTF8[i], toAlloc, &argv[i], &finalLen) != 0){
                    printf("Warning: file name is not fully ascii compatible - those characters were replaced by '_'. Please use simple name for path/file \n");
                }
            }
            
            RunParameters parameters;
            if(argc == 1){
                printf("Usage:\n");
                printf("%s filename\n", argv[0]);
                printf(" filename - original TIFF file from measurement\n"); 
                strcpy(parameters.file, "data/160907_gel2.tif");
                parameters.columns = 24;
                parameters.targetMark = 5;
                run(&parameters);
            }else{
                strcpy(argv[0], parameters.file);
                run(&parameters);
            }
            
            
            
            if (!VirtualFree(memoryStart, 0, MEM_RELEASE)) {
                //more like log it
                ASSERT(!"Failed to free memory");
            }
            
            
        }else{
            //more like log it
            ASSERT(!"failed to init memory");
        }
        return 0;
        
        
    }
    
    
    
    int mainCRTStartup(){
        //this might fail, consider, whether stop the app, or continue
        SetProcessDEPPolicy(PROCESS_DEP_ENABLE);
        int argc = 0;
        LPWSTR * argv =  CommandLineToArgvW(GetCommandLineW(), &argc);
        
        int result = main(argv,argc);
        LocalFree(argv);
        ExitProcess(result);
    }
    
