    #include "util_filesystem.h"
#include "util_font.cpp"
#include "util_math.cpp"
    
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
        uint8 pointsCount;
        UserInput input;
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
        for(uint32 h = 0; h < howMuch; h++){
            for(uint32 w = 0; w < target->info.height; w++){
                target->data[(toY + h) * target->info.width + w] = target->data[(fromY + h) * target->info.width + w];
            }
        }
    }
    
    void run(RunParameters * parameters){
        
        if(!parameters->isManual || programContext.state == ProgramState_Init){
            FileContents imageFile;
            readFile(parameters->inputfile, &imageFile);
            
            decodeTiff(&imageFile, &programContext.bitmap);
            
            ASSERT(programContext.bitmap.info.bitsPerSample * programContext.bitmap.info.samplesPerPixel == 8);
            
            if(parameters->isManual){
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
                scaleImage(&tmp, &draw, renderer.width, renderer.height);
                renderer.drawBitmapData = draw;
                renderer.originalBitmapData = tmp;
            }
            
            programContext.state = ProgramState_SelectCorners;
            
        }
        
        
        
        uint8 thresholdX = 20;
        uint8 thresholdY = 4;
        int32 wj = programContext.bitmap.info.width / 10;
        
        if(!parameters->isManual || programContext.state == ProgramState_SelectCorners){
            
            if(parameters->isManual){
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
                        programContext.state = ProgramState_SelectHalf;
                    }
                    if(programContext.input.back == true){
                        programContext.pointsCount--;
                    }
                }
                
            }else{
                
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
                    
                    v2 down = V2(0, 1);
                    float32 degAngle = radAngle(rotation, down) * 180 / PI;
                    if(det(rotation, down) < 0){
                        degAngle *= -1;
                    }
                    
                    POPI;
                    
                    
                    
                    rotateImage(&programContext.bitmap, degAngle, center.x, center.y);
                    
                    
                    
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
                        cropImageX(&programContext.bitmap, lX, rX);
                        
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
                        bY = h;
                        
                        cropImageY(&programContext.bitmap, bY, tY);
                        
                    }
                    
                    
                }
                programContext.state = ProgramState_SelectHalf;
            }
            
            
            
        }
        
        PixelGradient * gradients;
        
        if(!parameters->isManual || programContext.state == ProgramState_SelectHalf){
            
            if(parameters->isManual){
                
            }else{
                
                
                
                //find middle
                
                
                
                ASSERT(programContext.bitmap.info.origin == BitmapOriginType_TopLeft);
                ASSERT(programContext.bitmap.info.interpretation = BitmapInterpretationType_GrayscaleBW01);
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
                
                uint32 blockHeight = candidates[candidateIndex];
                uint8 totalBlocks = 0;
                
                for(uint8 cropIndex = 0; cropIndex < parameters->blocksCount; cropIndex++){
                    if(!parameters->dontInput[cropIndex]){
                        moveImageBlockUp(&programContext.bitmap, (uint32)(((float32)cropIndex / parameters->blocksCount)*programContext.bitmap.info.height), totalBlocks * blockHeight, blockHeight);
                        totalBlocks++;
                    }
                }
                
                programContext.bitmap.info.height = totalBlocks*blockHeight;
            }
            programContext.state = ProgramState_SelectMarks;
        }
        BitmapFont font;
        
        uint32 colW;
        uint32 markOffset;
        
        float32 bordersX;
        if(!parameters->isManual || programContext.state == ProgramState_SelectMarks){
            
            if(parameters->isManual){
                
            }else{
                
                
                
                //we are going to print
                if(parameters->labelsCount != 0 || !parameters->dontMark){
                    
                    
                    FileContents fontFile;
                    readFile("font.bmp", &fontFile);
                    Image source;
                    decodeBMP(&fontFile, &source);
                    flipY(&source);
                    initBitmapFont(&font, &source, source.info.width / 16); 
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
                        
                        markOffset = fontSize * strlen(parameters->markText);
                        
                        scaleCanvas(&programContext.bitmap, programContext.bitmap.info.width + markOffset, programContext.bitmap.info.height, markOffset, 0);
                        
                        printToBitmap(&programContext.bitmap, 0, mark.startY, parameters->markText, &font, fontSize);
                    }
                    /*
                    FileContents bmp2;
                    encodeBMP(&programContext.bitmap, &bmp2);
                    saveFile("bmp.bmp", &bmp2);
                    return;
                    */
                    
                }
            }
            programContext.state = ProgramState_Shutdown;
        }
        
        
        if(!parameters->isManual || programContext.state == ProgramState_SelectMarks){
            
            
            
            if(parameters->labelsCount != 0){
                scaleCanvas(&programContext.bitmap, programContext.bitmap.info.width, programContext.bitmap.info.height + colW, 0, 0);
                char stamp[2];
                stamp[1] = '\0';
                uint8 symbolIndex = parameters->beginningSymbolIndex;
                for(uint8 ci = 1; ci < parameters->columns; ci++){
                    stamp[0] = parameters->labels[symbolIndex];
                    printToBitmap(&programContext.bitmap, markOffset + ci*colW + (uint32)((bordersX/2)*programContext.bitmap.info.width), programContext.bitmap.info.height - colW, stamp , &font, colW);
                    symbolIndex = (symbolIndex + 1) % parameters->labelsCount;
                }
            }
            
            
            
            
            if(parameters->invertColors){
                for(uint32 i = 0; i < programContext.bitmap.info.width * programContext.bitmap.info.height * programContext.bitmap.info.bitsPerSample * programContext.bitmap.info.samplesPerPixel; i++){
                    programContext.bitmap.data[i] = 255 - programContext.bitmap.data[i]; 
                }
            }
            
            FileContents tiff;
            encodeBMP(&programContext.bitmap, &tiff);
            saveFile(parameters->outputfile, &tiff);
            
            POPI;
            
            context.quit = true;
            
        }
        
        
}