#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define DATA_OFFSET_OFFSET 0x000A
#define WIDTH_OFFSET 0x0012
#define HEIGHT_OFFSET 0x0016
#define BITS_PER_PIXEL_OFFSET 0x001C
#define HEADER_SIZE 14
#define INFO_HEADER_SIZE 40
#define NO_COMPRESION 0
#define MAX_NUMBER_OF_COLORS 0
#define ALL_COLORS_REQUIRED 0

typedef unsigned int int32;
typedef short int16;
typedef unsigned char byte;

//****Inputs*****
//fileName: The name of the file to open 
//
//****Outputs****
//pixels: A pointer to a byte array. This will contain the pixel data
//width: An int pointer to store the width of the image in pixels
//height: An int pointer to store the height of the image in pixels
//bytesPerPixel: An int pointer to store the number of bytes per pixel that are used in the image

void ReadImage(const char *fileName, byte **pixels, int32 *width, int32 *height, int32 *bytesPerPixel)
{
        //Open the file for reading in binary mode
        FILE *imageFile = fopen(fileName, "rb");

        //Read data offset
        int32 dataOffset;
        fseek(imageFile, DATA_OFFSET_OFFSET, SEEK_SET);
        fread(&dataOffset, 4, 1, imageFile);

        //Read width
        fseek(imageFile, WIDTH_OFFSET, SEEK_SET);
        fread(width, 4, 1, imageFile);

        //Read height
        fseek(imageFile, HEIGHT_OFFSET, SEEK_SET);
        fread(height, 4, 1, imageFile);

		printf("reading: %s\n", fileName);
		printf("width: %u height: %u\n", *width, *height);

        //Read bits per pixel
        int16 bitsPerPixel;
        fseek(imageFile, BITS_PER_PIXEL_OFFSET, SEEK_SET);
        fread(&bitsPerPixel, 2, 1, imageFile);

        //Allocate a pixel array
        *bytesPerPixel = ((int32)bitsPerPixel) / 8;

		printf("bytesPerPixel: %u\n", *bytesPerPixel);

        //Rows are stored bottom-up
        //Each row is padded to be a multiple of 4 bytes. 
        //We calculate the padded row size in bytes
        int paddedRowSize = (int)(4 * ceil((float)(*width) / 4.0f))*(*bytesPerPixel);

        //We are not interested in the padded bytes, so we allocate memory just for
        //the pixel data
        int unpaddedRowSize = (*width) * (*bytesPerPixel);

		// printf("unpaddedRowSize: %d\n", unpaddedRowSize);

        //Total size of the pixel data in bytes
        int totalSize = unpaddedRowSize * (*height);
        *pixels = (byte*)malloc(totalSize);

        //Read the pixel data Row by Row.
        //Data is padded and stored bottom-up
        int i = 0;

        //point to the last row of our pixel array (unpadded)
        byte *currentRowPointer = *pixels + ((*height - 1) * unpaddedRowSize);
        for (i = 0; i < *height; i++)
        {
			//put file cursor in the next row from top to bottom
	        fseek(imageFile, dataOffset+(i*paddedRowSize), SEEK_SET);

	        //read only unpaddedRowSize bytes (we can ignore the padding bytes)
	        fread(currentRowPointer, 1, unpaddedRowSize, imageFile);

	        //point to the next row (from bottom to top)
	        currentRowPointer -= unpaddedRowSize;
        }

        fclose(imageFile);
}

//***Inputs*****
//fileName: The name of the file to save 
//pixels: Pointer to the pixel data array
//width: The width of the image in pixels
//height: The height of the image in pixels
//bytesPerPixel: The number of bytes per pixel that are used in the image

void WriteImage(const char *fileName, byte *pixels, int32 width, int32 height,int32 bytesPerPixel)
{
        //Open file in binary mode
        FILE *outputFile = fopen(fileName, "wb");

        //*****HEADER************//
        //write signature
        const char *BM = "BM";
        fwrite(&BM[0], 1, 1, outputFile);
        fwrite(&BM[1], 1, 1, outputFile);

        //Write file size considering padded bytes
        int paddedRowSize = (int)(4 * ceil((float)width/4.0f))*bytesPerPixel;
        int32 fileSize = paddedRowSize*height + HEADER_SIZE + INFO_HEADER_SIZE;
        fwrite(&fileSize, 4, 1, outputFile);

        //Write reserved
        int32 reserved = 0x0000;
        fwrite(&reserved, 4, 1, outputFile);

        //Write data offset
        int32 dataOffset = HEADER_SIZE+INFO_HEADER_SIZE;
        fwrite(&dataOffset, 4, 1, outputFile);

        //*******INFO*HEADER******//
        //Write size
        int32 infoHeaderSize = INFO_HEADER_SIZE;
        fwrite(&infoHeaderSize, 4, 1, outputFile);

        //Write width and height
        fwrite(&width, 4, 1, outputFile);
        fwrite(&height, 4, 1, outputFile);

        //Write planes
        int16 planes = 1; //always 1
        fwrite(&planes, 2, 1, outputFile);

        //write bits per pixel
        int16 bitsPerPixel = bytesPerPixel * 8;
        fwrite(&bitsPerPixel, 2, 1, outputFile);

        //write compression
        int32 compression = NO_COMPRESION;
        fwrite(&compression, 4, 1, outputFile);

        //write image size (in bytes)
        int32 imageSize = width*height*bytesPerPixel;
        fwrite(&imageSize, 4, 1, outputFile);

        //write resolution (in pixels per meter)
        int32 resolutionX = 11811; //300 dpi
        int32 resolutionY = 11811; //300 dpi
        fwrite(&resolutionX, 4, 1, outputFile);
        fwrite(&resolutionY, 4, 1, outputFile);

        //write colors used 
        int32 colorsUsed = MAX_NUMBER_OF_COLORS;
        fwrite(&colorsUsed, 4, 1, outputFile);

        //Write important colors
        int32 importantColors = ALL_COLORS_REQUIRED;
        fwrite(&importantColors, 4, 1, outputFile);

        //write data
        int i = 0;
        int unpaddedRowSize = width*bytesPerPixel;
        for ( i = 0; i < height; i++)
        {
                //start writing from the beginning of last row in the pixel array
                int pixelOffset = ((height - i) - 1)*unpaddedRowSize;
                fwrite(&pixels[pixelOffset], 1, paddedRowSize, outputFile);	
        }
        fclose(outputFile);
}

const char *intenseChars = "-~=a#";

void ReadCell(byte *pixels, int32 unpaddedRowSize, int32 bytesPerPixel, int16 cellWidth, int16 cellHeight, byte cellX, byte cellY)
{

        // duplicate the pointer to the pixel data
        byte* pixelPointer = pixels;

        // offset the pointer to the cell location we want.
        // to get it to the right Y location, offset by the width of a row * number of rows in a cell * number of cells
        // to get it to the right X location, offset by the width of a cell * number of cells
        pixelPointer += cellY * unpaddedRowSize * cellHeight;
        pixelPointer += cellX * (cellWidth * bytesPerPixel);

        byte r;
        byte intensity;



        // for each row of pixels in any cell,
        for (int pd= 0; pd <= cellHeight; pd++) {

                // for each pixel in a row in any cell
                // cell pixel across
                for (int cpa = 0; cpa <= cellWidth; cpa++) {

                        // printf("%x,", *(pixelPointer + (cpa * bytesPerPixel)));
                        // printf("%x,", *(pixelPointer + (j * bytesPerPixel) + 1));
                        // printf("%x;", *(pixelPointer + (j * bytesPerPixel) + 2));
                
                        r = *(pixelPointer + (cpa * bytesPerPixel));

                        // should make a 0-4 integer
                        intensity = r / 63;
                        printf("%c", intenseChars[intensity]);
                        // printf("%u %c ", intensity, intenseChars[intensity]);


                        // if (r == 0) {
                        //         printf("-");
                        // } else {
                        //         printf("#");
                        // }

                }

                printf("\n");
                pixelPointer += unpaddedRowSize;
        }

}

// start char?
void BitmapFont(byte* pixels, int32 width, int32 height, int32 bytesPerPixel, int16 cellWidth, int16 cellHeight) 
{
        // repeated code
        int32 unpaddedRowSize = width * bytesPerPixel;

        int startChar = 32;
        int whichChar = 1;

        int cellsAcross = width / cellWidth;
        int cellsDown = height / cellHeight;

	int16 maxCells = 64;

	// for (int i = 0; i < maxCells; i++)
	// {
        //         printf("%u,", i % cellsAcross);
        //         printf("%u; ", i / cellsDown);
	// 	// printf("%c\n", i);
        // }



        int x = whichChar % cellsAcross;
        int y = whichChar / cellsDown;

        printf("x: %u y: %u\n", x, y);
        printf("char: %c\n", startChar + whichChar);

        ReadCell(pixels, unpaddedRowSize, bytesPerPixel, cellWidth, cellHeight, x, y);


}

// notes:
// if we're working with a bitmap font image where there's no gap between the last cell in a row and the end of the image
// then we can do special stuff, right
// i should figure out a way that incorporates that being a possibility

// terminal colors?

// ouutput hex?


int main()
{
        byte *pixels;
        int32 width;
        int32 height;
        int32 bytesPerPixel;

        // ReadImage("img.bmp", &pixels, &width, &height,&bytesPerPixel);
        ReadImage("ExportedFont.bmp", &pixels, &width, &height, &bytesPerPixel);

		

        // WriteImage("img2.bmp", pixels, width, height, bytesPerPixel);

	BitmapFont(pixels, width, height, bytesPerPixel, 32, 32);

        free(pixels);
        return 0;
}
