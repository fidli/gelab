    #include "util_filesystem.h"
    
    struct RunParameters{
        char file[256];
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
        
        
        
        
        
        //get the rotation angle automatically - locate image corners
        //lets do weighted HOG 100 x 100 tiles and find any left and right side
        ASSERT(bitmap.info.origin == BitmapOriginType_TopLeft);
        ASSERT(bitmap.info.interpretation = BitmapInterpretationType_GrayscaleBW01);
        PUSHI;
        PixelGradient * gradients = &PUSHA(PixelGradient, (bitmap.info.width) * (bitmap.info.height));
        uint8 * pixelArea = &PUSHA(uint8, 9);
        float32 tilesize = 0.01f;
        uint32 tileW = (uint32)(tilesize * bitmap.info.width);
        uint32 tileH = (uint32)(tilesize * bitmap.info.height);
        uint32 tilesCountX = (bitmap.info.width / tileW) + ((bitmap.info.width % tileW) != 0 ? 1 : 0);
        uint32 tilesCountY = bitmap.info.height / tileH + ((bitmap.info.height % tileH) != 0 ? 1 : 0);
        
        
        for(uint32 x = 0; x < bitmap.info.width; x = x + tileW){
            for(uint32 y = 0; y < bitmap.info.height; y = y + tileH){
                for(uint32 dX = x + 1; dX < tileW + x - 1; dX++){
                    for(uint32 dY = y + 1; dY < tileH + y - 1; dY++){
                        
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
                            int16 difference = ((int8) pixelArea[pi] - (int8) pixelArea[4]);
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
        
        for(uint32 x = 0; x < bitmap.info.width; x = x + tileW){
            for(uint32 y = 0; y < bitmap.info.height; y = y + tileH){
                for(uint32 dX = x + 1; dX < tileW + x - 1; dX++){
                    for(uint32 dY = y + 1; dY < tileH + y - 1; dY++){
                        bitmap.data[dY * bitmap.info.width + dX] = gradients[dY * bitmap.info.width + dX].size  > 2 ? 255 : 0;
                    }
                }
                
            }
        }
        
        
        
        //get the average gradient
        PixelGradient * average = &PUSHA(PixelGradient, tilesCountY * tilesCountX);
        
        uint32 * sums = &PUSHA(uint32, 9);
        uint32 * directions = &PUSHA(uint32, 9);
        
        for(uint8 i = 0; i < 9; i++){
            sums[i] = 0;
            directions[i] = 0;
        }
        
        
        uint32 tileIndexX = 0;
        uint32 tileIndexY = 0;
        for(uint32 x = 0; x < bitmap.info.width; x = x + tileW){
            tileIndexY = 0;
            for(uint32 y = 0; y < bitmap.info.height; y = y + tileH){
                PixelGradient * target = &average[tileIndexY * tilesCountX + tileIndexX];
                
                for(uint32 dX = x + 1; dX < tileW + x - 1; dX++){
                    for(uint32 dY = y + 1; dY < tileH + y - 1; dY++){
                        sums[gradients[dY * bitmap.info.width + dX].direction] += gradients[dY * bitmap.info.width + dX].size;
                        directions[gradients[dY * bitmap.info.width + dX].direction] += 1;
                    }
                }
                uint8 maxDirectionIndex = 4;
                for(uint8 i = 0; i < 9; i++){
                    if(directions[i] > directions[maxDirectionIndex]){
                        maxDirectionIndex = i;
                    }
                }
                if(directions[maxDirectionIndex] != 0){
                    target->size = (uint8)(sums[maxDirectionIndex] / directions[maxDirectionIndex]);
                    target->direction = maxDirectionIndex;
                }else{
                    target->size = 0;
                    target->direction = 4;
                }
                for(uint8 i = 0; i < 9; i++){
                    sums[i] = 0;
                    directions[i] = 0;
                }
                
                
                tileIndexY++;
            }
            tileIndexX++;
        }
        
        
        tileIndexX = 0;
        tileIndexY = 0;
        for(uint32 x = 0; x < bitmap.info.width; x = x + tileW){
            tileIndexY = 0;
            for(uint32 y = 0; y < bitmap.info.height; y = y + tileH){
                PixelGradient * source = &average[tileIndexY * tilesCountX + tileIndexX];
                for(uint32 dX = x + 1; dX < tileW + x - 1; dX++){
                    for(uint32 dY = y + 1; dY < tileH + y - 1; dY++){
                        bitmap.data[dY * bitmap.info.width + dX] = (source->size > 2) ? 128 : 0;
                        //(source->direction == 5) ? 255 : 0;
                    }
                }
                tileIndexY++;
            }
            tileIndexX++;
        }
        
        
        //find the longest leftmost top vertical  lane
        TileCoord topLeft = {0, 0};
        //find the longest leftmost bottom vertical  lane
        TileCoord botLeft = {0, tilesCountY - 1};
        //find the longest rightmost top vertical  lane
        TileCoord topRight = {tilesCountX - 1, 0};
        //find the longest rightmost bottom vertical  lane
        TileCoord botRight = {tilesCountX - 1, tilesCountY - 1};
        
        TileCoord topLeftBest = {0, 0};
        TileCoord botLeftBest = {0, tilesCountY - 1};
        TileCoord topRightBest = {tilesCountX - 1, 0};
        TileCoord botRightBest = {tilesCountX - 1, tilesCountY - 1};
        
        bool topLeftStreak = false;
        bool botLeftStreak = false;
        bool topRightStreak = false;
        bool botRightStreak = false;
        
        for(uint32 tileX = 0; tileX < tilesCountX; tileX++){
            for(uint32 tileY = 0; tileY < tilesCountY; tileY++){
                PixelGradient * source = &average[tileY * tilesCountX + tileX];
                if(source->size > 2){
                    //consider this tile as lit
                    if(topLeftStreak){
                        
                    }else{
                        topLeft.x = tileX;
                        topLeft.y = tileY;
                        topLeft.streak = 0;
                    }
                }
            }
        }
        
        
        POPI;
        
        
        FileContents bmp;
        encodeBMP(&bitmap, &bmp);
        saveFile("bmp.bmp", &bmp);
}