#include "helpers.h"
#include <pthread.h>

typedef struct {
    int thread_id;
    int threads;
    int sigma;
    pthread_barrier_t *first_barrier;
    pthread_barrier_t *second_barrier;
    ppm_image *image;
    ppm_image *new_image;
    int step_x;
    int step_y;
    int check_create_threads;
    unsigned char **grid;
    ppm_image ** contour_map;
} pthread_structure;

int minim (int a, int b);

void *threads_run(void *arg);

void last_part (pthread_structure *thread_structure);
void update_image(ppm_image *image, ppm_image *contour, int x, int y) ;