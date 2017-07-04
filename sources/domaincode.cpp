    #include "util_filesystem.h"
#include "util_font.cpp"
    
    struct RunParameters{
        char file[256];
        uint8 columns;
        uint8 targetMark;
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
    
    void run(RunParameters * parameters){
        FileContents imageFile;
        readFile(parameters->file, &imageFile);
        Image bitmap;
        decodeTiff(&imageFile, &bitmap);
        
        ASSERT(bitmap.info.bitsPerSample * bitmap.info.samplesPerPixel == 8);
        
        //contrast the image
        
        uint32 averageIntensity = 0;
        for(uint32 w = 0; w < bitmap.info.width; w++){
            for(uint32 h = 0; h < bitmap.info.height; h++){
                averageIntensity += bitmap.data[w + h*bitmap.info.width];
            }
        }
        
        averageIntensity /= bitmap.info.width * bitmap.info.height;
        float32 dst = (float32)(20 - averageIntensity);
        applyContrast(&bitmap, -1.5f * dst);
        
        
        /*
        FileContents bmp2;
        encodeBMP(&bitmap, &bmp2);
        saveFile("bmp.bmp", &bmp2);
        return;
        */
        
        uint8 thresholdX = 20;
        uint8 thresholdY = 4;
        
        uint8 rotatingSamples = 5;
        int32 wj = bitmap.info.width / 10;
        {
            PUSHI;
            v2 * rotations = &PUSHA(v2, rotatingSamples);
            v2 * centers = &PUSHA(v2, rotatingSamples);
            uint16 rotationCount = 0;
            int32 rowH = (int32)(bitmap.info.height * 0.9) / rotatingSamples;
            for(uint8 rsi = 0; rsi < rotatingSamples; rsi++){
                //sample 1
                bool foundTop = false;
                bool foundBot = false;
                int32 h1 = (int32)(bitmap.info.height * 0.1) + rsi*rowH;
                int32 h2 = h1 + rowH;
                
                
                int32 w1, w2;
                
                for(uint32 w = 0; w < bitmap.info.width && (!foundBot || !foundTop); w++){
                    if(!foundTop){
                        if(bitmap.data[h1 * bitmap.info.width + w] >= thresholdX && bitmap.data[h1 * bitmap.info.width + w + wj] >= thresholdX){
                            foundTop = true;
                            w1= w;
                        }
                    }
                    if(!foundBot){
                        if(bitmap.data[h2 * bitmap.info.width + w] >= thresholdX && bitmap.data[h2 * bitmap.info.width + w + wj] >= thresholdX){
                            foundBot = true;
                            w2= w;
                        }
                    }
                }
                
                if(foundBot && foundTop){
                    rotations[rotationCount] = normalize(V2((float32)(w2 - w1),(float32)(h2 - h1)));
                    centers[rotationCount] = V2((float32)w1/bitmap.info.width, (float32)h1/bitmap.info.height);
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
            
            
            
            rotateImage(&bitmap, degAngle, center.x, center.y);
        }
        /*
        FileContents bmp2;
        encodeBMP(&bitmap, &bmp2);
        saveFile("bmp.bmp", &bmp2);
        return;
        */
        
        {
            //crop
            uint32 lX, rX, tY, bY;
            int32 h = bitmap.info.height / 2;
            uint32 w = 0;
            for(; w < bitmap.info.width; w++){
                if(bitmap.data[h * bitmap.info.width + w] >= thresholdX && bitmap.data[h * bitmap.info.width + w + wj] >= thresholdX){
                    break;
                }
                
            }
            lX = w;
            
            for(w = bitmap.info.width - 1; w >= 0; w--){
                if(bitmap.data[h * bitmap.info.width + w] >= thresholdX && bitmap.data[h * bitmap.info.width + w - wj] >= thresholdX){
                    break;
                }
                
            }
            rX = w;
            cropImageX(&bitmap, lX, rX);
            
            uint32 w1 = (uint32)((float32)bitmap.info.width * 0.1f);
            uint32 w2 = (uint32)((float32)bitmap.info.width * 0.9f);
            int32 hj = bitmap.info.height / 10;
            
            bool foundLeft = false;
            bool foundRight = false;
            
            h = 0;
            for(; h < bitmap.info.height && (!foundLeft || !foundRight); h++){
                if(!foundRight){
                    if(bitmap.data[h * bitmap.info.width + w2] >= thresholdY && bitmap.data[(h + hj) * bitmap.info.width + w2] >= thresholdY){
                        foundRight = true;
                        
                    }
                }
                if(!foundLeft){
                    if(bitmap.data[h * bitmap.info.width + w1] >= thresholdY && bitmap.data[(h + hj) * bitmap.info.width + w1] >= thresholdY){
                        foundLeft = true;
                    }
                }
            }
            
            ASSERT(foundLeft && foundRight);
            tY = h;
            
            foundRight = foundLeft = false;
            h = bitmap.info.height - 1;
            for(; h >= 0 && (!foundLeft || !foundRight); h--){
                if(!foundRight){
                    if(bitmap.data[h * bitmap.info.width + w2] >= thresholdY && bitmap.data[(h - hj) * bitmap.info.width + w2] >= thresholdY){
                        foundRight = true;
                        
                    }
                }
                if(!foundLeft){
                    if(bitmap.data[h * bitmap.info.width + w1] >= thresholdY && bitmap.data[(h - hj) * bitmap.info.width + w1] >= thresholdY){
                        foundLeft = true;
                    }
                }
            }
            
            ASSERT(foundLeft && foundRight);
            bY = h;
            
            cropImageY(&bitmap, bY, tY);
            
        }
        
        //find middle
        
        
        ASSERT(bitmap.info.origin == BitmapOriginType_TopLeft);
        ASSERT(bitmap.info.interpretation = BitmapInterpretationType_GrayscaleBW01);
        PUSHI;
        PixelGradient * gradients = &PUSHA(PixelGradient, (bitmap.info.width) * (bitmap.info.height));
        uint32 * averages = &PUSHA(uint32, (bitmap.info.width) * (bitmap.info.height));
        uint8 * pixelArea = &PUSHA(uint8, 9);
        float32 tilesize = 0.02f;
        uint32 tileW = (uint32)(bitmap.info.width-2);
        uint32 tileH = (uint32)(tilesize * bitmap.info.height);
        uint32 tilesCountX = (bitmap.info.width / tileW) + ((bitmap.info.width % tileW) != 0 ? 1 : 0);
        uint32 tilesCountY = bitmap.info.height / tileH + ((bitmap.info.height % tileH) != 0 ? 1 : 0);
        
        
        for(uint32 x = 1; x < bitmap.info.width - 1; x = x + tileW){
            for(uint32 y = 1; y < bitmap.info.height - 1; y = y + tileH){
                for(uint32 dX = x; dX < tileW + x; dX++){
                    for(uint32 dY = y; dY < tileH + y; dY++){
                        
                        //imagine pixels as numpad, middle pixel is 5, indexed from 0, instead of 1
                        pixelArea[0] = bitmap.data[(dY + 1) * bitmap.info.width + dX - 1];
                        pixelArea[1] = bitmap.data[(dY + 1) * bitmap.info.width + dX];
                        pixelArea[2] = bitmap.data[(dY + 1) * bitmap.info.width + dX + 1];
                        pixelArea[3] = bitmap.data[dY * bitmap.info.width + dX - 1];
                        pixelArea[4] = bitmap.data[dY * bitmap.info.width + dX];
                        pixelArea[5] = bitmap.data[dY * bitmap.info.width + dX + 1];
                        pixelArea[6] = bitmap.data[(dY - 1) * bitmap.info.width + dX - 1];
                        pixelArea[7] = bitmap.data[(dY - 1) * bitmap.info.width + dX];
                        pixelArea[8] = bitmap.data[(dY - 1) * bitmap.info.width + dX + 1];
                        
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
                        gradients[dY * bitmap.info.width + dX].size = maxleap;
                        gradients[dY * bitmap.info.width + dX].direction = direction;
                        
                    }
                }
            }
            
        }
        /*
        for(uint32 x = 1; x < bitmap.info.width - 1; x = x + tileW){
            for(uint32 y = 1; y < bitmap.info.height - 1; y = y + tileH){
                for(uint32 dX = x; dX < tileW + x; dX++){
                    for(uint32 dY = y; dY < tileH + y; dY++){
                        //bitmap.data[dY * bitmap.info.width + dX] = averages[y * bitmap.info.width + x];// (gradients[dY * bitmap.info.width + dX].size  > 5 &&
                        //bitmap.data[dY * bitmap.info.width + dX] = bitmap.data[dY * bitmap.info.width + dX] < 40)? 255 : 0;
                        bitmap.data[dY * bitmap.info.width + dX] = (gradients[dY * bitmap.info.width + dX].size > 5) ? 255 : 0;
                    }
                }
                
            }
        }
        */
        
        uint8 bottomFindingGradientThreshold = 5;
        
        for(uint32 x = 1; x < bitmap.info.width - 1; x = x + tileW){
            for(uint32 y = 1; y < bitmap.info.height - 1; y = y + tileH){
                averages[y*bitmap.info.width + x] = 0;
                for(uint32 dX = x; dX < tileW + x; dX++){
                    for(uint32 dY = y; dY < tileH + y; dY++){
                        averages[y*bitmap.info.width + x] += gradients[dY * bitmap.info.width + dX].size > bottomFindingGradientThreshold ? 255 : 0;
                    }
                }
                averages[y*bitmap.info.width + x] /= tileW*tileH;
            }
            
        }
        /*
        for(uint32 x = 1; x < bitmap.info.width - 1; x = x + tileW){
            for(uint32 y = 1; y < bitmap.info.height - 1; y = y + tileH){
                for(uint32 dX = x; dX < tileW + x; dX++){
                    for(uint32 dY = y; dY < tileH + y; dY++){
                        bitmap.data[dY * bitmap.info.width + dX] = averages[y * bitmap.info.width + x];// (gradients[dY * bitmap.info.width + dX].size  > 5 &&
                        //bitmap.data[dY * bitmap.info.width + dX] = bitmap.data[dY * bitmap.info.width + dX] < 40)? 255 : 0;
                        //bitmap.data[dY * bitmap.info.width + dX] = gradients[dY * bitmap.info.width + dX].size > 5 ? 255 : 0;
                    }
                }
                
            }
        }
        */
        
        
        uint32 * candidates = &PUSHA(uint32, tilesCountY);
        uint8 * candidateAverages = &PUSHA(uint8, tilesCountY);
        
        uint32 candidateCount = 0;
        
        for(uint32 x = 1; x < bitmap.info.width - 1; x = x + tileW){
            for(uint32 y = 1 + 2*tileH; y < bitmap.info.height - 1; y = y + tileH){
                bool isCandidate = //averages[y * bitmap.info.width + x] > 10 &&
                averages[(y - 2*tileH)* bitmap.info.width + x] < averages[(y - 1*tileH)* bitmap.info.width + x] &&
                    averages[(y - 1*tileH)* bitmap.info.width + x] < averages[(y)* bitmap.info.width + x] &&
                    averages[(y + 1*tileH)* bitmap.info.width + x] < averages[(y)* bitmap.info.width + x] &&
                    averages[(y + 2*tileH)* bitmap.info.width + x] < averages[(y + 1*tileH)* bitmap.info.width + x];
                if(isCandidate){
                    candidates[candidateCount] = y + tileH/2;
                    candidateAverages[candidateCount] = averages[y * bitmap.info.width + x];
                    candidateCount++;
                }
                /*
                for(uint32 dX = x; dX < tileW + x; dX++){
                    for(uint32 dY = y; dY < tileH + y; dY++){
                        bitmap.data[dY * bitmap.info.width + dX] = 0;
                        if(isCandidate){
                            bitmap.data[dY * bitmap.info.width + dX] = averages[y * bitmap.info.width + x];
                        }
                        //bitmap.data[dY * bitmap.info.width + dX] = averages[y * bitmap.info.width + x];// (gradients[dY * bitmap.info.width + dX].size  > 5 &&
                        //bitmap.data[dY * bitmap.info.width + dX] = bitmap.data[dY * bitmap.info.width + dX] < 40)? 255 : 0;
                        //bitmap.data[dY * bitmap.info.width + dX] = gradients[dY * bitmap.info.width + dX].size > 5 ? 255 : 0;
                    }
                }
                */
            }
        }
        
        
        
        ASSERT(candidateCount > 0);
        uint32 candidateIndex = 0;
        bool found = false;
        
        for(uint32 ci = 0; ci < candidateCount; ci++){
            if(candidates[ci] > 0.4 * bitmap.info.height &&
               candidates[ci] < 0.6 * bitmap.info.height &&
               (
                !found ||
                candidateAverages[ci] < candidateAverages[candidateIndex]
                )){
                candidateIndex = ci;
                found = true;
            }
        }
        
        ASSERT(found);
        
        cropImageY(&bitmap, candidates[candidateIndex], 0);
        
        struct Cluster{
            uint32 startY;
            uint32 endY;
        };
        
        Cluster mark;
        
        float32 bordersX = 0.03f;
        float32 topBorder = 0.1f;
        
        uint32 colW = (uint32)(bitmap.info.width * (1-bordersX)) / parameters->columns;
        
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
        int16 markIndex = 0;
        
        for(uint32 y = (k + (uint32)(topBorder*bitmap.info.height)); y < bitmap.info.height - k; y++){
            currentAverage = 0;
            uint32 averageIntensity = 0;
            for(uint32 x = k + (uint32)((bordersX/2)*bitmap.info.width); x < colW + (uint32)((bordersX/2)*bitmap.info.width); x++){
                if(gradients[y * bitmap.info.width + x].size > markGradientThreshold &&
                   bitmap.data[y * bitmap.info.width + x] > markIntensityThreshold){
                    
                    uint16 areasize = 0;
                    for(uint32 kX = x - k; kX < colW + (uint32)((bordersX/2)*bitmap.info.width) && kX < x + k + 1; kX++){
                        for(uint32 kY = y - k; kY < bitmap.info.height-1 && kY < y + k + 1; kY++){
                            pixelKArea[areasize] = bitmap.data[kY * bitmap.info.width + kX];
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
                        averageIntensity += bitmap.data[y*bitmap.info.width + x];
                        //bitmap.data[y * bitmap.info.width + x] = 255;
                    }else{
                        //bitmap.data[y * bitmap.info.width + x] = 0;
                    }
                }else{
                    //bitmap.data[y * bitmap.info.width + x] = 0;
                }
                
            }
            
            if(currentAverage > clusterWidthQuorum * colW){
                
                if(!lastCluster){
                    mark.startY = y;
                    clusterMinimum = averageIntensity / currentAverage;
                }
                lastCluster = true;
                /*for(uint32 x = 0; x < colW; x++){
                    bitmap.data[y*bitmap.info.width + x] = 100;
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
        /*
        FileContents bmp2;
        encodeBMP(&bitmap, &bmp2);
        saveFile("bmp.bmp", &bmp2);
        return;
        */
        FileContents fontFile;
        BitmapFont font;
        readFile("font.bmp", &fontFile);
        Image source;
        decodeBMP(&fontFile, &source);
        flipY(&source);
        initBitmapFont(&font, &source, source.info.width / 16); 
        
        ASSERT(markIndex == parameters->targetMark);
        
        const char * message = "FUCK";
        
        uint32 fontSize = 64;
        
        scaleCanvas(&bitmap, bitmap.info.width + fontSize * strlen(message), bitmap.info.height + colW, fontSize * strlen(message), 0);
        
        printToBitmap(&bitmap, 0, mark.startY, message, &font, fontSize);
        
        
        for(uint8 ci = 1; ci < parameters->columns; ci++){
            printToBitmap(&bitmap, fontSize * strlen(message) + ci*colW + (uint32)((bordersX/2)*bitmap.info.width), bitmap.info.height - colW,  "A", &font, colW);
        }
        
        POPI;
        
        
        
        FileContents bmp;
        encodeBMP(&bitmap, &bmp);
        saveFile("bmp.bmp", &bmp);
}