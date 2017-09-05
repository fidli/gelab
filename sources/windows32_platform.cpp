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
#include "util_graphics.cpp"
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
    
    struct DrawContext{
        uint32 * drawbuffer;
        Image drawBitmapData;
        Image originalBitmapData;
        BITMAPINFO drawinfo;
        HDC  backbufferDC;
        HBITMAP DIBSection;
        uint32 width;
        uint32 height;
    };
    
    
    
    struct Context{
        HINSTANCE hInstance;
        HWND window;
        bool quit;
        bool mouseOut;
        
    };
    
    
    Context context;
    
    
    DrawContext renderer;
    
#include "domaincode.cpp"
    
    
    void resizeCanvas(HWND window, LONG width, LONG height){
        if(renderer.DIBSection){
            DeleteObject(renderer.DIBSection);
        }else{
            
            renderer.backbufferDC = CreateCompatibleDC(NULL);
        }
        
        renderer.drawinfo = {{
                sizeof(BITMAPINFOHEADER),
                width,
                -height,
                1,
                32,
                BI_RGB,
                0,
                0,
                0,
                0,
                0},
            0
        };
        renderer.DIBSection = CreateDIBSection(renderer.backbufferDC, &renderer.drawinfo, DIB_RGB_COLORS, (void**) &renderer.drawbuffer, NULL, NULL);
        
        renderer.height = height;
        renderer.width = width;
        if(programContext.bitmap.data != NULL){
            scaleImage(&renderer.originalBitmapData, &renderer.drawBitmapData, renderer.width, renderer.height);
        }
        
    }
    
    void updateCanvas(HDC dc, int x, int y, int width, int height){
        StretchDIBits(dc, x, y, width, height, 0, 0, width, height, renderer.drawbuffer, &renderer.drawinfo, DIB_RGB_COLORS, SRCCOPY);
    }
    
    LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam){
        switch(message)
        {
            case WM_SIZE:{
                resizeCanvas(context.window, (WORD)lParam, (WORD) (lParam >> sizeof(WORD) * 8));
            }
            break;
            case WM_NCMOUSEMOVE:{
                context.mouseOut = true;
                
                return 0;
            }break;
            case WM_MOUSEMOVE:{
                
                uint16 x = (WORD) lParam;
                uint16 y = (WORD) (lParam >> 16);
                
                context.mouseOut = false;
                
                
                programContext.mouse.x = x;
                programContext.mouse.y = y;
                return 0;
            }break;
            case WM_LBUTTONUP:{
                uint16 x = (WORD) lParam;
                uint16 y = (WORD) (lParam >> 16);
                programContext.input.click = true;
                programContext.mouse.x = x;
                programContext.mouse.y = y;
                return 0;
            }break;
            case WM_KEYUP:{
                switch(wParam){
                    case VK_BACK:
                    case VK_ESCAPE:{
                        programContext.input.back = true;
                    }break;
                    case VK_LEFT:{
                        programContext.input.left = true;
                    }break;
                    case VK_RIGHT:{
                        programContext.input.right = true;
                    }break;
                    default:{
                    }break;
                };
                return 0;
            }break;
            case WM_PAINT:{
                PAINTSTRUCT paint;
                HDC dc = BeginPaint(window, &paint);
                int x = paint.rcPaint.left;
                int y = paint.rcPaint.top;
                int width = paint.rcPaint.right - paint.rcPaint.left;
                int height = paint.rcPaint.bottom - paint.rcPaint.top;
                
                updateCanvas(dc, x, y, width, height);
                
                EndPaint(context.window, &paint);
            }
            break;
            case WM_CLOSE:
            case WM_DESTROY:
            {
                context.quit = true;
                return 0;
            } break;
        }
        
        return DefWindowProc (window, message, wParam, lParam);
    }
    
    
    
    
    
    
    static inline void printHelp(const char * binaryName){
        printf("\nUsage: %s inputfile [-b num_of_blocks] [-m \"text\" [order]] [-l list beginning_symbol_index -n total_column_count] [-i list] [-I] [-M] [-f] outputfile\n\n", binaryName);
        printf(" inputfile\n  - original TIFF file from experiment\n\n");
        printf(" -b num_of_blocks\n  - valid range [1, 50]\n  - how many experiments are in inputfile - default: 1\n\n"); 
        
        printf(" -m \"text\" [order]\n  - valid 'order' range [1, 255]\n  - find mark on place 'order' and put 'text' next to it, mark wont be added, if this is not set, if -M is set, [order] MUST be omitted as it has no effect\n\n");
        printf(" -l list beginning_symbol_index\n  - valid 'beginning_symbol_range' range [1, 50]\n  - 'list' are ASCII characters (one letter) labeling experiment columns in cycle starting at 'beginning_symbol_index', requires -n to be set too, columns wont be market if this is not set\n\n");
        printf(" -n total_column_count\n  - valid range [1, 255]\n  - sets the total column count that the experiment has\n\n");
        
        printf(" -i list\n  - which blocks in the experiment to output, indexes separated by comma - default: outputs all\n  - valid 'index' range [1, 50]\n\n"); 
        
        printf(" -I\n  - do not invert colors\n\n");
        printf(" -M\n  - crop and outline experiment manually. This option will pop up a window that requires interaction\n\n");
        
        printf(" -f\n  - do not prompt for file overwrite. Overwrite it.\n\n");
        
        
        printf(" outputfile\n  - where to save the ouput to. If file exists, you will be asked whether to overwrite the file.\n\n"); 
        
        printf("Visit gelab.fidli.eu or contact me: info@fidli.eu for questions, feature requests or more.\n");
        
    }
    
    
    static inline int main(LPWSTR * argvW, int argc) {
        
        LPVOID memoryStart = VirtualAlloc(NULL, TEMP_MEM + PERSISTENT_MEM + STACK_MEM, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        
        if (memoryStart) {
            initMemory(memoryStart);
            
            if(initIo()){
                
                if(jettisonAllPrivileges() == ERROR_SUCCESS){
                    
                    
                    char ** argv = &PUSHA(char *, argc);
                    char ** argvUTF8 = &PUSHA(char *, argc);
                    bool success = true;
                    for(int i = 0; i < argc && success; i++){
                        int toAlloc = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);
                        success &= toAlloc != 0;
                        argvUTF8[i] = &PUSHA(char, toAlloc);
                        int res = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, argvUTF8[i], toAlloc, NULL, NULL);
                        success &= res != 0;
                        uint32 finalLen;
                        if(convUTF8toAscii((byte *)argvUTF8[i], toAlloc, &argv[i], &finalLen) != 0){
                            printf("Error: argument is not fully ascii compatible - those characters were replaced by '_'. Please use simple ASCII parameter values\n");
                        }
                    }
                    if(success){
                        
                        RunParameters * parameters = (RunParameters *) mem.persistent;
                        parameters->invertColors = true;
                        parameters->manualCrop = false;
                        parameters->blocksCount = 1;
                        parameters->dontMark = true;
                        parameters->labelsCount = 0;
                        parameters->targetMark = 0;
                        parameters->columns = 0;
                        parameters->skip = 0;
                        parameters->isManual = false;
                        parameters->forceSave = false;
                        for(int32 i = 0; i < ARRAYSIZE(parameters->dontInput); i++){
                            parameters->dontInput[i] = false;
                        }
                        if(argc == 1){
                            printHelp(argv[0]);
                            
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
                                            int16 mark = 4068;
                                            
                                            if(i >= argc - 2 ||sscanf(argv[i+1], "%256%256s", parameters->markText) != 1){
                                                printf("Error: Invalid parameter -m\n");
                                                valid = false;
                                                break;
                                            };
                                            
                                            int8 m = sscanf(argv[i+2], "%hd", &mark);
                                            
                                            if((mark < 1 || mark > 255) && mark != 4068){
                                                printf("Error: Invalid mark order, available range is [1,255]\n");
                                                valid = false;
                                                break;
                                            }
                                            if(mark == 4068){
                                                parameters->targetMark = -1;
                                            }else{
                                                parameters->targetMark = mark-1;
                                            }
                                            parameters->dontMark = false;
                                            i += 1 + m;
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
                                        case 'f':{
                                            parameters->forceSave = true;
                                        }break;
                                        case 'M':{
                                            parameters->isManual = true;
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
                            if(highestBlockList > 0){
                                for(uint8 i = 0; i < parameters->blocksCount; i++){
                                    if(parameters->dontInput[i]){
                                        parameters->skip++;
                                    }
                                }
                            }
                            if(!parameters->dontMark && parameters->targetMark == (uint8)-1 && !parameters->isManual){
                                senseCheck = false;
                                printf("Error: Invalid parameter -m. Please specify mark index you would like to set.\n");
                            }
                            if(parameters->labelsCount > 0 && parameters->columns == 0){
                                senseCheck = false;
                                printf("Error: Please specify also -n when specifying -l\n");
                            }
                            
                            if(!valid || !senseCheck){
                                printf("Error: Invalid arguments, see usage.\n");
                                printHelp(argv[0]);
                            }else{
                                
                                if(!parameters->forceSave && fileExists(parameters->outputfile)){
                                    bool sat = false;
                                    while(!sat){
                                        printf("\nOutput file '%s' exists. Overwrite? (y/n): ", parameters->outputfile);
                                        char reply;
                                        while(scanf("%c", &reply) == 1){
                                            if(reply == 'n'){
                                                sat = true;
                                                context.quit = true;
                                                
                                            }else if(reply == 'y'){
                                                sat = true;
                                                
                                            }
                                            break;
                                        }
                                    }
                                    
                                }
                                
                                if(!context.quit && parameters->isManual){
                                    Image renderingTarget;
                                    Color red;
                                    Color green;
                                    red.full = 0xF0FF0000;
                                    green.full = 0x0000FF00;
                                    
                                    renderingTarget.info.bitsPerSample = 8;
                                    renderingTarget.info.samplesPerPixel = 4;
                                    renderingTarget.info.interpretation = BitmapInterpretationType_ARGB;
                                    renderingTarget.info.origin = BitmapOriginType_TopLeft;
                                    
                                    WNDCLASSEX style = {};
                                    style.cbSize = sizeof(WNDCLASSEX);
                                    style.style = CS_OWNDC;
                                    style.lpfnWndProc = WindowProc;
                                    style.hInstance = context.hInstance;
                                    style.lpszClassName = "MainClass";
                                    if(RegisterClassEx(&style) != 0){
                                        context.window = CreateWindowEx(NULL,
                                                                        "MainClass", "GeLab manual crop | gelab.fidli.eu", WS_OVERLAPPEDWINDOW | WS_SIZEBOX,
                                                                        CW_USEDEFAULT, CW_USEDEFAULT,CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, context.hInstance, NULL);
                                        
                                        if(context.window){
                                            ShowWindow(context.window, SW_SHOWMAXIMIZED);
                                            HCURSOR cursor = (HCURSOR) LoadImage(context.hInstance, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE);
                                            
                                            while (!context.quit) {
                                                
                                                
                                                programContext.input = {};
                                                
                                                MSG msg;
                                                while(PeekMessage(&msg, context.window, 0, 0, PM_REMOVE))
                                                {
                                                    TranslateMessage(&msg);
                                                    DispatchMessage(&msg);
                                                }
                                                
                                                //todo, platform independent input processing
                                                if(programContext.pointsCount == 2 && programContext.state == ProgramState_SelectCorners){
                                                    dv2 lineVec = programContext.points[1] - programContext.points[0];
                                                    dv2 perpVec = {-lineVec.y, lineVec.x};
                                                    v2 unitPerpVec = normalize(perpVec);
                                                    float32 coef = dot({(float32)programContext.mouse.x - programContext.points[0].x, (float32)programContext.mouse.y - programContext.points[0].y}, unitPerpVec);
                                                    v2 perpWorld = coef * unitPerpVec;
                                                    dv2 projcast1 = {(int32) perpWorld.x, (int32)perpWorld.y};
                                                    dv2 projcast2 = projcast1 + programContext.points[1];
                                                    projcast1 += programContext.points[0];
                                                    programContext.line[0] = projcast1;
                                                    programContext.line[1] = projcast2;
                                                }
                                                
                                                if(programContext.state == ProgramState_SelectHalf){
                                                    dv2 lineVec = programContext.sortedPoints[3] - programContext.sortedPoints[0];
                                                    v2 unitVec = normalize(lineVec);
                                                    float32 coef = dot({(float32)programContext.mouse.x - programContext.sortedPoints[0].x, (float32)programContext.mouse.y - programContext.sortedPoints[0].y}, unitVec);
                                                    v2 perpWorld = coef * unitVec;
                                                    dv2 projcast1 = {(int32) perpWorld.x, (int32)perpWorld.y};
                                                    dv2 projcast2 = projcast1 + programContext.sortedPoints[1];
                                                    projcast1 += programContext.sortedPoints[0];
                                                    
                                                    if(projcast1.y < programContext.sortedPoints[0].y && projcast1.y < programContext.sortedPoints[3].y){
                                                        if(programContext.sortedPoints[0].y < programContext.sortedPoints[3].y){
                                                            projcast1 = programContext.sortedPoints[0];
                                                            projcast2 = programContext.sortedPoints[1];
                                                        }else{
                                                            projcast1 = programContext.sortedPoints[3];
                                                            projcast2 = programContext.sortedPoints[2];
                                                        }
                                                    }
                                                    if(projcast1.y > programContext.sortedPoints[3].y && projcast1.y > programContext.sortedPoints[0].y){
                                                        if(programContext.sortedPoints[3].y > programContext.sortedPoints[0].y){
                                                            projcast1 = programContext.sortedPoints[3];
                                                            projcast2 = programContext.sortedPoints[2];
                                                        }else{
                                                            projcast1 = programContext.sortedPoints[0];
                                                            projcast2 = programContext.sortedPoints[1];
                                                        }
                                                    }
                                                    if(projcast1.x < programContext.sortedPoints[0].x && projcast1.x < programContext.sortedPoints[3].x){
                                                        if(programContext.sortedPoints[0].x < programContext.sortedPoints[3].x){
                                                            projcast1 = programContext.sortedPoints[0];
                                                            projcast2 = programContext.sortedPoints[1];
                                                        }else{
                                                            projcast1 = programContext.sortedPoints[3];
                                                            projcast2 = programContext.sortedPoints[2];
                                                        }
                                                    }
                                                    if(projcast1.x > programContext.sortedPoints[3].x && projcast1.x > programContext.sortedPoints[0].x){
                                                        if(programContext.sortedPoints[3].x > programContext.sortedPoints[0].x){
                                                            projcast1 = programContext.sortedPoints[3];
                                                            projcast2 = programContext.sortedPoints[2];
                                                        }else{
                                                            projcast1 = programContext.sortedPoints[0];
                                                            projcast2 = programContext.sortedPoints[1];
                                                        }
                                                    }
                                                    
                                                    
                                                    dv2 offset = projcast1 - programContext.sortedPoints[0];
                                                    dv2 lastProjcast = (projcast1 + ((parameters->blocksCount - 1)*offset));
                                                    v2 newOffset = normalize(offset) * (length(programContext.sortedPoints[3] - programContext.sortedPoints[0])/parameters->blocksCount);
                                                    offset = {(int32) newOffset.x, (int32) newOffset.y};
                                                    
                                                    if(lastProjcast.y < programContext.sortedPoints[0].y && lastProjcast.y < programContext.sortedPoints[3].y){
                                                        if(programContext.sortedPoints[0].y < programContext.sortedPoints[3].y){
                                                            projcast1 = programContext.sortedPoints[3] + offset;
                                                            projcast2 = programContext.sortedPoints[2] + offset;
                                                        }else{
                                                            
                                                            projcast1 = programContext.sortedPoints[0] + offset;
                                                            projcast2 = programContext.sortedPoints[1] + offset;
                                                        }
                                                    }
                                                    if(lastProjcast.y > programContext.sortedPoints[3].y && lastProjcast.y > programContext.sortedPoints[0].y){
                                                        if(programContext.sortedPoints[3].y > programContext.sortedPoints[0].y){
                                                            projcast1 = programContext.sortedPoints[0] + offset;
                                                            projcast2 = programContext.sortedPoints[1] + offset;
                                                        }else{
                                                            projcast1 = programContext.sortedPoints[3] + offset;
                                                            projcast2 = programContext.sortedPoints[2] + offset;
                                                        }
                                                    }
                                                    if(lastProjcast.x < programContext.sortedPoints[0].x && lastProjcast.x < programContext.sortedPoints[3].x){
                                                        if(programContext.sortedPoints[0].x < programContext.sortedPoints[3].x){
                                                            projcast1 = programContext.sortedPoints[3] + offset;
                                                            projcast2 = programContext.sortedPoints[2] + offset;
                                                        }else{
                                                            projcast1 = programContext.sortedPoints[0] + offset;
                                                            projcast2 = programContext.sortedPoints[1] + offset;
                                                        }
                                                    }
                                                    if(lastProjcast.x > programContext.sortedPoints[3].x && lastProjcast.x > programContext.sortedPoints[0].x){
                                                        if(programContext.sortedPoints[3].x > programContext.sortedPoints[0].x){
                                                            projcast1 = programContext.sortedPoints[0] + offset;
                                                            projcast2 = programContext.sortedPoints[1] + offset;
                                                        }else{
                                                            projcast1 = programContext.sortedPoints[3] + offset;
                                                            projcast2 = programContext.sortedPoints[2] + offset;
                                                        }
                                                    }
                                                    programContext.line[0] = projcast1;
                                                    programContext.line[1] = projcast2;
                                                    
                                                }else if(programContext.state == ProgramState_SelectMarks){
                                                    
                                                    dv2 lineVec = programContext.sortedPoints[3] - programContext.sortedPoints[0];
                                                    v2 unitVec = normalize(lineVec);
                                                    float32 coef = dot({(float32)programContext.mouse.x - programContext.sortedPoints[0].x, (float32)programContext.mouse.y - programContext.sortedPoints[0].y}, unitVec);
                                                    v2 perpWorld = coef * unitVec;
                                                    dv2 projcast1 = {(int32) perpWorld.x, (int32)perpWorld.y};
                                                    projcast1 += programContext.sortedPoints[0];
                                                    if(projcast1.y > programContext.half[0].y && programContext.half[0].y > programContext.sortedPoints[0].y){
                                                        projcast1 = programContext.half[0];
                                                        
                                                    }
                                                    if(projcast1.y < programContext.half[0].y && programContext.half[0].y < programContext.sortedPoints[0].y){
                                                        projcast1 = programContext.half[0];
                                                        
                                                    }
                                                    if(projcast1.y < programContext.sortedPoints[0].y && programContext.sortedPoints[0].y < programContext.half[0].y){
                                                        projcast1 = programContext.sortedPoints[0];
                                                        
                                                    }
                                                    if(projcast1.y > programContext.sortedPoints[0].y && programContext.sortedPoints[0].y > programContext.half[0].y){
                                                        projcast1 = programContext.sortedPoints[0];
                                                        
                                                    }
                                                    
                                                    if(projcast1.x > programContext.half[0].x && programContext.half[0].x > programContext.sortedPoints[0].x){
                                                        projcast1 = programContext.half[0];
                                                        
                                                    }
                                                    if(projcast1.x < programContext.half[0].x && programContext.half[0].x < programContext.sortedPoints[0].x){
                                                        projcast1 = programContext.half[0];
                                                        
                                                    }
                                                    if(projcast1.x < programContext.sortedPoints[0].x && programContext.sortedPoints[0].x < programContext.half[0].x){
                                                        projcast1 = programContext.sortedPoints[0];
                                                        
                                                    }
                                                    if(projcast1.x > programContext.sortedPoints[0].x && programContext.sortedPoints[0].x > programContext.half[0].x){
                                                        projcast1 = programContext.sortedPoints[0];
                                                        
                                                    }
                                                    v2 cast = 0.1f *(programContext.sortedPoints[1] - programContext.sortedPoints[0]);
                                                    dv2 projcast2 = projcast1 + DV2((int32)cast.x, (int32)cast.y); 
                                                    programContext.line[0] = projcast1;
                                                    programContext.line[1] = projcast2;
                                                }
                                                
                                                if(context.mouseOut){
                                                    if(GetCursor() == NULL){
                                                        cursor = SetCursor(cursor);
                                                    }
                                                }else{
                                                    if(GetCursor() != NULL){
                                                        cursor = SetCursor(NULL);
                                                    }
                                                }
                                                
                                                
                                                
                                                
                                                
                                                runManual(parameters);
                                                
                                                
                                                renderingTarget.info.width = renderer.width;
                                                renderingTarget.info.height = renderer.height;
                                                renderingTarget.data = (byte *)renderer.drawbuffer;
                                                
                                                //todo: separate rendering routines before porting to other machines
                                                
                                                /**
                                                draw basic image
                                                */
                                                for(uint32 h = 0; h < renderer.height; h++){
                                                    uint32 pitch = h*renderer.width;
                                                    
                                                    for(uint32 w = 0; w < renderer.width; w++){
                                                        
                                                        ((uint32 *)renderingTarget.data)[pitch + w] = ((uint32 *)renderer.drawBitmapData.data)[pitch + w];
                                                        
                                                        
                                                    }
                                                }
                                                
                                                if(programContext.pointsCount <= 1){
                                                    dv2 tmp1 = {(int32)programContext.mouse.x, 0};
                                                    dv2 tmp2 = {(int32)programContext.mouse.x, (int32)renderingTarget.info.height};
                                                    drawLine(&renderingTarget, &tmp1, &tmp2, green);
                                                    tmp1 = {0, (int32)programContext.mouse.y};
                                                    tmp2 = {(int32)renderingTarget.info.width, (int32)programContext.mouse.y};
                                                    drawLine(&renderingTarget, &tmp1, &tmp2, green);
                                                }
                                                
                                                
                                                if(programContext.pointsCount  == 1){
                                                    drawLine(&renderingTarget, &programContext.points[0], &programContext.mouse, red);
                                                }else if(programContext.pointsCount == 2){
                                                    drawLine(&renderingTarget, &programContext.points[0], &programContext.points[1], green);
                                                    
                                                    drawLine(&renderingTarget, &programContext.points[0], &programContext.line[0], green);
                                                    drawLine(&renderingTarget, &programContext.points[1], &programContext.line[1], green);
                                                    drawLine(&renderingTarget, &programContext.line[0], &programContext.line[1], green);
                                                    
                                                    
                                                }else if(programContext.pointsCount == 4){
                                                    //round and round we go
                                                    drawLine(&renderingTarget, &programContext.sortedPoints[0], &programContext.sortedPoints[1], green);
                                                    drawLine(&renderingTarget, &programContext.sortedPoints[1], &programContext.sortedPoints[2], green);
                                                    drawLine(&renderingTarget, &programContext.sortedPoints[2], &programContext.sortedPoints[3], green);
                                                    drawLine(&renderingTarget, &programContext.sortedPoints[3], &programContext.sortedPoints[0], green);
                                                    
                                                    
                                                    
                                                    if(programContext.state >= ProgramState_SelectHalf){
                                                        
                                                        dv2 line[2];
                                                        
                                                        Color currentColor;
                                                        
                                                        if(programContext.state == ProgramState_SelectHalf){
                                                            currentColor = red;
                                                            line[0] = programContext.line[0];
                                                            line[1] = programContext.line[1];
                                                        }else{
                                                            currentColor = green;
                                                            line[0] = programContext.half[0];
                                                            line[1] = programContext.half[1];
                                                        }
                                                        
                                                        float32 width = length(programContext.sortedPoints[1] - programContext.sortedPoints[0]);
                                                        float32 height = length(programContext.sortedPoints[3] - programContext.sortedPoints[0]);
                                                        dv2 arrow[3] = {programContext.sortedPoints[0], programContext.sortedPoints[0], programContext.sortedPoints[0]};
                                                        v2 xOffset = normalize(programContext.sortedPoints[1] - programContext.sortedPoints[0]) * (width / 10.0f);
                                                        v2 yOffset = normalize(programContext.sortedPoints[3] - programContext.sortedPoints[0]) * (height / 10.0f);
                                                        
                                                        arrow[0] += {(int32)xOffset.x, (int32)xOffset.y};
                                                        arrow[1] += {(int32)(1.5f*xOffset.x), (int32)(1.5f*xOffset.y)};
                                                        arrow[2] += {(int32)(2*xOffset.x), (int32)(2*xOffset.y)};
                                                        
                                                        arrow[0] += {(int32)yOffset.x, (int32)yOffset.y};
                                                        arrow[1] += {(int32)(2*yOffset.x), (int32)(2*yOffset.y)};
                                                        arrow[2] += {(int32)(yOffset.x), (int32)(yOffset.y)};
                                                        
                                                        drawLine(&renderingTarget, &arrow[0], &arrow[1] ,green);
                                                        drawLine(&renderingTarget, &arrow[2], &arrow[1] ,green);
                                                        
                                                        
                                                        dv2 pt1;
                                                        dv2 pt2;
                                                        for(uint8 i = 0; i < parameters->blocksCount; i++){
                                                            dv2 shift = i * (line[0] - programContext.sortedPoints[0]);
                                                            
                                                            pt1 = line[0] + shift;
                                                            pt2 = line[1] + shift;
                                                            if(i == 0 && programContext.state == ProgramState_SelectHalf){
                                                                drawLine(&renderingTarget, &pt1, &pt2, currentColor, 5);
                                                            }else{
                                                                drawLine(&renderingTarget, &pt1, &pt2, currentColor);
                                                            }
                                                            if(parameters->dontInput[i]){
                                                                dv2 shift2 = (i - 1) * (line[0] - programContext.sortedPoints[0]);
                                                                dv2 pt1p = line[0] + shift2;
                                                                dv2 pt2p = line[1] + shift2;
                                                                drawLine(&renderingTarget, &pt1p, &pt2, currentColor);
                                                                drawLine(&renderingTarget, &pt2p, &pt1, currentColor);
                                                            }
                                                        }
                                                        
                                                        drawLine(&renderingTarget, &pt1, &programContext.sortedPoints[2], currentColor);
                                                        drawLine(&renderingTarget, &pt2, &programContext.sortedPoints[3], currentColor);
                                                        
                                                    }
                                                    if(programContext.state > ProgramState_SelectHalf){
                                                        for(uint8 i = 0; i < parameters->blocksCount; i++){
                                                            if(!parameters->dontInput[i]){
                                                                dv2 shift = i * (programContext.half[0] - programContext.sortedPoints[0]);
                                                                dv2 pt1 = programContext.line[0] + shift;
                                                                dv2 pt2 = programContext.line[1] + shift;
                                                                drawLine(&renderingTarget, &pt1, &pt2, red);
                                                            }
                                                        }
                                                    }
                                                    
                                                }
                                                
                                                //draw cursor at mouse
                                                if(programContext.state == ProgramState_SelectHalf){
                                                    uint8 cursize = 32;
                                                    v2 vec = cursize*normalize(programContext.sortedPoints[1] - programContext.sortedPoints[0]);
                                                    dv2 pt1 = programContext.mouse - DV2((int32)vec.x, (int32)vec.y);
                                                    dv2 pt2 = programContext.mouse + DV2((int32)vec.x, (int32)vec.y);
                                                    drawLine(&renderingTarget, &pt1, &pt2,  red, 5);
                                                    pt1 = programContext.mouse;
                                                    vec = cursize *normalize(programContext.sortedPoints[3] - programContext.sortedPoints[0]);
                                                    pt2 = programContext.mouse + DV2((int32)vec.x, (int32)vec.y);
                                                    drawLine(&renderingTarget, &pt1, &pt2,  red, 5);
                                                    
                                                }
                                                
                                                InvalidateRect(context.window, NULL, TRUE);                       
                                            }
                                            
                                            
                                            
                                            
                                            
                                            
                                        }else{
                                            printf("Error: System failed to create window for manual cropping.\n"); 
                                            ASSERT(!"failed to create window");
                                        }
                                        
                                        UnregisterClass("MainClass", context.hInstance);
                                    }
                                    
                                }else{
                                    if(!context.quit){
                                        runAutomatic(parameters);
                                    }
                                }
                            }
                        }
                    }else{
                        //system argument parsing failed
                        printf("Error: System failed to parse CLI arguments\n");
                        ASSERT(false);
                    }
                }else{
                    printf("Error: System failed to lower priviledges\n");
                    ASSERT(false);
                    //failed to jettison all priv
                }
            }else{
                //failed to init io
                char buf[] = "System failed to init console.\n";
                FileContents error;
                error.contents = buf;
                error.size = strlen(buf);
                appendFile("error.log", &error);
                ASSERT(false);
            }
            
            if (!VirtualFree(memoryStart, 0, MEM_RELEASE)) {
                //more like log it
                char buf[] = "System failed to release memory.\n";
                FileContents error;
                error.contents = buf;
                error.size = strlen(buf);
                appendFile("error.log", &error);
                ASSERT(!"Failed to free memory");
            }
            
            
            
        }else{
            //more like log it
            char buf[] = "System failed to init memory.\n";
            FileContents error;
            error.contents = buf;
            error.size = strlen(buf);
            appendFile("error.log", &error);
            ASSERT(!"failed to init memory");
        }
        return 0;
        
        
    }
    
    
    
    int mainCRTStartup(){
        //this might fail, consider, whether stop the app, or continue
        SetProcessDEPPolicy(PROCESS_DEP_ENABLE);
        int argc = 0;
        LPWSTR * argv =  CommandLineToArgvW(GetCommandLineW(), &argc);
        context.hInstance = GetModuleHandle(NULL);
        context.quit = false;
        renderer.drawBitmapData = {};
        programContext.state = ProgramState_Init;
        programContext.pointsCount = 0;
        programContext.input = {};
        
        int result = main(argv,argc);
        LocalFree(argv);
        ExitProcess(result);
    }
    
