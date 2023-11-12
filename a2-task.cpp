#include <iostream>
#include <fstream>
#include <vector>
#include <tuple>
#include <time.h>
#include <cmath>
#include <complex>
#include <chrono>

#include "a2-helpers.hpp"

#include <numeric>
#include <omp.h>
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
int mandelbrot(Image &image, double ratio = 0.15, int task_num=1, int task_size=1)
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

    int w_size = w/task_size;
    int h_size = h/task_size;
    for (i = task_num * w_size; i < (task_num+1) * w_size; i++)
    {
        for (j = 0; j < h; j++)
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

void convolution_2d_helper(Image &src, Image &dst, int kernel_width, double sigma, int nsteps=1, int task_num=1, int task_size=1)
{
    int h = src.height;
    int w = src.width;
    int channels = src.channels;

    std::vector<std::vector<double>> kernel = get_2d_kernel(kernel_width, kernel_width, sigma);

    int w_size = w/task_size;
    int h_size = h/task_size;

    int displ = (kernel.size() / 2); // height==width!
        
        for (int j = task_num * w_size; j < (task_num+1) * w_size; j++)
            {
            for (int ch = 0; ch < channels; ch++)
            {
                for (int i = 0; i < h; i++)
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
}

void convolution_2d(Image &src, Image &dst, int kernel_width, double sigma, int nsteps=1)
{
    int task_size = 256;
    for (int step = 0; step < nsteps; step++)
    {
        #pragma omp parallel num_threads(16) default(none) firstprivate(task_size,nsteps,sigma,kernel_width) shared(dst,src)
        {
            #pragma omp single 
                {
                    for (int i = 0; i<task_size; i++)
                    {
                        #pragma omp task default(none) firstprivate(i,task_size,nsteps,sigma,kernel_width) shared(dst,src)
                        convolution_2d_helper(src, dst, kernel_width, sigma, nsteps, i, task_size);
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


    // auto t1 = chrono::high_resolution_clock::now();
    auto t1 = omp_get_wtime();
    // Generate the mandelbrot set 
    // Use OpenMP tasking to implement a parallel version
    
    int task_size = 512;
    vector<int> pixels_inside_local(task_size, 0);
    #pragma omp parallel default(none) firstprivate(task_size,ratio) shared(image,pixels_inside_local) num_threads(16)
    {
        #pragma omp single
        {
            for (int i = 0; i<task_size; i++) {
                #pragma omp task default(none) firstprivate(task_size,ratio,i) shared(image,pixels_inside_local)
                pixels_inside_local[i]=(mandelbrot(image, ratio, i, task_size));
            }
        }
    }
    pixels_inside = std::accumulate(pixels_inside_local.begin(), pixels_inside_local.end(), 0);
    auto t2 = omp_get_wtime();

    std::cout << "Mandelbrot time: " << chrono::duration<double>(t2 - t1).count() << endl;
    std::cout << "Total Mandelbrot pixels(1478025): " << pixels_inside << endl;

    // Actual 2D convolution part
    // Use OpenMP tasking to implement a parallel version

    auto t3 = omp_get_wtime();

    convolution_2d(image, filtered_image, 5, 0.37, 20);

    auto t4 = omp_get_wtime();


    std::cout << "Convolution time: " << chrono::duration<double>(t4 - t3).count() << endl;

    std::cout << "Total time: " << chrono::duration<double>((t4 - t3) + (t2-t1)).count() << endl;
    // save image

    std::ofstream ofs("mandelbrot-task.ppm", std::ofstream::out);
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
