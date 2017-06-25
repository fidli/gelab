    #include "util_filesystem.h"
    
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
        
        //contrast the image
        
        
        //applyContrast(&bitmap, -50);
        
        
        
        
        
        uint8 thresholdX = 20;
        uint8 thresholdY = 4;
        
        
        
        int32 wj = bitmap.info.width / 10;
        {
            //sample 1
            bool foundTop = false;
            bool foundBot = false;
            int32 h1 = bitmap.info.height / 3;
            int32 h2 = h1 + bitmap.info.height / 10;
            
            
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
            
            v2 rotation = V2((float32)(w2 - w1),(float32)(h2 - h1));
            v2 down = V2(0, 1);
            float32 degAngle = radAngle(rotation, down) * 180 / PI;
            
            
            ASSERT(foundTop && foundBot);
            
            //sample finished
            
            rotateImage(&bitmap, degAngle, (float32)(w1/bitmap.info.width), (float32)(h1/bitmap.info.height));
        }
        
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
        
        uint32 yMiddle = 0;
        
        for(uint32 x = 1; x < bitmap.info.width - 1; x = x + tileW){
            for(uint32 y = 1 + 2*tileH; y < bitmap.info.height - 1; y = y + tileH){
                for(uint32 dX = x; dX < tileW + x; dX++){
                    for(uint32 dY = y; dY < tileH + y; dY++){
                        //bitmap.data[dY * bitmap.info.width + dX] = 0;
                        if(averages[(y)* bitmap.info.width + x] < 20 && averages[(y)* bitmap.info.width + x] > 10 &&
                           averages[(y - 2*tileH)* bitmap.info.width + x] < averages[(y - 1*tileH)* bitmap.info.width + x] &&
                           averages[(y - 1*tileH)* bitmap.info.width + x] < averages[(y)* bitmap.info.width + x] &&
                           averages[(y + 1*tileH)* bitmap.info.width + x] < averages[(y)* bitmap.info.width + x] &&
                           averages[(y + 2*tileH)* bitmap.info.width + x] < averages[(y + 1*tileH)* bitmap.info.width + x]){
                            yMiddle = y + tileH/2;
                        }
                        //bitmap.data[dY * bitmap.info.width + dX] = averages[y * bitmap.info.width + x];// (gradients[dY * bitmap.info.width + dX].size  > 5 &&
                        //bitmap.data[dY * bitmap.info.width + dX] = bitmap.data[dY * bitmap.info.width + dX] < 40)? 255 : 0;
                        //bitmap.data[dY * bitmap.info.width + dX] = gradients[dY * bitmap.info.width + dX].size > 5 ? 255 : 0;
                    }
                }
                
            }
        }
        
        cropImageY(&bitmap, yMiddle, 0);
        
        struct Cluster{
            uint32 startY;
            uint32 endY;
        };
        
        Cluster mark;
        
        uint32 colW = bitmap.info.width / parameters->columns;
        
        uint8 markGradientThreshold = 4;
        uint8 markIntensityThreshold = 20;
        uint16 k = 2;
        uint8 * pixelKArea = &PUSHA(uint8, k*k);
        bool lastCluster = false;
        uint32 currentAverage = 0;
        //kNN sort of clustering
        
        uint8 markIntensityDifferenceThreshold = 15;
        float32 clusterMembershipQuorum = 0.35f;
        float32 clusterWidthQuorum = 0.4f;
        
        //finding the left side mark
        int16 markIndex = 0;
        
        for(uint32 y = k; y < bitmap.info.height - 1; y++){
            currentAverage = 0;
            for(uint32 x = k; x < colW; x++){
                if(gradients[y * bitmap.info.width + x].size > markGradientThreshold &&
                   bitmap.data[y * bitmap.info.width + x] > markIntensityThreshold){
                    
                    uint16 areasize = 0;
                    for(uint32 kX = x - k; kX < colW && kX < x + k + 1; kX++){
                        for(uint32 kY = y - k; kY < bitmap.info.height-1 && kY < y + k + 1; kY++){
                            pixelKArea[areasize] = bitmap.data[kY * bitmap.info.width + kX];
                            areasize++;
                        }
                    } 
                    
                    uint16 members = 0;
                    for(uint16 pi = 0; pi < areasize; pi++){
                        int16 difference = ((int16) pixelKArea[pi] - (int16) pixelKArea[k*(k*2+1) + k]);
                        ASSERT(difference < 256);
                        if(ABS(difference) >= markIntensityDifferenceThreshold){
                            members++;
                        }
                    }
                    
                    if(members > clusterMembershipQuorum*(2*k+1)*(2*k+1)){
                        currentAverage += 1;
                        //bitmap.data[y * bitmap.info.width + x] = 255;
                    }else{
                        //bitmap.data[y * bitmap.info.width + x] = 0;
                    }
                }else{
                    //bitmap.data[y * bitmap.info.width + x] = 0;
                }
                
            }
            if(currentAverage > clusterWidthQuorum * colW){
                /*for(uint32 x = 0; x < colW; x++){
                    bitmap.data[y*bitmap.info.width + x] = 100;
                }*/
                if(!lastCluster){
                    mark.startY = y;
                }
                lastCluster = true;
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
        
        
        
        POPI;
        
        
        
        FileContents bmp;
        encodeBMP(&bitmap, &bmp);
        saveFile("bmp.bmp", &bmp);
}