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
    
    
    
#define PERSISTENT_MEM MEGABYTE(1)
#define TEMP_MEM MEGABYTE(200)
#define STACK_MEM MEGABYTE(100)
    
    
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
    
    static inline void printHelp(const char * binaryName){
        printf("\nUsage: %s inputfile [-b num_of_blocks] [-m order \"text\"] [-l list beginning_symbol_index -n total_column_count] [-i list] [-IM] outputfile\n\n", binaryName);
        printf(" inputfile\n  - original TIFF file from experiment\n\n");
        printf(" -b num_of_blocks\n  - valid range [1, 50]\n  - how many experiments are in inputfile - default: 1\n\n"); 
        
        printf(" -m order \"text\"\n  - valid 'order' range [1, 255]\n  - find mark on place 'order' and put 'text' next to it, mark wont be added, if this is not set\n\n");
        printf(" -l list beginning_symbol_index\n  - valid 'beginning_symbol_range' range [1, 50]\n  - 'list' are ASCII characters (one letter) labeling experiment columns in cycle starting at 'beginning_symbol_index', requires -n to be set too, columns wont be market if this is not set\n\n");
        printf(" -n total_column_count\n  - valid range [1, 255]\n  - sets the total column count that the experiment has\n\n");
        
        printf(" -i list\n  - which blocks in the experiment to output, indexes separated by comma - default: outputs all\n  - valid 'index' range [1, 50]\n\n"); 
        
        printf(" -I\n  - do not invert colors\n\n");
        printf(" -M\n  - crop and outline experiment manually. This option will pop up a window that requires interaction\n\n");
        
        printf(" outputfile\n  - where to save the ouput to. THIS FILE WILL BE OVERWRITTEN\n"); 
        
    }
    
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
            
            RunParameters * parameters = (RunParameters *) mem.persistent;
            parameters->invertColors = true;
            parameters->manualCrop = false;
            parameters->blocksCount = 1;
            parameters->dontMark = true;
            parameters->labelsCount = 0;
            parameters->targetMark = 0;
            parameters->columns = 0;
            for(int32 i = 0; i < ARRAYSIZE(parameters->dontInput); i++){
                parameters->dontInput[i] = false;
            }
            if(argc == 1){
                printHelp(argv[0]);
                /*
                strcpy(parameters->inputfile, "data/160907_gel1.tif");
                strcpy(parameters->outputfile, "crea.tif");
                parameters->columns = 24;
                parameters->targetMark = 5;
                run(parameters);
                */
            }else{
                bool valid = true;
                uint8 highestBlockList = 0;
                if(argc >= 3){
                    
                    strcpy(parameters->inputfile, argv[1]);
                    strcpy(parameters->outputfile, argv[argc-1]);
                    
                    for(uint8 i = 2; i < argc - 1 && valid; i++){
                        if(argv[i][0] != '-'){
                            valid = false;
                            break;
                        }
                        switch(argv[i][1]){
                            case 'b':{
                                int16 blocks;
                                if(i == argc - 2 || sscanf(argv[i+1], "%hd", &blocks) != 1){
                                    printf("Error: Invalid parameter -b\n");
                                    valid = false;
                                    break;
                                };
                                if(blocks < 1 || blocks > 50){
                                    printf("Error: Invalid blocks count, available range is [1,50]\n");
                                    valid = false;
                                    break;
                                }
                                parameters->blocksCount = blocks;
                                i++;
                            }break;
                            case 'm':{
                                int16 mark;
                                if(i == argc - 3 || sscanf(argv[i+1], "%hd", &mark) != 1 || sscanf(argv[i+2], "%256%256s", parameters->markText) != 1){
                                    printf("Error: Invalid parameter -m\n");
                                    valid = false;
                                    break;
                                };
                                if(mark < 1 || mark > 255){
                                    printf("Error: Invalid mark order, available range is [1,255]\n");
                                    valid = false;
                                    break;
                                }
                                parameters->targetMark = mark-1;
                                parameters->dontMark = false;
                                i += 2;
                            }break;
                            case 'l':{
                                int16 index;
                                char buffer[256];
                                if(i == argc - 3 || sscanf(argv[i+2], "%hd", &index) != 1 || sscanf(argv[i+1], "%256%256s", buffer) != 1){
                                    printf("Error: Invalid parameter -l\n");
                                    valid = false;
                                    break;
                                };
                                if(index < 1 || index > 50){
                                    printf("Error: Invalid beginning_symbol_index, available range is [1,50]\n");
                                    valid = false;
                                    break;
                                }
                                uint32 j = 0;
                                for(;buffer[j] != '\0'; j++){
                                    if(parameters->labelsCount == 50){
                                        printf("Error: Labeling list exceeded count of 50.\n");
                                        valid = false;
                                        break;
                                    }
                                    parameters->labels[parameters->labelsCount] = buffer[j];
                                    parameters->labelsCount++;
                                }
                                
                                if(index > parameters->labelsCount){
                                    printf("Error: Invalid beginning_symbol_index, its bigger than label list size\n");
                                    valid = false;
                                    break;
                                }
                                
                                parameters->beginningSymbolIndex = index-1;
                                i += 2;
                            }break;
                            case 'n':{
                                int16 cols;
                                if(i == argc - 2 || sscanf(argv[i+1], "%hd", &cols) != 1){
                                    printf("Error: Invalid parameter -n\n");
                                    valid = false;
                                    break;
                                };
                                if(cols < 1 || cols > 255){
                                    printf("Error: inalid columns amount, available range is [1,255]\n");
                                    valid = false;
                                    break;
                                }
                                parameters->columns = cols;
                                i++;
                            }break;
                            case 'i':{
                                char buffer[256];
                                if(i == argc - 2 || sscanf(argv[i+1], "%256%256s", buffer) != 1){
                                    printf("Error: Invalid parameter -i\n");
                                    valid = false;
                                    break;
                                };
                                
                                for(int32 j = 0; j < ARRAYSIZE(parameters->dontInput); j++){
                                    parameters->dontInput[j] = true;
                                }
                                
                                uint32 offset = 0;
                                int32 currentNumber;
                                while(sscanf(buffer + offset, "%256%d", &currentNumber) == 1){
                                    if(currentNumber < 1 || currentNumber > 50){
                                        printf("Error: Invalid block list index, available range is [1,50]\n");
                                        valid = false;
                                        break;
                                    }
                                    if(currentNumber > highestBlockList){
                                        highestBlockList = currentNumber;
                                    }
                                    parameters->dontInput[currentNumber-1] = false;
                                    offset += numlen(currentNumber) + 1;
                                    if(*(buffer + offset - 1) != ',' && *(buffer + offset - 1) != '\0'){
                                        printf("Error: Invalid block list separator\n");
                                        valid = false;
                                        break;
                                    }
                                }
                                
                                i++;
                            }break;
                            case 'I':{
                                parameters->invertColors = false;
                            }break;
                            case 'M':{
                                //manual crop
                            }break;
                            default:{
                                valid = false;
                                break;
                            }break;
                        }
                        
                    }
                }else{
                    valid = false;
                }
                bool senseCheck = true;
                
                if(highestBlockList > parameters->blocksCount){
                    senseCheck = false;
                    printf("Error: Block list contains block with higher index than block count\n");
                }
                
                if(parameters->labelsCount > 0 && parameters->columns == 0){
                    senseCheck = false;
                    printf("Error: Please specify also -n when specifying -l\n");
                }
                
                if(!valid || !senseCheck){
                    printf("Error: Invalid arguments, see usage.\n");
                    printHelp(argv[0]);
                }else{
                    run(parameters);
                }
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
    
