/**
 * @file mandel.c
 * @brief creates the images for the video by shifting the viewport of the image
 * 
 * Course: CPE 2600
 * Section: 111
 * Assignment: Lab 11
 * Name: Walker Williams
 * 
 * Compile: gcc -o mandel mandel.c jpegrw.c -ljpeg
 *          ./mandel
 */
/// 
//  mandel.c
//  Based on example code found here:
//  https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html
//
//  Converted to use jpg instead of BMP and other minor changes
//  
///
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>
#include "jpegrw.h"

#define NUM_FRAMES 50
#define MAX_THREADS 20

typedef struct 
{
	imgRawImage *img;
	double xmin;
	double xmax;
	double ymin;
	double ymax;
	int max;
	int start_row;
	int end_row;
} thread_data_t;


// local routines
static int iteration_to_color( int i, int max );
static int iterations_at_point( double x, double y, int max );
static void compute_image( imgRawImage *img, double xmin, double xmax,
									double ymin, double ymax, int max, int num_threads );
static void* compute_image_thread(void *arg);
static void show_help();


int main( int argc, char *argv[] )
{
	char c;

	// These are the default configuration values used
	// if no command line arguments are given.
	char outfile[50] = "mandel.jpg";
	double xcenter = 0;
	double ycenter = 0;
	double xscale = 4;
	double yscale = 0; // calc later
	int    image_width = 1000;
	int    image_height = 1000;
	int    max = 1000;
	int    num_proc = 1;
	int    num_threads = 1;

	// For each command line argument given,
	// override the appropriate configuration value.
	
	while((c = getopt(argc,argv,"x:y:s:W:H:m:o:c:t:h"))!=-1) {
		switch(c) 
		{
			case 'x':
				xcenter = atof(optarg);
				break;
			case 'y':
				ycenter = atof(optarg);
				break;
			case 's':
				xscale = atof(optarg);
				break;
			case 'W':
				image_width = atoi(optarg);
				break;
			case 'H':
				image_height = atoi(optarg);
				break;
			case 'm':
				max = atoi(optarg);
				break;
			//case 'o': // Was causing issues with my file save method
			//	outfile = &optarg;
			//	break;
			case 'c':
				num_proc = atof(optarg);
				break;
			case 't':
				num_threads = atof(optarg);
				if (num_threads < 1)
				{
					num_threads = 1;
				}
				if (num_threads > MAX_THREADS)
				{
					num_threads = MAX_THREADS;
				}
				break;
			case 'h':
				show_help();
				exit(1);
				break;
		}
	}

	int active_children = 0;
	
	for (int i = 0; i < NUM_FRAMES; i++)
    {
        while (active_children >= num_proc) {
            wait(NULL);
            active_children--;
        }

        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork failed");
            exit(1);
        }
        else if (pid == 0)
        {
            snprintf(outfile, sizeof(outfile), "mandel%d.jpg", i);

            yscale = xscale / image_width * image_height;

            printf("mandel: x=%lf y=%lf xscale=%lf yscale=%1f max=%d outfile=%s\n",xcenter,ycenter,xscale,yscale,max,outfile);

            imgRawImage* img = initRawImage(image_width, image_height);
            setImageCOLOR(img, 0);

            compute_image(img, xcenter - xscale/2, xcenter + xscale/2, ycenter - yscale/2, ycenter + yscale/2, max, num_threads);

            storeJpegImageFile(img, outfile);

            freeRawImage(img);

            exit(0); 
        }
        else
        {
            active_children++;
            xscale *= 0.90;
        }
    }

    while (active_children > 0)
    {
        wait(NULL);
        active_children--;
    }

    return 0;
}

/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

int iterations_at_point( double x, double y, int max )
{
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while( (x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;

		x = xt;
		y = yt;

		iter++;
	}

	return iter;
}

/**
 * Thread helper function to compute part of the image
 */
void* compute_image_thread(void *arg)
{
    thread_data_t *data = (thread_data_t *)arg;
    
    int width = data->img->width;
    
    // For every pixel in the assigned row range...
    for(int j = data->start_row; j < data->end_row; j++) {
        for(int i = 0; i < width; i++) {
            
            // Determine the point in x,y space for that pixel.
            double x = data->xmin + i*(data->xmax-data->xmin)/width;
            double y = data->ymin + j*(data->ymax-data->ymin)/data->img->height;

            // Compute the iterations at that point.
            int iters = iterations_at_point(x, y, data->max);

            // Set the pixel in the image
            setPixelCOLOR(data->img, i, j, iteration_to_color(iters, data->max));
        }
    }
    
    pthread_exit(NULL);
}


/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void compute_image(imgRawImage* img, double xmin, double xmax, double ymin, double ymax, int max, int num_threads )
{
	int i,j;

	int width = img->width;
	int height = img->height;

	if (num_threads <= 1) {
		// For every pixel in the image...

		for(j=0;j<height;j++) {

			for(i=0;i<width;i++) {

				// Determine the point in x,y space for that pixel.
				double x = xmin + i*(xmax-xmin)/width;
				double y = ymin + j*(ymax-ymin)/height;

				// Compute the iterations at that point.
				int iters = iterations_at_point(x,y,max);

				// Set the pixel in the bitmap.
				setPixelCOLOR(img,i,j,iteration_to_color(iters,max));
			}
		}
	}
	else 
	{
		// Multithreaded code implementation
		pthread_t threads[num_threads];
		thread_data_t thread_data[num_threads];
		
		int rows_per_thread = height / num_threads;
		int extra_rows = height % num_threads;
		
		int current_row = 0;
		
		// Creates each thread
		for (int i = 0; i < num_threads; i++) 
		{
			thread_data[i].img = img;
			thread_data[i].xmin = xmin;
			thread_data[i].xmax = xmax;
			thread_data[i].ymin = ymin;
			thread_data[i].ymax = ymax;
			thread_data[i].max = max;
			thread_data[i].start_row = current_row;
			
			// Handles extra rows if they are not an even division into the threads
			int rows_this_thread = rows_per_thread + (i < extra_rows ? 1 : 0);
			thread_data[i].end_row = current_row + rows_this_thread;
			
			current_row = thread_data[i].end_row;
			
			if (pthread_create(&threads[i], NULL, compute_image_thread, &thread_data[i]) != 0) 
			{
				perror("Failed to create thread");
				exit(1);
			}
		}
		
		// Wait for all threads to finish
		for (int i = 0; i < num_threads; i++) 
		{
			if (pthread_join(threads[i], NULL) != 0) 
			{
				perror("Failed to join thread");
				exit(1);
			}
		}
	}

}


/*
Convert a iteration number to a color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/
int iteration_to_color( int iters, int max )
{
	int color = 0xFFFFFF*iters/(double)max;
	return color;
}


// Show help message
void show_help()
{
	printf("Use: mandel [options]\n");
	printf("Where options are:\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates (X-axis). (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=1000)\n");
	printf("-H <pixels> Height of the image in pixels. (default=1000)\n");
	printf("-o <file>   Set output file. (default=mandel.bmp)\n");
	printf("-h          Show this help text.\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}
