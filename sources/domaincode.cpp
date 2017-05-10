    #include "util_filesystem.h"
    
    struct RunParameters{
        char file[256];
    };
    
    
    
    void run(RunParameters * parameters){
        FileContents imageFile;
        readFile(parameters->file, &imageFile);
        Image bitmap;
        decodeTiff(&imageFile, &bitmap);
        
        //get the rotation angle automatically - locate image corners
        //lets do HOG 100 x 100 tiles and find any left and right side
        ASSERT(image.info.origin == BitmapOriginType_TopLeft);
        ASSERT(image.info.interpretation = BitmapInterpretationType_GrayscaleBW01);
        PUSHI;
        uint8 hogTile = &PUSHA(uint8, image.info.width * image.info.height * 0.01);
        uint8 hogAvgs = &PUSHA(uint8, 10);
        uint32 y = 0.45 * bitmap.info.height;
        for(uint32 x = 0; x < bitmap.info.width; x = x + 0.1 * bitmap.info.width){
            for(uint32 dx = 1; dx < 0.1 * bitmap.info.width - 1; dx++){
                for(uint32 dy = 1; dy < 0.1 * bitmap.info.height - 1; dy++){
                    
                    bitmap.data[(y + dy) * bitmap.image.width + x + dx]
                }
            }
            
        }
        POPI;
        rotateImage(&bitmap, 45);
        
        FileContents bmp;
        encodeBMP(&bitmap, &bmp);
        saveFile("bmp.bmp", &bmp);
}