    #include "util_filesystem.h"
#include "util_font.cpp"
#include "util_math.cpp"
#include "util_sort.cpp"
    
    enum ProgramState{
        ProgramState_Init,
        ProgramState_SelectCorners,
        ProgramState_SelectHalf,
        ProgramState_SelectMarks,
        ProgramState_Shutdown
    };
    
    
    struct UserInput{
        bool click;
        bool back;
    };
    
    
    struct ProgramContext{
        ProgramState state;
        Image bitmap;
        dv2 mouse;
        dv2 line[2];
        dv2 points[4];
        dv2 sortedPoints[4];
        dv2 half[2];
        uint8 pointsCount;
        UserInput input;
        v2 rotation;
        v2 rotationCenter;
        dv2 mark;
        uint32 blockSize;
    };
    
    
    ProgramContext programContext;
    
    
    struct RunParameters{
        char inputfile[256];
        char outputfile[256];
        uint8 columns;
        uint8 targetMark;
        char markText[256];
        uint8 blocksCount;
        bool invertColors;
        bool manualCrop;
        bool dontInput[50];
        char labels[50];
        uint8 beginningSymbolIndex;
        
        uint8 labelsCount;
        
        bool dontMark;
        bool isManual;
    };
    
    struct PixelGradient{
        uint8 direction;
        uint8 size;
    };
    
    struct TileCoord{
        uint32 x;
        uint32 y;
        uint32 streak;
    };
    
    void moveImageBlockUp(Image * target, uint32 fromY, uint32 toY, uint32 howMuch){
        ASSERT(target->info.bitsPerSample * target->info.samplesPerPixel == 8);
        ASSERT(toY < fromY);
        if(howMuch + fromY > target->info.height){
            howMuch = target->info.height - fromY;
        }
        for(uint32 h = 0; h < howMuch; h++){
            for(uint32 w = 0; w < target->info.width; w++){
                target->data[(toY + h) * target->info.width + w] = target->data[(fromY + h) * target->info.width + w];
            }
        }
        target->info.height -= toY-fromY;
    }
    
    bool openFiles(const RunParameters * parameters){
        FileContents imageFile;
        
        bool result = readFile(parameters->inputfile, &imageFile);
        ASSERT(result);
        if(result == false){
            printf("Error: failed to read file contents\n");
            context.quit = true;
            return false;
        }
        result = decodeTiff(&imageFile, &programContext.bitmap);
        ASSERT(result);
        if(result == false){
            printf("Error: failed to decode tiff file\n");
            context.quit = true;
            return false;
        }
        
        ASSERT(programContext.bitmap.info.bitsPerSample * programContext.bitmap.info.samplesPerPixel == 8);
        if(programContext.bitmap.info.bitsPerSample * programContext.bitmap.info.samplesPerPixel != 8 || programContext.bitmap.info.interpretation != BitmapInterpretationType_GrayscaleBW01){
            printf("Error: invalid image format (is not grayscale?)\n");
            context.quit = true;
            return false;
        }
        return true;
    }
    
    bool saveFiles(const RunParameters * parameters){
        
        FileContents tiff;
        bool result = encodeTiff(&programContext.bitmap, &tiff);
        if(!result){
            printf("Error: failed to encode image into the TIF.\n");
            context.quit = true;
            return false;
        }
        result = saveFile(parameters->outputfile, &tiff);
        if(!result){
            printf("Error: System failed to save image into the file.\n");
            context.quit = true;
            return false;
        }
        
        return true;
    }
    
    bool postProcess(const RunParameters * parameters){
        /*
        bool result;
        if(!parameters->dontMark){
            result = scaleCanvas(&programContext.bitmap, programContext.bitmap.info.width + markOffset, programContext.bitmap.info.height, markOffset, 0);
            if(!result){
                printf("Error: IP failed to scale canvas.\n");
                context.quit = true;
                return false;
            }
        }
        
        markOffset = fontSize * strlen(parameters->markText);
        
        
        
        
        
        result = printToBitmap(&programContext.bitmap, 0, mark.startY, parameters->markText, &font, fontSize);
        if(!result){
            printf("Error: failed to print mark to the bitmap.\n");
            context.quit = true;
            return;
        }
        for(uint8 i = 0; i < parameters->blocksCount; i++){
        
        }
        
        if(parameters->invertColors){
            for(uint32 i = 0; i < programContext.bitmap.info.width * programContext.bitmap.info.height * programContext.bitmap.info.bitsPerSample * programContext.bitmap.info.samplesPerPixel; i++){
                programContext.bitmap.data[i] = 255 - programContext.bitmap.data[i]; 
            }
        }
        */
        return true;
    }
    
    void runAutomatic(RunParameters * parameters){
        
        if(!openFiles(parameters)){
            return;
        }
        
        uint8 thresholdX = 20;
        uint8 thresholdY = 4;
        int32 wj = programContext.bitmap.info.width / 10;
        
        
        //contrast the image
        uint32 averageIntensity = 0;
        for(uint32 w = 0; w < programContext.bitmap.info.width; w++){
            for(uint32 h = 0; h < programContext.bitmap.info.height; h++){
                averageIntensity += programContext.bitmap.data[w + h*programContext.bitmap.info.width];
            }
        }
        
        averageIntensity /= programContext.bitmap.info.width * programContext.bitmap.info.height;
        float32 dst = (float32)(20 - averageIntensity);
        applyContrast(&programContext.bitmap, -1.5f * dst);
        
        
        /*
        FileContents bmp2;
        encodeBMP(&programContext.bitmap, &bmp2);
        saveFile("bmp.bmp", &bmp2);
        return;
        */
        
        
        
        uint8 rotatingSamples = 5;
        
        {
            PUSHI;
            v2 * rotations = &PUSHA(v2, rotatingSamples);
            v2 * centers = &PUSHA(v2, rotatingSamples);
            uint16 rotationCount = 0;
            int32 rowH = (int32)(programContext.bitmap.info.height * 0.9) / rotatingSamples;
            for(uint8 rsi = 0; rsi < rotatingSamples; rsi++){
                //sample 1
                bool foundTop = false;
                bool foundBot = false;
                int32 h1 = (int32)(programContext.bitmap.info.height * 0.1) + rsi*rowH;
                int32 h2 = h1 + rowH;
                
                
                int32 w1, w2;
                
                for(uint32 w = 0; w < programContext.bitmap.info.width && (!foundBot || !foundTop); w++){
                    if(!foundTop){
                        if(programContext.bitmap.data[h1 * programContext.bitmap.info.width + w] >= thresholdX && programContext.bitmap.data[h1 * programContext.bitmap.info.width + w + wj] >= thresholdX){
                            foundTop = true;
                            w1= w;
                        }
                    }
                    if(!foundBot){
                        if(programContext.bitmap.data[h2 * programContext.bitmap.info.width + w] >= thresholdX && programContext.bitmap.data[h2 * programContext.bitmap.info.width + w + wj] >= thresholdX){
                            foundBot = true;
                            w2= w;
                        }
                    }
                }
                
                if(foundBot && foundTop){
                    rotations[rotationCount] = normalize(V2((float32)(w2 - w1),(float32)(h2 - h1)));
                    centers[rotationCount] = V2((float32)w1/programContext.bitmap.info.width, (float32)h1/programContext.bitmap.info.height);
                    rotationCount++;
                }
            }
            
            ASSERT(rotationCount>0);
            if(rotationCount == 0){
                printf("Error: CV failed to analyze rotation\n");
                context.quit = true;
                return;
            }
            
            
            v2 rotation = {};
            v2 center = {};
            
            uint8 biggestCluster = 0;
            
            //take the most similar vectors
            for(uint8 ri = 0; ri < rotationCount; ri++){
                uint8 currentCluster = 0;
                v2 currentRotation = {};
                v2 currentCenter = {};
                for(uint8 ri2 = 0; ri2 < rotationCount; ri2++){
                    if(ri2 != ri){
                        //normalized, dot = len * len * cos
                        //therefore dot = cosine
                        //looking for it to be near 1 - since cos of 0 deg is 1
                        if(dot(rotations[ri], rotations[ri2]) > 0.93){
                            currentCenter += centers[ri];
                            currentRotation += rotations[ri];
                            currentCluster++;
                        }
                    }
                }
                if(currentCluster > biggestCluster){
                    center = currentCenter;
                    rotation = currentRotation;
                    center *= 1.0/currentCluster;
                    rotation *= 1.0/currentCluster;
                    biggestCluster = currentCluster;
                }
            }
            
            ASSERT(biggestCluster > 0);
            if(biggestCluster == 0){
                printf("Error: CV failed to recognize rotation, too many different guesses.\n");
                context.quit = true;
                return;
            }
            
            programContext.rotation = rotation;
            programContext.rotationCenter = center;
            
            POPI;
            
            v2 down = V2(0, 1);
            float32 degAngle = radAngle(programContext.rotation, down) * 180 / PI;
            if(det(programContext.rotation, down) < 0){
                degAngle *= -1;
            }
            
            rotateImage(&programContext.bitmap, degAngle, programContext.rotationCenter.x, programContext.rotationCenter.y);
            
            
            
            
            /*
            FileContents bmp2;
            encodeBMP(&programContext.bitmap, &bmp2);
            saveFile("bmp.bmp", &bmp2);
            return;
            */
            
            {
                //crop
                uint32 lX, rX, tY, bY;
                int32 h = programContext.bitmap.info.height / 2;
                uint32 w = 0;
                for(; w < programContext.bitmap.info.width; w++){
                    if(programContext.bitmap.data[h * programContext.bitmap.info.width + w] >= thresholdX && programContext.bitmap.data[h * programContext.bitmap.info.width + w + wj] >= thresholdX){
                        break;
                    }
                    
                }
                lX = w;
                
                for(w = programContext.bitmap.info.width - 1; w >= 0; w--){
                    if(programContext.bitmap.data[h * programContext.bitmap.info.width + w] >= thresholdX && programContext.bitmap.data[h * programContext.bitmap.info.width + w - wj] >= thresholdX){
                        break;
                    }
                    
                }
                rX = w;
                bool result = cropImageX(&programContext.bitmap, lX, rX);
                ASSERT(result);
                if(result == false){
                    printf("Error: IP failed to crop image\n");
                    context.quit = true;
                    return;
                }
                
                uint32 w1 = (uint32)((float32)programContext.bitmap.info.width * 0.1f);
                uint32 w2 = (uint32)((float32)programContext.bitmap.info.width * 0.9f);
                int32 hj = programContext.bitmap.info.height / 10;
                
                bool foundLeft = false;
                bool foundRight = false;
                
                h = 0;
                for(; h < programContext.bitmap.info.height && (!foundLeft || !foundRight); h++){
                    if(!foundRight){
                        if(programContext.bitmap.data[h * programContext.bitmap.info.width + w2] >= thresholdY && programContext.bitmap.data[(h + hj) * programContext.bitmap.info.width + w2] >= thresholdY){
                            foundRight = true;
                            
                        }
                    }
                    if(!foundLeft){
                        if(programContext.bitmap.data[h * programContext.bitmap.info.width + w1] >= thresholdY && programContext.bitmap.data[(h + hj) * programContext.bitmap.info.width + w1] >= thresholdY){
                            foundLeft = true;
                        }
                    }
                }
                
                ASSERT(foundLeft && foundRight);
                if(foundLeft == false || foundLeft == false){
                    printf("Error: CV failed to find image borders to crop\n");
                    context.quit = true;
                    return;
                }
                tY = h;
                
                foundRight = foundLeft = false;
                h = programContext.bitmap.info.height - 1;
                for(; h >= 0 && (!foundLeft || !foundRight); h--){
                    if(!foundRight){
                        if(programContext.bitmap.data[h * programContext.bitmap.info.width + w2] >= thresholdY && programContext.bitmap.data[(h - hj) * programContext.bitmap.info.width + w2] >= thresholdY){
                            foundRight = true;
                            
                        }
                    }
                    if(!foundLeft){
                        if(programContext.bitmap.data[h * programContext.bitmap.info.width + w1] >= thresholdY && programContext.bitmap.data[(h - hj) * programContext.bitmap.info.width + w1] >= thresholdY){
                            foundLeft = true;
                        }
                    }
                }
                
                ASSERT(foundLeft && foundRight);
                if(foundLeft == false || foundLeft == false){
                    printf("Error: CV failed to find image borders to crop\n");
                    context.quit = true;
                    return;
                }
                bY = h;
                
                result = cropImageY(&programContext.bitmap, bY, tY);
                ASSERT(result);
                if(result == false){
                    printf("Error: IP failed to crop image\n");
                    context.quit = true;
                    return;
                }
            }
            
            
            
            
            
            
            
        }
        
        PixelGradient * gradients;
        
        
        
        //find middle
        
        
        
        
        PUSHI;
        gradients = &PUSHA(PixelGradient, (programContext.bitmap.info.width) * (programContext.bitmap.info.height));
        uint32 * averages = &PUSHA(uint32, (programContext.bitmap.info.width) * (programContext.bitmap.info.height));
        uint8 * pixelArea = &PUSHA(uint8, 9);
        float32 tilesize = 0.02f;
        uint32 tileW = (uint32)(programContext.bitmap.info.width-2);
        uint32 tileH = (uint32)(tilesize * programContext.bitmap.info.height);
        uint32 tilesCountX = (programContext.bitmap.info.width / tileW) + ((programContext.bitmap.info.width % tileW) != 0 ? 1 : 0);
        uint32 tilesCountY = programContext.bitmap.info.height / tileH + ((programContext.bitmap.info.height % tileH) != 0 ? 1 : 0);
        
        
        for(uint32 x = 1; x < programContext.bitmap.info.width - 1; x = x + tileW){
            for(uint32 y = 1; y < programContext.bitmap.info.height - 1; y = y + tileH){
                for(uint32 dX = x; dX < tileW + x; dX++){
                    for(uint32 dY = y; dY < tileH + y; dY++){
                        
                        //imagine pixels as numpad, middle pixel is 5, indexed from 0, instead of 1
                        pixelArea[0] = programContext.bitmap.data[(dY + 1) * programContext.bitmap.info.width + dX - 1];
                        pixelArea[1] = programContext.bitmap.data[(dY + 1) * programContext.bitmap.info.width + dX];
                        pixelArea[2] = programContext.bitmap.data[(dY + 1) * programContext.bitmap.info.width + dX + 1];
                        pixelArea[3] = programContext.bitmap.data[dY * programContext.bitmap.info.width + dX - 1];
                        pixelArea[4] = programContext.bitmap.data[dY * programContext.bitmap.info.width + dX];
                        pixelArea[5] = programContext.bitmap.data[dY * programContext.bitmap.info.width + dX + 1];
                        pixelArea[6] = programContext.bitmap.data[(dY - 1) * programContext.bitmap.info.width + dX - 1];
                        pixelArea[7] = programContext.bitmap.data[(dY - 1) * programContext.bitmap.info.width + dX];
                        pixelArea[8] = programContext.bitmap.data[(dY - 1) * programContext.bitmap.info.width + dX + 1];
                        
                        uint8 maxleap = 0;
                        uint8 direction = 4; //from middle to middle
                        //find biggest leap
                        for(uint8 pi = 0; pi < 9; pi++){
                            int16 difference = ((int16) pixelArea[pi] - (int16) pixelArea[4]);
                            ASSERT(difference < 256);
                            if(difference > 0 && (uint8)difference > maxleap){
                                maxleap = (uint8) difference;
                                direction = pi;
                            }
                        }
                        gradients[dY * programContext.bitmap.info.width + dX].size = maxleap;
                        gradients[dY * programContext.bitmap.info.width + dX].direction = direction;
                        
                    }
                }
            }
            
        }
        /*
        for(uint32 x = 1; x < programContext.bitmap.info.width - 1; x = x + tileW){
        for(uint32 y = 1; y < programContext.bitmap.info.height - 1; y = y + tileH){
        for(uint32 dX = x; dX < tileW + x; dX++){
        for(uint32 dY = y; dY < tileH + y; dY++){
        //programContext.bitmap.data[dY * programContext.bitmap.info.width + dX] = averages[y * programContext.bitmap.info.width + x];// (gradients[dY * programContext.bitmap.info.width + dX].size  > 5 &&
        //programContext.bitmap.data[dY * programContext.bitmap.info.width + dX] = programContext.bitmap.data[dY * programContext.bitmap.info.width + dX] < 40)? 255 : 0;
        programContext.bitmap.data[dY * programContext.bitmap.info.width + dX] = (gradients[dY * programContext.bitmap.info.width + dX].size > 5) ? 255 : 0;
        }
        }
        
        }
        }
        */
        
        uint8 bottomFindingGradientThreshold = 0;
        
        for(uint32 x = 1; x < programContext.bitmap.info.width - 1; x = x + tileW){
            for(uint32 y = 1; y < programContext.bitmap.info.height - 1; y = y + tileH){
                averages[y*programContext.bitmap.info.width + x] = 0;
                for(uint32 dX = x; dX < tileW + x; dX++){
                    for(uint32 dY = y; dY < tileH + y; dY++){
                        averages[y*programContext.bitmap.info.width + x] += gradients[dY * programContext.bitmap.info.width + dX].size > bottomFindingGradientThreshold ? 255 : 0;
                    }
                }
                averages[y*programContext.bitmap.info.width + x] /= tileW*tileH;
            }
            
        }
        /*
        for(uint32 x = 1; x < programContext.bitmap.info.width - 1; x = x + tileW){
        for(uint32 y = 1; y < programContext.bitmap.info.height - 1; y = y + tileH){
        for(uint32 dX = x; dX < tileW + x; dX++){
        for(uint32 dY = y; dY < tileH + y; dY++){
        programContext.bitmap.data[dY * programContext.bitmap.info.width + dX] = averages[y * programContext.bitmap.info.width + x];// (gradients[dY * programContext.bitmap.info.width + dX].size  > 5 &&
        //programContext.bitmap.data[dY * programContext.bitmap.info.width + dX] = programContext.bitmap.data[dY * programContext.bitmap.info.width + dX] < 40)? 255 : 0;
        //programContext.bitmap.data[dY * programContext.bitmap.info.width + dX] = gradients[dY * programContext.bitmap.info.width + dX].size > 5 ? 255 : 0;
        }
        }
        
        }
        }
        */
        
        
        uint32 * candidates = &PUSHA(uint32, tilesCountY);
        uint8 * candidateAverages = &PUSHA(uint8, tilesCountY);
        
        uint32 candidateCount = 0;
        
        for(uint32 x = 1; x < programContext.bitmap.info.width - 1; x = x + tileW){
            for(uint32 y = 1 + 2*tileH; y < programContext.bitmap.info.height - 1; y = y + tileH){
                bool isCandidate = //averages[y * programContext.bitmap.info.width + x] > 10 &&
                averages[(y - 2*tileH)* programContext.bitmap.info.width + x] < averages[(y - 1*tileH)* programContext.bitmap.info.width + x] &&
                    averages[(y - 1*tileH)* programContext.bitmap.info.width + x] < averages[(y)* programContext.bitmap.info.width + x] &&
                    averages[(y + 1*tileH)* programContext.bitmap.info.width + x] < averages[(y)* programContext.bitmap.info.width + x] &&
                    averages[(y + 2*tileH)* programContext.bitmap.info.width + x] < averages[(y + 1*tileH)* programContext.bitmap.info.width + x];
                if(isCandidate){
                    candidates[candidateCount] = y + tileH/2;
                    candidateAverages[candidateCount] = averages[y * programContext.bitmap.info.width + x];
                    candidateCount++;
                }
                /*
                for(uint32 dX = x; dX < tileW + x; dX++){
                for(uint32 dY = y; dY < tileH + y; dY++){
                programContext.bitmap.data[dY * programContext.bitmap.info.width + dX] = 0;
                if(isCandidate){
                programContext.bitmap.data[dY * programContext.bitmap.info.width + dX] = averages[y * programContext.bitmap.info.width + x];
                }
                //programContext.bitmap.data[dY * programContext.bitmap.info.width + dX] = averages[y * programContext.bitmap.info.width + x];// (gradients[dY * programContext.bitmap.info.width + dX].size  > 5 &&
                //programContext.bitmap.data[dY * programContext.bitmap.info.width + dX] = programContext.bitmap.data[dY * programContext.bitmap.info.width + dX] < 40)? 255 : 0;
                //programContext.bitmap.data[dY * programContext.bitmap.info.width + dX] = gradients[dY * programContext.bitmap.info.width + dX].size > 5 ? 255 : 0;
                }
                }
                */
            }
        }
        
        
        
        ASSERT(candidateCount > 0);
        if(candidateCount == 0){
            printf("Error: CV failed to find middle mark\n");
            context.quit = true;
            return;
        }
        
        /*
        uint32 * candidateIndex = &PUSHA(uint32, parameters->blocksCount);
        bool * found =  &PUSHA(bool, parameters->blocksCount);
        
        float32 margin = 0.1 * ((parameters->blocksCount > 2)? (1.0 / powd(2, parameters->blocksCount-2)) : 1);
        
        for(uint8 cropIndex = 0; cropIndex < parameters->blocksCount; cropIndex++){
        found[cropIndex] = false;
        candidateIndex[cropIndex] = 0;
        if(!parameters->dontInput[cropIndex]){
        float32 borderline = (float32)(cropIndex + 1) / (float32)parameters->blocksCount;
        for(uint32 ci = 0; ci < candidateCount; ci++){
        if(candidates[ci] > (borderline - margin) * programContext.bitmap.info.height &&
        candidates[ci] < (borderline + margin) * programContext.bitmap.info.height &&
        (!found[cropIndex] ||
        candidateAverages[ci] < candidateAverages[candidateIndex[cropIndex]])
        ){
        
        candidateIndex[cropIndex] = ci;
        found[cropIndex] = true;
        
        }
        
        }
        
        ASSERT(found[cropIndex]);
        cropImageY(&programContext.bitmap, candidates[candidateIndex[cropIndex]], 0);
        
        }
        }
        
        
        
        */
        
        
        uint32 candidateIndex = 0;
        bool found = false;
        
        for(uint32 ci = 0; ci < candidateCount; ci++){
            if(candidates[ci] > 0.4 * programContext.bitmap.info.height &&
               candidates[ci] < 0.6 * programContext.bitmap.info.height &&
               (
                !found ||
                candidateAverages[ci] < candidateAverages[candidateIndex]
                )){
                candidateIndex = ci;
                found = true;
            }
        }
        
        ASSERT(found);
        if(!found){
            printf("Error: CV failed to find marks to half the image near middle of the whole picture\n");
            context.quit = true;
            return;
        }
        
        uint32 blockHeight = candidates[candidateIndex];
        uint8 totalBlocks = 0;
        
        for(uint8 cropIndex = 0; cropIndex < parameters->blocksCount; cropIndex++){
            if(!parameters->dontInput[cropIndex]){
                moveImageBlockUp(&programContext.bitmap, (uint32)(((float32)cropIndex / parameters->blocksCount)*programContext.bitmap.info.height), totalBlocks * blockHeight, blockHeight);
                totalBlocks++;
            }
        }
        
        programContext.bitmap.info.height = totalBlocks*blockHeight;
        
        BitmapFont font;
        
        uint32 colW;
        uint32 markOffset;
        
        float32 bordersX;
        
        
        
        
        //we are going to print
        if(parameters->labelsCount != 0 || !parameters->dontMark){
            FileContents fontFile;
            bool result = readFile("font.bmp", &fontFile);
            ASSERT(result);
            if(!result){
                printf("Error: system failed to read font file.\n");
                context.quit = true;
                return;
            }
            Image source;
            result = decodeBMP(&fontFile, &source);
            if(!result){
                printf("Error: failed to decode BMP font file, perhaps it has incorrect format.\n");
                context.quit = true;
                return;
            }
            result = flipY(&source);
            if(!result){
                printf("Error: IP failed to flip font image.\n");
                context.quit = true;
                return;
            }
            result = initBitmapFont(&font, &source, source.info.width / 16); 
            if(!result){
                printf("Error: failed to init font file.\n");
                context.quit = true;
                return;
            }
            uint32 fontSize = 24;
            
            
            markOffset = 0;
            
            bordersX = 0.03f;
            
            float32 topBorder = 0.1f;
            
            
            
            colW = (uint32)(programContext.bitmap.info.width * (1-bordersX)) / (parameters->columns == 0 ? 20 : parameters->columns);
            
            
            if(!parameters->dontMark){
                
                struct Cluster{
                    uint32 startY;
                    uint32 endY;
                };
                
                Cluster mark;
                
                
                uint8 markGradientThreshold = 4;
                uint8 markIntensityThreshold = 15;
                uint16 k = 3;
                uint8 * pixelKArea = &PUSHA(uint8, k*k);
                bool lastCluster = false;
                uint32 currentAverage = 0;
                //kNN sort of clustering
                
                uint8 markIntensityDifferenceThreshold = 10;
                float32 clusterMembershipQuorum = 0.3f;
                float32 clusterWidthQuorum = 0.4f;
                uint8 clusterMinimum = 0;
                
                
                //finding the left side mark
                int16 markIndex = -1;
                
                
                for(uint32 y = (k + (uint32)(topBorder*programContext.bitmap.info.height)); y < programContext.bitmap.info.height - k; y++){
                    currentAverage = 0;
                    uint32 averageIntensity = 0;
                    for(uint32 x = k + (uint32)((bordersX/2)*programContext.bitmap.info.width); x < colW + (uint32)((bordersX/2)*programContext.bitmap.info.width); x++){
                        if(gradients[y * programContext.bitmap.info.width + x].size > markGradientThreshold &&
                           programContext.bitmap.data[y * programContext.bitmap.info.width + x] > markIntensityThreshold){
                            
                            uint16 areasize = 0;
                            for(uint32 kX = x - k; kX < colW + (uint32)((bordersX/2)*programContext.bitmap.info.width) && kX < x + k + 1; kX++){
                                for(uint32 kY = y - k; kY < programContext.bitmap.info.height-1 && kY < y + k + 1; kY++){
                                    pixelKArea[areasize] = programContext.bitmap.data[kY * programContext.bitmap.info.width + kX];
                                    areasize++;
                                }
                            } 
                            
                            uint16 members = 0;
                            for(uint16 pi = 0; pi < areasize; pi++){
                                int16 difference = ((int16) pixelKArea[pi] - (int16) pixelKArea[k*(k*2+1) + k]);
                                ASSERT(difference < 256);
                                if(ABS(difference) >= markIntensityDifferenceThreshold || (lastCluster && pixelKArea[pi] >= clusterMinimum)){
                                    members++;
                                }
                            }
                            
                            if(members > clusterMembershipQuorum*(2*k+1)*(2*k+1)){
                                currentAverage += 1;
                                averageIntensity += programContext.bitmap.data[y*programContext.bitmap.info.width + x];
                                //programContext.bitmap.data[y * programContext.bitmap.info.width + x] = 255;
                            }else{
                                //programContext.bitmap.data[y * programContext.bitmap.info.width + x] = 0;
                            }
                        }else{
                            //programContext.bitmap.data[y * programContext.bitmap.info.width + x] = 0;
                        }
                        
                    }
                    
                    if(currentAverage > clusterWidthQuorum * colW){
                        
                        if(!lastCluster){
                            mark.startY = y;
                            clusterMinimum = averageIntensity / currentAverage;
                        }
                        lastCluster = true;
                        /*for(uint32 x = 0; x < colW; x++){
                        programContext.bitmap.data[y*programContext.bitmap.info.width + x] = 100;
                        }*/
                    }else{
                        if(lastCluster){
                            mark.endY = y;
                            markIndex++;
                        }
                        lastCluster = false;
                        
                    }
                    if(markIndex == parameters->targetMark){
                        break;
                    }
                }
                
                ASSERT(markIndex == parameters->targetMark);
                if(markIndex != parameters->targetMark){
                    printf("Error: IP failed to find desired mark.\n");
                    context.quit = true;
                    return;
                }
                
                
            }
            /*
            FileContents bmp2;
            encodeBMP(&programContext.bitmap, &bmp2);
            saveFile("bmp.bmp", &bmp2);
            return;
            */
            
            
        }
        
        
        
        context.quit = true;
        
        postProcess(parameters);
        saveFiles(parameters);
        context.quit = true;
        
        
    }
    
    
    
    void runManual(RunParameters * parameters){
        
        switch(programContext.state){
            case ProgramState_Init:{
                if(!openFiles(parameters)){
                    return;
                };
                
                //resize
                Image draw;
                Image tmp;
                draw.info = programContext.bitmap.info;
                draw.info.samplesPerPixel = 4;
                tmp.info = draw.info;
                tmp.data = (byte *) &PPUSHA(uint32, draw.info.width * draw.info.height);
                draw.data = (byte *) &PPUSHA(uint32, draw.info.width * draw.info.height);
                for(uint32 i = 0; i < programContext.bitmap.info.width * programContext.bitmap.info.width; i++){
                    for(uint8 j = 0; j < 3; j++){
                        tmp.data[4*i + j] = programContext.bitmap.data[i];
                    }
                    
                    
                }
                bool result = scaleImage(&tmp, &draw, renderer.width, renderer.height);
                ASSERT(result);
                if(result == false){
                    printf("Error: IP failed to scale rendering image\n");
                    context.quit = true;
                    return;
                }
                renderer.drawBitmapData = draw;
                renderer.originalBitmapData = tmp;
                
                programContext.state = ProgramState_SelectCorners;
                
            }break;
            case ProgramState_SelectCorners:{
                if(programContext.pointsCount == 0){
                    if(programContext.input.click == true){
                        programContext.points[programContext.pointsCount++] = programContext.mouse;
                    }
                    if(programContext.input.back == true){
                        context.quit = true;
                    }
                }else if(programContext.pointsCount == 1){
                    if(programContext.input.click == true){
                        programContext.points[programContext.pointsCount++] = programContext.mouse;
                    }
                    if(programContext.input.back == true){
                        programContext.pointsCount--;
                    }
                }else if(programContext.pointsCount == 2){
                    if(programContext.input.click == true){
                        programContext.points[2] = programContext.line[0];
                        programContext.points[3] = programContext.line[1];
                        
                        programContext.pointsCount = 4;
                        for(uint8 i = 0; i < ARRAYSIZE(programContext.sortedPoints); i++){
                            programContext.sortedPoints[i] = programContext.points[i];
                        }
                        
                        //sort stable by left top corner to right bot corner
                        //closest to the origin is top left corner
                        insertSort((byte *) programContext.sortedPoints, sizeof(programContext.sortedPoints[0]), ARRAYSIZE(programContext.sortedPoints), [](void * a, void *b) -> int8{
                                   int32 y1 = ((dv2 *) a)->y;
                                   int32 y2 = ((dv2 *) b)->y;
                                   
                                   int32 x1 = ((dv2 *) a)->x;
                                   int32 x2 = ((dv2 *) b)->x;
                                   int32 A = y1*y1+x1*x1;
                                   int32 B = y2*y2+x2*x2;
                                   if(A > B){
                                   return 1;
                                   }else if(A < B){
                                   return -1;
                                   }
                                   return 0
                                   ;});
                        //the rightmost with smallest Y is second
                        insertSort((byte *) &programContext.sortedPoints[1], sizeof(programContext.sortedPoints[0]), ARRAYSIZE(programContext.sortedPoints) - 1, [](void * a, void *b) -> int8{
                                   int32 x1 = ((dv2 *) a)->x;
                                   int32 x2 = ((dv2 *) b)->x;
                                   int32 A = x1;
                                   int32 B = x2;
                                   if(A < B){
                                   return 1;
                                   }else if(A > B){
                                   return -1;
                                   }
                                   return 0
                                   ;});
                        
                        uint8 leastIndex = 1;
                        for(uint8 i = leastIndex + 1; i < ARRAYSIZE(programContext.sortedPoints) - 1; i++){
                            if(programContext.sortedPoints[i].y < programContext.sortedPoints[leastIndex].y){
                                leastIndex = i;
                            }
                        }
                        dv2 tmp = programContext.sortedPoints[1];
                        programContext.sortedPoints[1] = programContext.sortedPoints[leastIndex];
                        programContext.sortedPoints[leastIndex] = tmp;
                        
                        //now the remaining be sorted by x
                        insertSort((byte *) &programContext.sortedPoints[2], sizeof(programContext.sortedPoints[0]), ARRAYSIZE(programContext.sortedPoints) - 2, [](void * a, void *b) -> int8{
                                   int32 x1 = ((dv2 *) a)->x;
                                   int32 x2 = ((dv2 *) b)->x;
                                   int32 A = x1;
                                   int32 B = x2;
                                   if(A > B){
                                   return 1;
                                   }else if(A < B){
                                   return -1;
                                   }
                                   return 0
                                   ;});
                        
                        
                        
                        programContext.rotation = {(float32)(programContext.sortedPoints[2].x - programContext.sortedPoints[0].x), (float32)(programContext.sortedPoints[2].y - programContext.sortedPoints[0].y)};
                        programContext.rotationCenter = {((float32)programContext.sortedPoints[0].x)/renderer.drawBitmapData.info.width, ((float32)programContext.sortedPoints[0].y)/renderer.drawBitmapData.info.height};
                        /*bool check = programContext.sortedPoints[0].x < programContext.sortedPoints[1].x && programContext.sortedPoints[0].y < programContext.sortedPoints[2].y &&
                            programContext.sortedPoints[0].y < programContext.sortedPoints[3].y &&
                            programContext.sortedPoints[2].x < programContext.sortedPoints[3].x;
                        ASSERT(check);
                        */
                        programContext.state = ProgramState_SelectHalf;
                        
                    }
                    if(programContext.input.back == true){
                        programContext.pointsCount--;
                    }
                    
                }
                
            }break;
            case ProgramState_SelectHalf:{
                if(parameters->blocksCount == 1){
                    programContext.state = ProgramState_SelectMarks;
                    programContext.half[0] = programContext.sortedPoints[2];
                    programContext.half[1] = programContext.sortedPoints[3];
                }else{
                    if(programContext.input.back == true){
                        programContext.pointsCount = 2;
                        programContext.state = ProgramState_SelectCorners;
                    }
                    if(programContext.input.click == true){
                        programContext.half[0] = programContext.line[0];
                        programContext.half[1] = programContext.line[1];
                        programContext.state = ProgramState_SelectMarks;
                    }
                }
            }break;
            case ProgramState_SelectMarks:{
                if(parameters->dontMark){
                    programContext.state = ProgramState_Shutdown;
                }
                else{
                    if(programContext.input.back == true){
                        if(parameters->blocksCount == 1){
                            programContext.pointsCount = 2;
                            programContext.state = ProgramState_SelectCorners;
                        }else{
                            programContext.state = ProgramState_SelectHalf;
                        }
                    }
                    if(programContext.input.click == true){
                        programContext.mark = programContext.line[0];
                        programContext.state = ProgramState_Shutdown;
                    }
                }
            }break;
            case ProgramState_Shutdown:{
                bool result = false;
                
                float32 scaleX = (float32) programContext.bitmap.info.width / renderer.drawBitmapData.info.width;
                float32 scaleY = (float32) programContext.bitmap.info.height / renderer.drawBitmapData.info.height;
                
                //rescale the points to fit the original bitmap data
                for(uint8 i = 0; i < ARRAYSIZE(programContext.sortedPoints); i++){
                    programContext.sortedPoints[i].x = (int32)(programContext.sortedPoints[i].x * scaleX);
                    programContext.sortedPoints[i].y = (int32)(programContext.sortedPoints[i].y * scaleY);
                }
                
                programContext.half[0].x = (int32)(programContext.half[0].x * scaleX);
                programContext.half[1].x = (int32)(programContext.half[1].x * scaleX);
                programContext.half[0].y = (int32)(programContext.half[0].y * scaleY);
                programContext.half[1].y = (int32)(programContext.half[1].y * scaleY); programContext.mark.x = (int32)(programContext.mark.x * scaleX);
                programContext.mark.y = (int32)(programContext.mark.y * scaleY);
                
                programContext.rotation.x = programContext.rotation.x * scaleX;
                programContext.rotation.y = programContext.rotation.y * scaleY;
                
                
                v2 down = V2(0, 1);
                float32 degAngle = radAngle(programContext.rotation, down) * 180 / PI;
                if(det(programContext.rotation, down) < 0){
                    degAngle *= -1;
                }
                
                float32 radAngle = degToRad(degAngle);
                
                result = rotateImage(&programContext.bitmap, degAngle, programContext.rotationCenter.x, programContext.rotationCenter.y);
                
                dv2 rotationCenter = {(int32)(programContext.rotationCenter.x * programContext.bitmap.info.width), (int32)(programContext.rotationCenter.y * programContext.bitmap.info.height)};
                
                
                
                
                
                
                //rotate the points too
                for(uint8 i = 0; i < ARRAYSIZE(programContext.sortedPoints); i++){
                    programContext.sortedPoints[i] = translate(programContext.sortedPoints[i],
                                                               -rotationCenter);
                }
                
                //rotate the points too
                for(uint8 i = 0; i < ARRAYSIZE(programContext.sortedPoints); i++){
                    v2 res = rotate(programContext.sortedPoints[i], radAngle);
                    programContext.sortedPoints[i] = {(int32)res.x, (int32)res.y};
                }
                
                //rotate the points too
                for(uint8 i = 0; i < ARRAYSIZE(programContext.sortedPoints); i++){
                    programContext.sortedPoints[i] = translate(programContext.sortedPoints[i],rotationCenter);
                }
                
                //crop that motherfucker
                
                int32 leftX = (programContext.sortedPoints[0].x + programContext.sortedPoints[2].x) / 2;
                int32 rightX = (programContext.sortedPoints[1].x + programContext.sortedPoints[3].x) / 2;
                
                int32 topY = (programContext.sortedPoints[0].y + programContext.sortedPoints[1].y) / 2;
                int32 botY = (programContext.sortedPoints[2].y + programContext.sortedPoints[3].y) / 2;
                topY = clamp(topY, 0, programContext.bitmap.info.height);
                botY = clamp(botY, 0, programContext.bitmap.info.height);
                leftX = clamp(leftX, 0, programContext.bitmap.info.width);
                rightX = clamp(rightX, 0, programContext.bitmap.info.width);
                result = cropImageY(&programContext.bitmap, botY, topY) && cropImageX(&programContext.bitmap, leftX, rightX);
                ASSERT(result);
                if(!result){
                    printf("Error: IP failed to crop the image\n");
                    context.quit = true;
                    return;
                }
                
                dv2 halfVector = programContext.half[0] - programContext.sortedPoints[0];
                programContext.blockSize = (uint32) length(halfVector);
                
                //now use only the valid blocks
                uint8 skipped = 0;
                for(uint8 i = 0; i < parameters->blocksCount; i++){
                    if(parameters->dontInput[i]){
                        int32 from = ((i+1-skipped) * programContext.blockSize);
                        int32 to = (i-skipped) * programContext.blockSize;
                        moveImageBlockUp(&programContext.bitmap, from, to, programContext.bitmap.info.height - from);
                        skipped++;
                    }
                }
                result = cropImageY(&programContext.bitmap, (parameters->blocksCount-skipped)*programContext.blockSize, 0);
                
                if(!result){
                    printf("Error: IP failed to crop the image\n");
                    context.quit = true;
                    return;
                }
                postProcess(parameters);
                
                saveFiles(parameters);
                context.quit = true;
            }break;
            default:{
                INV;
            }break;
        }
        
        
        /*
        PixelGradient * gradients;
        
        
        BitmapFont font;
        
        uint32 colW;
        uint32 markOffset;
        
        float32 bordersX;
        
        
        if(!parameters->isManual || programContext.state == ProgramState_Shutdown){
        
        bool result;
        if(parameters->isManual){
        
        }
        
        
        
        if(parameters->labelsCount != 0){
        INV;
        result = scaleCanvas(&programContext.bitmap, programContext.bitmap.info.width, programContext.bitmap.info.height + colW, 0, 0);
        if(!result){
        printf("Error: IP failed to scale image for the labels.\n");
        context.quit = true;
        return;
        }
        char stamp[2];
        stamp[1] = '\0';
        uint8 symbolIndex = parameters->beginningSymbolIndex;
        for(uint8 ci = 1; ci < parameters->columns; ci++){
        stamp[0] = parameters->labels[symbolIndex];
        result = printToBitmap(&programContext.bitmap, markOffset + ci*colW + (uint32)((bordersX/2)*programContext.bitmap.info.width), programContext.bitmap.info.height - colW, stamp , &font, colW);
        if(!result){
        printf("Error: Iailed to print label.\n");
        context.quit = true;
        return;
        }
        symbolIndex = (symbolIndex + 1) % parameters->labelsCount;
        }
        }
        
        
        
        
        
        
        */
        
        
        
        
        
        
        
        
        
        
}