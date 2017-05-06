    #include "util_filesystem.h"
    
    struct RunParameters{
        char file[256];
    };
    
    
    
    void run(RunParameters * parameters){
        FileContents imageFile;
        readFile(parameters->file, &imageFile);
        Image bitmap;
        decodeTiff(&imageFile, &bitmap);
        FileContents bmp;
        encodeBMP(&bitmap, &bmp);
        saveFile("bmp.bmp", &bmp);
        /*FileContents bmp2;
        readFile("data\\reexported.bmp", &bmp2);
        Image bitmap2;
        decodeBMP(&bmp2, &bitmap2);*/
}