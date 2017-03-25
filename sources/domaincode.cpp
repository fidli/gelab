#include "util_filesystem.h"

struct RunParameters{
    char file[256];
};



void run(RunParameters * parameters){
    FileContents imageFile;
    readFile(parameters->file, &imageFile);
    Image bitmap;
    convTiffToBitmap(&imageFile, &bitmap);
}