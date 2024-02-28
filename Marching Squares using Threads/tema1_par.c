// Author: APD team, except where source was noted

#include "helpers.h"
#include "threads.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define CONTOUR_CONFIG_COUNT 16
#define FILENAME_MAX_SIZE 50
#define STEP 8
#define SIGMA 200
#define RESCALE_X 2048
#define RESCALE_Y 2048

#define CLAMP(v, min, max) \
    if (v < min)           \
    {                      \
        v = min;           \
    }                      \
    else if (v > max)      \
    {                      \
        v = max;           \
    }

// Creates a map between the binary configuration (e.g. 0110_2) and the corresponding pixels
// that need to be set on the output image. An array is used for this map since the keys are
// binary numbers in 0-15. Contour images are located in the './contours' directory.
ppm_image **init_contour_map()
{
    ppm_image **map = (ppm_image **)malloc(CONTOUR_CONFIG_COUNT * sizeof(ppm_image *));
    if (!map)
    {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++)
    {
        char filename[FILENAME_MAX_SIZE];
        sprintf(filename, "./contours/%d.ppm", i);
        map[i] = read_ppm(filename);
    }

    return map;
}

// Updates a particular section of an image with the corresponding contour pixels.
// Used to create the complete contour image.
void update_image(ppm_image *image, ppm_image *contour, int x, int y)
{
    for (int i = 0; i < contour->x; i++)
    {
        for (int j = 0; j < contour->y; j++)
        {
            int contour_pixel_index = contour->x * i + j;
            int image_pixel_index = (x + i) * image->y + y + j;

            image->data[image_pixel_index].red = contour->data[contour_pixel_index].red;
            image->data[image_pixel_index].green = contour->data[contour_pixel_index].green;
            image->data[image_pixel_index].blue = contour->data[contour_pixel_index].blue;
        }
    }
}

// Corresponds to step 1 of the marching squares algorithm, which focuses on sampling the image.
// Builds a p x q grid of points with values which can be either 0 or 1, depending on how the
// pixel values compare to the `sigma` reference value. The points are taken at equal distances
// in the original image, based on the `step_x` and `step_y` arguments.
unsigned char **sample_grid(ppm_image *image, int step_x, int step_y, unsigned char sigma, pthread_structure *thread_structure, pthread_t *threads, int num_threads, int run_first_create)
{
    int p = image->x / step_x;
    int q = image->y / step_y;
    int r;

    unsigned char **grid = (unsigned char **)malloc((p + 1) * sizeof(unsigned char *));
    if (!grid)
    {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    grid[p] = (unsigned char *)malloc((q + 1) * sizeof(unsigned char));
    

    /*
        Actualizam datele thread-urilor
    */

    for (int i = 0; i < num_threads; i++)
    {
        thread_structure[i].image = image;
        thread_structure[i].step_x = step_x;
        thread_structure[i].step_y = step_y;
        thread_structure[i].grid = grid;
        thread_structure[i].sigma = sigma;
        thread_structure[i].thread_id = i;
    }

    /*
        In cazul in care am dat rescale la imagine, atunci thread-urile create
        anterior trebuie sa astepte pentru a se actualiza noile date, in caz
        contrar vom creea thread-urile
    */

    if (run_first_create == 1)
    {
        pthread_barrier_wait(thread_structure->second_barrier);
    }
    else
    {
        for (int i = 0; i < num_threads; i++)
        {
            r = pthread_create(&threads[i], NULL, threads_run, &thread_structure[i]);
            if (r)
            {
                printf("Eroare la creerea thread-ului %d\n", i);
                exit(-1);
            }
        }
    }

    /*
        Asteptam ca thread-urile sa isi termine executia pentru a
        returna grid-ul
    */

    for (int i = 0; i < num_threads; i++)
    {
        r = pthread_join(threads[i], NULL);
        if (r)
        {
            printf("Eroare la asteptarea thread-ului %d\n", i);
            exit(-1);
        }
    }

    return grid;
}

// Corresponds to step 2 of the marching squares algorithm, which focuses on identifying the
// type of contour which corresponds to each subgrid. It determines the binary value of each
// sample fragment of the original image and replaces the pixels in the original image with
// the pixels of the corresponding contour image accordingly.
void march(ppm_image *image, unsigned char **grid, ppm_image **contour_map, int step_x, int step_y)
{
    int p = image->x / step_x;
    int q = image->y / step_y;

    for (int i = 0; i < p; i++)
    {
        for (int j = 0; j < q; j++)
        {
            unsigned char k = 8 * grid[i][j] + 4 * grid[i][j + 1] + 2 * grid[i + 1][j + 1] + 1 * grid[i + 1][j];
            update_image(image, contour_map[k], i * step_x, j * step_y);
        }
    }
}

// Calls `free` method on the utilized resources.
void free_resources(ppm_image *image, ppm_image **contour_map, unsigned char **grid, int step_x)
{
    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++)
    {
        free(contour_map[i]->data);
        free(contour_map[i]);
    }
    free(contour_map);

    // for (int i = 0; i <= image->x / step_x; i++)
    // {
    //     free(grid[i]);
    // }
    free(grid[image->x/step_x]);
    free(grid);

    free(image->data);
    free(image);
}

ppm_image *rescale_image(ppm_image *image, int *run_first_create, pthread_structure *thread_structure, pthread_t *threads, int num_threads, pthread_barrier_t *second_barrier)
{

    int r;

    // we only rescale downwards
    if (image->x <= RESCALE_X && image->y <= RESCALE_Y)
    {
        return image;
    }

    *run_first_create = 1;

    // alloc memory for image
    ppm_image *new_image = (ppm_image *)malloc(sizeof(ppm_image));
    if (!new_image)
    {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }
    new_image->x = RESCALE_X;
    new_image->y = RESCALE_Y;

    new_image->data = (ppm_pixel *)malloc(new_image->x * new_image->y * sizeof(ppm_pixel));
    if (!new_image)
    {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
    }

    /*
        Adaugam date in structura thread-urilor
    */
    for (int i = 0; i < num_threads; i++)
    {
        thread_structure[i].image = new_image;
        thread_structure[i].new_image = image;
        thread_structure[i].check_create_threads = *run_first_create;
        thread_structure[i].thread_id = i;
    }

    /*
        Cream thread-urile in cazul in care trebuie sa rescalam imaginea
    */
    for (int i = 0; i < num_threads; i++)
    {
        r = pthread_create(&threads[i], NULL, threads_run, &thread_structure[i]);
        if (r)
        {
            printf("Eroare la creerea thread-ului %d\n", i);
            exit(-1);
        }
    }

    /*
        Asteptam ca thread-urile care merg in paralel sa formeze noua imagine
    */

    pthread_barrier_wait(second_barrier);

    free(image->data);
    free(image);

    return new_image;
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "Usage: ./tema1 <in_file> <out_file> <P>\n");
        return 1;
    }

    int num_threads = atoi(argv[3]);
    int r;

    pthread_barrier_t first_barrier;
    pthread_barrier_t second_barrier;
    r = pthread_barrier_init(&first_barrier, NULL, num_threads);
    if (r)
    {
        printf("Eroare la creearea primei bariere\n");
        exit(-1);
    }
    r = pthread_barrier_init(&second_barrier, NULL, num_threads + 1);
    if (r)
    {
        printf("Eroare la creearea celei de-a doua bariere\n");
        exit(-1);
    }
    pthread_t threads[num_threads];
    pthread_structure thread_structure[num_threads];

    int run_first_create = 0;

    for (int i = 0; i < num_threads; i++)
    {
        thread_structure[i].first_barrier = &first_barrier;
        thread_structure[i].second_barrier = &second_barrier;
        thread_structure[i].threads = num_threads;
        thread_structure[i].check_create_threads = run_first_create;
    }
    ppm_image *image = read_ppm(argv[1]);
    int step_x = STEP;
    int step_y = STEP;

    // 0. Initialize contour map
    ppm_image **contour_map = init_contour_map();

    for (int i = 0; i < num_threads; i++)
    {
        thread_structure[i].contour_map = contour_map;
    }

    // 1. Rescale the image
    ppm_image *scaled_image = rescale_image(image, &run_first_create, thread_structure, threads, num_threads, &second_barrier);

    // 2. Sample the grid
    unsigned char **grid = sample_grid(scaled_image, step_x, step_y, SIGMA, thread_structure, threads, num_threads, run_first_create);

    // 4. Write output
    write_ppm(scaled_image, argv[2]);

    free_resources(scaled_image, contour_map, grid, step_x);

    /*
        Distrugem barierele
    */

    r = pthread_barrier_destroy(&first_barrier);
    if (r)
    {
        printf("Eroare la distrugerea primei bariere\n");
        exit(-1);
    }
    r = pthread_barrier_destroy(&second_barrier);
    if (r)
    {
        printf("Eroare la distrugerea celei de-a doua bariere\n");
        exit(-1);
    }

    return 0;
}
