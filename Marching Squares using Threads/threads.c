
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "helpers.h"
#include "threads.h"

void *threads_run(void *arg)
{

    /*
        Extragem datele date ca argumente functiei
    */

    pthread_structure thread_structure = *(pthread_structure *)arg;

    /*
        Cand hread_structure.check_create_threads == 0 vom paraleliza doar
        sample_grid si march() in caz contrat pe langa cele doua functii vom
        paraleliza si rescale_image()
    */

    if (thread_structure.check_create_threads == 0)
    {
        last_part(&thread_structure);
    }
    else
    {
        /*
            impartim task-urile thread-urilor in mod egal
        */
        int start = thread_structure.thread_id * thread_structure.image->x / thread_structure.threads;
        int end = minim((thread_structure.thread_id + 1) * thread_structure.image->x / thread_structure.threads, thread_structure.image->x);

        for (int i = start; i < end; i++)
        {
            for (int j = 0; j < thread_structure.image->y; j++)
            {
                uint8_t sample[3];
                sample[0] = 1;
                sample[1] = 1;
                float u = (float)i / (float)(thread_structure.image->x - 1);
                float v = (float)j / (float)(thread_structure.image->y - 1);
                sample_bicubic(thread_structure.new_image, u, v, sample);

                thread_structure.image->data[i * thread_structure.image->y + j].red = sample[0];
                thread_structure.image->data[i * thread_structure.image->y + j].green = sample[1];
                thread_structure.image->data[i * thread_structure.image->y + j].blue = sample[2];
            }
        }

        /*
            Prima bariera asteapta pentru ca thread-ul principal sa returneze noua imagine,
            iar a doua bariera astepta pentru reactualizarea datelor
        */

        pthread_barrier_wait(thread_structure.second_barrier);
        pthread_barrier_wait(thread_structure.second_barrier);

        thread_structure = *(pthread_structure *)arg;

        last_part(&thread_structure);
    }

    return NULL;
}

void last_part(pthread_structure *thread_structure)
{

    int p = thread_structure->image->x / thread_structure->step_x;
    int q = thread_structure->image->y / thread_structure->step_y;

    /*
           impartim task-urile thread-urilor in mod egal
   */

    int start = thread_structure->thread_id * (double)p / thread_structure->threads;
    int end = minim((thread_structure->thread_id + 1) * (double)p / thread_structure->threads, p);

    for(int i = start ; i < end ; i++){
        thread_structure->grid[i] = (unsigned char *)malloc((q + 1) * sizeof(unsigned char));
        if (!thread_structure->grid[i])
        {
            fprintf(stderr, "Unable to allocate memory\n");
            exit(1);
        }
    }

    pthread_barrier_wait(thread_structure->first_barrier);

    for (int i = start; i < end; i++)
    {
        for (int j = 0; j < q; j++)
        {
            ppm_pixel curr_pixel = thread_structure->image->data[i * thread_structure->step_x * thread_structure->image->y + j * thread_structure->step_y];

            unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

            if (curr_color > thread_structure->sigma)
                thread_structure->grid[i][j] = 0;
            else
                thread_structure->grid[i][j] = 1;
        }
    }

    for (int i = start; i < end; i++)
    {
        ppm_pixel curr_pixel = thread_structure->image->data[i * thread_structure->step_x * thread_structure->image->y + thread_structure->image->x - 1];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > thread_structure->sigma)
            thread_structure->grid[i][q] = 0;
        else
            thread_structure->grid[i][q] = 1;
    }

    /*
        impartim task-urile thread-urilor in mod egal
    */

    int start2 = thread_structure->thread_id * (double)q / thread_structure->threads;
    int end2 = minim((thread_structure->thread_id + 1) * (double)q / thread_structure->threads, q);

    for (int j = start2; j < end2; j++)
    {
        ppm_pixel curr_pixel = thread_structure->image->data[(thread_structure->image->x - 1) * thread_structure->image->y + j * thread_structure->step_y];

        unsigned char curr_color = (curr_pixel.red + curr_pixel.green + curr_pixel.blue) / 3;

        if (curr_color > thread_structure->sigma)
            thread_structure->grid[p][j] = 0;
        else
            thread_structure->grid[p][j] = 1;
    }

    /*
        Asteptam ca tot grid-ul sa fie complet
    */

    pthread_barrier_wait(thread_structure->first_barrier);

    /*
            impartim task-urile thread-urilor in mod egal
    */

    int start3 = thread_structure->thread_id * (double)p / thread_structure->threads;
    int end3 = minim((thread_structure->thread_id + 1) * (double)p / thread_structure->threads, p);

    for (int i = start3; i < end3; i++)
    {
        for (int j = 0; j < q; j++)
        {
            unsigned char k = 8 * thread_structure->grid[i][j] + 4 * thread_structure->grid[i][j + 1] + 2 * thread_structure->grid[i + 1][j + 1] + 1 * thread_structure->grid[i + 1][j];
            update_image(thread_structure->image, thread_structure->contour_map[k], i * thread_structure->step_x, j * thread_structure->step_y);
        }
    }

    pthread_barrier_wait(thread_structure->first_barrier);

    /*
        Eliberam memoria alocata pentru grid
    */

    for (int i = start; i < end; i++)
    {
        free(thread_structure->grid[i]);
    }
}

int minim(int a, int b)
{
    return (a > b) ? b : a;
}