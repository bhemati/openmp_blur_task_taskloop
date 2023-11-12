#include <iostream>
#include <fstream>
#include <vector>
#include <tuple>
#include <time.h>
#include <cmath>
#include <complex>
#include <chrono>

#include "a2-helpers.hpp"

using namespace std;

// A set of random gradients, adjusted for this mandelbrot algorithm
vector<gradient> gradients = {
    gradient({0, 0, 0}, {76, 57, 125}, 0.0, 0.010, 2000),
    gradient({76, 57, 125}, {255, 255, 255}, 0.010, 0.020, 2000),
    gradient({255, 255, 255}, {0, 0, 0}, 0.020, 0.050, 2000),
    gradient({0, 0, 0}, {0, 0, 0}, 0.050, 1.0, 2000)};

// Test if point c belongs to the Mandelbrot set
bool mandelbrot_kernel(complex<double> c, vector<int> &pixel)
{
    int max_iterations = 2048, iteration = 0;
    complex<double> z(0, 0);

    while (abs(z) <= 4 && (iteration < max_iterations))
    {
        z = z * z + c;
        iteration++;
    }

    // now the computation of the color gradient and interpolation
    double length = sqrt(z.real() * z.real() + z.imag() * z.imag());
    long double m = (iteration + 1 - log(length) / log(2.0));
    double q = m / (double)max_iterations;

    q = iteration + 1 - log(log(length)) / log(2.0);
    q /= max_iterations;

    colorize(pixel, q, iteration, gradients);

    return (iteration < max_iterations);
}

/**
 * Compute the Mandelbrot set for each pixel of a given image.
 * Image is the Image data structure for storing RGB image
 * The default value for ratio is 0.15.
 * 
 * @param[inout] image
 * @param[in] ratio
 * 
*/
int mandelbrot(Image &image, double ratio = 0.15)
{
    int i, j; 
    int h = image.height;
    int w = image.width;
    int channels = image.channels;
    ratio /= 10.0;
    
    int pixels_inside=0;

    // pixel to be passed to the mandelbrot function
    vector<int> pixel = {0, 0, 0}; // red, green, blue (each range 0-255)
    complex<double> c;

    for (j = 0; j < h; j++)
    {
        for (i = 0; i < w; i++)
        {
            double dx = (double)i / (w)*ratio - 1.10;
            double dy = (double)j / (h)*0.1 - 0.35;

            c = complex<double>(dx, dy);

            if (mandelbrot_kernel(c, pixel)) // the actual mandelbrot kernel
                pixels_inside++;

            // apply to the image
            for (int ch = 0; ch < channels; ch++)
                image(ch, j, i) = pixel[ch];
        }
    }
    
    return pixels_inside;
}

/**
 * 2D Convolution
 * src is the source Image to which we apply the filter.
 * Resulting image is saved in dst. The size of the kernel is 
 * given with kernel_width (must be odd number). Sigma represents 
 * the standard deviation of the filter. The number of iterations
 * is given with the nstep (default=1)
 * 
 * @param[in] src
 * @param[out] dst
 * @param[in] kernel_width
 * @param[in] sigma
 * @param[in] nsteps
 * 
*/
void convolution_2d(Image &src, Image &dst, int kernel_width, double sigma, int nsteps=1)
{
    int h = src.height;
    int w = src.width;
    int channels = src.channels;

    std::vector<std::vector<double>> kernel = get_2d_kernel(kernel_width, kernel_width, sigma);

    int displ = (kernel.size() / 2); // height==width!
    for (int step = 0; step < nsteps; step++)
    {
        for (int ch = 0; ch < channels; ch++)
        {
            for (int i = 0; i < h; i++)
            {
                for (int j = 0; j < w; j++)
                {
                    double val = 0.0;

                    for (int k = -displ; k <= displ; k++)
                    {
                        for (int l = -displ; l <= displ; l++)
                        {
                            int cy = i + k;
                            int cx = j + l;
                            int src_val = 0;

                            // if it goes outside we disregard that value
                            if (cx < 0 || cx > w - 1 || cy < 0 || cy > h - 1) {
                                continue;
                            } else {
                                src_val = src(ch, cy, cx);
                            }
                            
                            val += kernel[k + displ][l + displ] * src_val;
                        }
                    }
                    dst(ch, i, j) = (int)(val > 255 ? 255 : (val < 0 ? 0 : val));
                }
            }
        }

        if ( step < nsteps-1 ) {
            // swap references
            // we can reuse the src buffer for this example
            Image tmp = src; src = dst; dst = tmp;
        }
    }
}

int main(int argc, char **argv)
{
    // height and width of the output image
    // keep the height/width ratio for the same image
    int width = 1536, height = 1024;
    double ratio = width / (double)height;

    double time;
    int i, j, pixels_inside = 0;

    int channels = 3; // red, green, blue

    // Generate Mandelbrot set int this image
    Image image(channels, height, width);

    // Save the results of 2D convolution in this image
    Image filtered_image(channels, height, width);

    auto t1 = chrono::high_resolution_clock::now();

    // Generate the mandelbrot set 
    // Use OpenMP tasking to implement a parallel version
    pixels_inside = mandelbrot(image, ratio);

    auto t2 = chrono::high_resolution_clock::now();

    cout << "Mandelbrot time: " << chrono::duration<double>(t2 - t1).count() << endl;
    cout << "Total Mandelbrot pixels: " << pixels_inside << endl;

    // Actual 2D convolution part
    // Use OpenMP tasking to implement a parallel version
    auto t3 = chrono::high_resolution_clock::now(); 

    convolution_2d(image, filtered_image, 5, 0.37, 20);

    auto t4 = chrono::high_resolution_clock::now();

    cout << "Convolution time: " << chrono::duration<double>(t4 - t3).count() << endl;

    cout << "Total time: " << chrono::duration<double>((t4 - t3) + (t2-t1)).count() << endl;

    // save image
    std::ofstream ofs("mandelbrot.ppm", std::ofstream::out);
    ofs << "P3" << std::endl;
    ofs << width << " " << height << std::endl;
    ofs << 255 << std::endl;

    for (int j = 0; j < height; j++)
    {
        for (int i = 0; i < width; i++)
        {
            ofs << " " << filtered_image(0, j, i) << " " << filtered_image(1, j, i) << " " << filtered_image(2, j, i) << std::endl;
        }
    }
    ofs.close();

    return 0;
}