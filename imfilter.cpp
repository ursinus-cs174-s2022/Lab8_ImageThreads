#include <stdio.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <mutex>
#include <sstream>
#include <iostream>
#include <functional>
#include <math.h>
#include "simplecanvas/simplecanvas.h"

using namespace std;

typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds milliseconds;


struct Parameters {
    const char* inpath;
    const char* outpath;
    float s; // Spatial sigma
    float b; // Grayscale sigma
    int nthreads=1;
    int reps=1; // Number of repetitions of the filter
};

/**
 * @brief Parse the command line arguments that specify parameters
 *        for image processing
 * 
 * @param argc Number of command line arguments
 * @param argv Array of command line arguments
 * @return Parameters
 */
Parameters parseArgs(int argc, char** argv) {
    Parameters params;
    argv++, argc--;
    while (argc > 0) {
        if ((*argv)[0] == '-') {
            if (strcmp(*argv, "--help") == 0) {
                printf("Usage: ./imfilter --in <path to input> --out <path to output> --kernelWidth <width of kernel> --filter(mean/motionBlur)\n");
                exit(0);
            }
            else if (!strcmp(*argv, "--in")) {
                argv++; argc--;
                params.inpath = (const char*)*argv;
            }
            else if (!strcmp(*argv, "--out")) {
                argv++; argc--;
                params.outpath = (const char*)*argv;
            }
            else if (!strcmp(*argv, "--s")) {
                argv++; argc--;
                params.s = atof(*argv); // Spatial standard deviation
            }
            else if (!strcmp(*argv, "--b")) {
                argv++; argc--;
                params.b = atof(*argv); // Brightness standard deviation
            }
            else if (!strcmp(*argv, "--nthreads")) {
                argv++; argc--;
                params.nthreads = atoi(*argv);
            }
            else if (!strcmp(*argv, "--reps")) {
                argv++; argc--;
                params.reps = atoi(*argv);
            }
            else { 
                fprintf(stderr, "Invalid option: %s\n", *argv);
                exit(0);
            }
        }
        argv++, argc--; 
    }
    return params;
}

/**
 * Compute the intensity of a color pixel
 * @param rgb The red, green, blue describing the pixel
 */
float getIntensity(float* rgb) {
    return 0.2125*rgb[0] + 0.7154*rgb[1] + 0.0721*rgb[2];
}

/**
 * @brief Convert 8-bit color channels to float
 * 
 * @param imagein Input image
 * @param x X coordinate of pixel
 * @param y Y coordinate of pixel
 * @param rgb By reference 3-length float array to hold [0, 1] colors
 */
void getColor01(SimpleCanvas& imagein, int x, int y, float* rgb) {
    for (int k = 0; k < 3; k++) {
        rgb[k] = (float)(imagein.data[y][x][k])/255.0f;
    }
}

/**
 * @brief Compute the color of a pixel in an image by applying a bilateral filter
 * 
 * @param imagein Input image
 * @param x X coordinate of point to blur
 * @param y Y coordinate of point to blur
 * @param s Spatial standard deviation of filter
 * @param b Brightness standard deviation of filter
 * @param output Output color at the end
 */
void bilateralFilterPixel(SimpleCanvas& imagein, int x, int y, float s, float b, uint8_t* output) {
    float weights = 0.0;
    int support = (int)(s*3);
    int x1 = (x-support > 0) ? x-support: 0;
    int x2 = (x+support < imagein.width) ? x+support:imagein.width-1;
    int y1 = (y-support > 0) ? y-support: 0;
    int y2 = (y+support < imagein.height) ? y+support:imagein.height-1;
    float I1[3];
    float I2[3];
    float diff[3];
    float final[3];
    for (int k = 0; k < 3; k++) {
        final[k] = 0;
    }
    getColor01(imagein, x, y, I1);

    for (float xs = x1; xs <= x2; xs++) {
        for (int ys = y1; ys <= y2; ys++) {
            getColor01(imagein, xs, ys, I2);

            // Spatial blur factor
            float d1 = 0.0;
            if (s > 0) {
                d1 = ((xs-x)*(xs-x) + (ys-y)*(ys-y))/(2.0*s*s);
            }

            // Intensity blur factor
            float d2 = 0;
            if (b > 0) {
                for (int k = 0; k < 3; k++) {
                    diff[k] = I1[k]-I2[k];
                }
                float diffI = getIntensity(diff);
                d2 = diffI*diffI/(2.0*b*b);
            }            
            
            // Write final weighted color contribution
            float w = exp(-d1-d2);
            for (int k = 0; k < 3; k++) {
                final[k] += w*I2[k];
            }
            weights += w;
        }
    }
    // Quantize back to [0, 255] and save result
    for (int k = 0; k < 3; k++) {
        output[k] = (uint8_t)(255*final[k]/weights);
    }
}

/**
 * @brief Perform a bilateral filter on an image
 * 
 * @param imagein Input image
 * @param imageout Output image
 * @param params Parameters for filtering
 */
void filterImage(SimpleCanvas& imagein, SimpleCanvas& imageout, Parameters params) {
    uint8_t pixel[3];
    for (int rep = 0; rep < params.reps; rep++) {
        for (int y = 0; y < imagein.height; y++) {
            for (int x = 0; x < imagein.width; x++) {
                bilateralFilterPixel(imagein, x, y, params.s, params.b, pixel);
                imageout.setPixel(x, y, pixel[0], pixel[1], pixel[2]);
            }
        }
        if (rep < params.reps-1) {
            // Copy over image for next rep
            stringstream stream;
            stream << "rep" << rep << ".png";
            imageout.write(stream.str());
            for (int y = 0; y < imagein.height; y++) {
                for (int x = 0; x < imagein.width; x++) {
                    for (int k = 0; k < 3; k++) {
                        imagein.data[y][x][k] = imageout.data[y][x][k];
                    }
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    Parameters params = parseArgs(argc, argv);
    SimpleCanvas imagein(params.inpath);
    SimpleCanvas imageout(params.inpath);

    Clock::time_point tic = Clock::now();
    filterImage(imagein, imageout, params);
    Clock::time_point toc = Clock::now();
    milliseconds ms = std::chrono::duration_cast<milliseconds>(toc-tic);
    cout << "Time elapsed: " << ms.count()  << "ms\n";

    imageout.write(params.outpath);
    return 0;
}