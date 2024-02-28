#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

struct file_struct
{
    int rank;
    char filename[MAX_FILENAME];
    char **hashes;
    int chunks;

} file_struct;

struct file_out
{
    char **file_name;
    int files_out;
    int rank;
} file_out;

struct upload_struct
{
    int rank;
    int numtasks;
} upload_struct;

int final = 0;
int sent = 0;

void send_and_receive(struct file_out *file_out_struct, struct file_struct **file_in, int *files_in_1, int is_malloced)
{
    int files_in = *files_in_1;

    MPI_Send(&file_out_struct->files_out, 1, MPI_INT, TRACKER_RANK, file_out_struct->rank, MPI_COMM_WORLD); // trimitem numarul de fisiere pe care trebuie sa le scriem
    for (int i = 0; i < file_out_struct->files_out; i++)
    {
        MPI_Send(file_out_struct->file_name[i], MAX_FILENAME, MPI_CHAR, TRACKER_RANK, file_out_struct->rank, MPI_COMM_WORLD); // trimitem numele fisierelor pe care trebuie sa le scriem
    }

    MPI_Recv(&files_in, 1, MPI_INT, TRACKER_RANK, file_out_struct->rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // primim numarul de fisiere de intrare

    if (is_malloced == 0)
        *file_in = (struct file_struct *)malloc(files_in * sizeof(struct file_struct)); // alocam memorie pentru fisierele de intrare

    for (int i = 0; i < files_in; i++) // primim fisierele de intrare
    {
        MPI_Recv(&(*file_in)[i].rank, 1, MPI_INT, TRACKER_RANK, file_out_struct->rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv((*file_in)[i].filename, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, file_out_struct->rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&(*file_in)[i].chunks, 1, MPI_INT, TRACKER_RANK, file_out_struct->rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        if (is_malloced == 0)
            (*file_in)[i].hashes = (char **)malloc((*file_in)[i].chunks * sizeof(char *));
        for (int j = 0; j < (*file_in)[i].chunks; j++)
        {
            if (is_malloced == 0)
                (*file_in)[i].hashes[j] = (char *)malloc(HASH_SIZE * sizeof(char));
            MPI_Recv((*file_in)[i].hashes[j], HASH_SIZE, MPI_CHAR, TRACKER_RANK, file_out_struct->rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
    }

    *files_in_1 = files_in;
}

void ACK_statement(int ACK, struct file_out *file_out_struct, struct file_struct **file_in, int *counter, struct file_struct *file_out, int i, int j, int k)
{
    if (ACK == 0)
    {
        MPI_Send((*file_in)[j].hashes[k], HASH_SIZE, MPI_CHAR, (*file_in)[j].rank, 999, MPI_COMM_WORLD); // trimitem chunk-ul

        MPI_Status status;

        MPI_Recv(&ACK, 1, MPI_INT, (*file_in)[j].rank, 990, MPI_COMM_WORLD, &status); // primim ACK

        if (ACK == 1) // daca e ACK-ul asteptat
        {
            strcpy(file_out[i].hashes[k], (*file_in)[j].hashes[k]); // copiem chunk-ul in fisierul de iesire

            (*counter)++;

            if ((*counter) == 10) // daca counter e 10, trimitem o actualizare la tracker
            {
                ACK = 4;
                MPI_Send(&ACK, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD); // trimitem cererea

                (*counter) = 0;

                send_and_receive(file_out_struct, file_in, &ACK, 1); // primim fisierele de intrare actualizate

                MPI_Recv(&ACK, 1, MPI_INT, TRACKER_RANK, file_out_struct->rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // primim ACK
            }
        }
    }
    else if (ACK == 1)
    {
        MPI_Send(&ACK, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);                                        // trimitem ACK
        MPI_Recv(&ACK, 1, MPI_INT, TRACKER_RANK, file_out_struct->rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // primim ACK

        if (ACK == 1)
        {
            printf("EROARE\n");
            exit(-1);
        }
    }
    else if (ACK == 2)
    {

        MPI_Send(&ACK, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);                                        // trimitem ACK
        MPI_Recv(&ACK, 1, MPI_INT, TRACKER_RANK, file_out_struct->rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // primim ACK

        if (ACK == 2)
        {
            printf("EROARE\n");
            exit(-1);
        }
    }
    else if (ACK == 4) // daca primim ACK = 4, atunci am terminat toate fisierele de scris
    {
        MPI_Send("TESTTESTTESTTESTTESTTESTTESTTEST", HASH_SIZE, MPI_CHAR, file_out_struct->rank, 999, MPI_COMM_WORLD); // trimitem cererea
        final = 1;
        MPI_Recv(&ACK, 1, MPI_INT, file_out_struct->rank, 990, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // primim ACK
    }
}

void *download_thread_func(void *arg)
{
    struct file_out *file_out_struct = (struct file_out *)arg; // structura cu fisierele pe care trebuie sa le scriem
    struct file_struct *file_in = NULL;
    int files_in;

    send_and_receive(file_out_struct, &file_in, &files_in, 0); // trimitem si primim fisierele de intrare

    struct file_struct *file_out;
    file_out = (struct file_struct *)malloc(file_out_struct->files_out * sizeof(struct file_struct)); // alocam memorie pentru fisierele de iesire

    for (int i = 0; i < file_out_struct->files_out; i++) // scriem fisierele de iesire
    {
        file_out[i].rank = file_out_struct->rank;
        strcpy(file_out[i].filename, file_out_struct->file_name[i]);
        file_out[i].chunks = 0;
        file_out[i].hashes = NULL;

        for (int j = 0; j < files_in; j++)
        {
            if (strcmp(file_out[i].filename, file_in[j].filename) == 0) // daca numele fisierului de iesire este acelasi cu cel al unui fisier de intrare
            {
                int counter = 0;
                file_out[i].chunks = file_in[j].chunks; // copiem numarul de chunk-uri
                file_out[i].hashes = (char **)malloc(file_out[i].chunks * sizeof(char *));
                for (int k = 0; k < file_out[i].chunks; k++)
                {
                    file_out[i].hashes[k] = (char *)malloc(HASH_SIZE * sizeof(char));

                    ACK_statement(0, file_out_struct, &file_in, &counter, file_out, i, j, k); // trimitem si primim chunk-urile
                }

                ACK_statement(1, file_out_struct, &file_in, &counter, file_out, i, j, 0); // trimitem ACK
            }
        }
        FILE *g; // scriem in fisierul de iesire
        char filename[MAX_FILENAME + 10];
        sprintf(filename, "client%d_%s", file_out_struct->rank, file_out[i].filename);
        g = fopen(filename, "w");
        for (int k = 0; k < file_out[i].chunks; k++)
        {
            fprintf(g, "%s\n", file_out[i].hashes[k]);
        }
        fclose(g);
    }

    ACK_statement(2, file_out_struct, &file_in, NULL, NULL, 0, 0, 0); // trimitem ACK

    int ACK = 0;

    MPI_Recv(&ACK, 1, MPI_INT, TRACKER_RANK, file_out_struct->rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    ACK_statement(ACK, file_out_struct, &file_in, NULL, NULL, 0, 0, 0); // trimitem ACK

    for (int i = 0; i < file_out_struct->files_out; i++)
    {
        // Free the hashes array
        for (int k = 0; k < file_out[i].chunks; k++)
        {
            free(file_out[i].hashes[k]);
        }
        free(file_out[i].hashes);
    }

    // Free the file_out structure
    free(file_out);

    for (int j = 0; j < files_in; j++)
    {
        free(file_in[j].hashes);
    }
    free(file_in);

    return NULL;
}

void *upload_thread_func(void *arg)
{
    while (1) // trimitem si primim fisierele de iesire
    {
        if (final == 0) // daca inca primeste cereri
        {
            int ACK = 0;
            MPI_Status status;
            char buffer[HASH_SIZE];
            MPI_Recv(buffer, HASH_SIZE + 10, MPI_CHAR, MPI_ANY_SOURCE, 999, MPI_COMM_WORLD, &status); // primeste cerere
            int source = status.MPI_SOURCE;

            ACK = 1;
            MPI_Send(&ACK, 1, MPI_INT, source, 990, MPI_COMM_WORLD); // trimite ACK
        }
        if (final == 1) // daca nu mai primeste cereri
        {
            break;
        }
    }

    return NULL;
}

void ACK_while(int files_in, int numtasks, struct file_struct *file_in)
{
    int nr = 0;
    while (1)
    {
        int ACK = -1;
        MPI_Status status;
        MPI_Recv(&ACK, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status); // primim ACK
        int i = status.MPI_SOURCE;
        switch (ACK)
        {
        case 0: // in cazul in care e 0, atunci vom confirma ca am primit mesajul
            ACK = 1;
            MPI_Send(&ACK, 1, MPI_INT, i, i, MPI_COMM_WORLD);
            break;
        case 1: // in cazul in care e 1, atunci vom confirma ca am primit mesajul
            ACK = 2;
            MPI_Send(&ACK, 1, MPI_INT, i, i, MPI_COMM_WORLD);
            break;
        case 2: // in cazul in care e 2, atunci confirmam ca am primit mesajul si aduman la nr 1 pentru a verifica daca au terminat toti clientii
            ACK = 3;
            MPI_Send(&ACK, 1, MPI_INT, i, i, MPI_COMM_WORLD);
            nr++;
            break;
        case 4: // in cazul in care e 4, atunci vom reactualiza swarm-ul
            ACK = 5;

            struct file_out file_out_struct;
            MPI_Recv(&file_out_struct.files_out, 1, MPI_INT, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            file_out_struct.file_name = (char **)malloc(file_out_struct.files_out * sizeof(char *));
            for (int j = 0; j < file_out_struct.files_out; j++)
            {
                file_out_struct.file_name[j] = (char *)malloc(MAX_FILENAME * sizeof(char));
                MPI_Recv(file_out_struct.file_name[j], MAX_FILENAME, MPI_CHAR, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            int counter = 0;

            for (int j = 0; j < file_out_struct.files_out; j++)
            {
                for (int k = 0; k < files_in; k++)
                {
                    if (strcmp(file_out_struct.file_name[j], file_in[k].filename) == 0 && file_out_struct.rank != file_in[k].rank)
                    {
                        counter++;
                    }
                }
            }

            MPI_Send(&counter, 1, MPI_INT, i, i, MPI_COMM_WORLD);

            for (int j = 0; j < file_out_struct.files_out; j++)
            {
                for (int k = 0; k < files_in; k++)
                {
                    if (strcmp(file_out_struct.file_name[j], file_in[k].filename) == 0 && file_out_struct.rank != file_in[k].rank)
                    {
                        MPI_Send(&file_in[k].rank, 1, MPI_INT, i, i, MPI_COMM_WORLD);
                        MPI_Send(file_in[k].filename, MAX_FILENAME, MPI_CHAR, i, i, MPI_COMM_WORLD);
                        MPI_Send(&file_in[k].chunks, 1, MPI_INT, i, i, MPI_COMM_WORLD);
                        for (int l = 0; l < file_in[k].chunks; l++)
                        {
                            MPI_Send(file_in[k].hashes[l], HASH_SIZE, MPI_CHAR, i, i, MPI_COMM_WORLD);
                        }
                    }
                }
            }

            MPI_Send(&ACK, 1, MPI_INT, i, i, MPI_COMM_WORLD);
            break;
        default:
            break;
        }
        if (nr == numtasks - 1) // daca toti clientii au terminat, atunci trimitem ACK = 4 pentru a le spune sa se opreasca
        {
            for (int i = 1; i < numtasks; i++)
            {
                ACK = 4;
                MPI_Send(&ACK, 1, MPI_INT, i, i, MPI_COMM_WORLD);
            }
            break;
        }
    }
}
void tracker(int numtasks, int rank)
{

    int files_in = 0;
    struct file_struct *file_in;

    for (int i = 1; i < numtasks; i++)
    {
        int counter = 0;
        MPI_Recv(&counter, 1, MPI_INT, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // primim numarul de fisiere de intrare
        files_in += counter;
    }

    file_in = (struct file_struct *)malloc(files_in * sizeof(struct file_struct)); // alocam memorie pentru fisierele de intrare
    int index = 0;

    for (int i = 1; i < numtasks; i++) // primim fisierele de intrare
    {
        int counter = 0;
        MPI_Recv(&counter, 1, MPI_INT, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for (int j = 0; j < counter; j++)
        {
            file_in[index].rank = i;
            MPI_Recv(file_in[index].filename, MAX_FILENAME, MPI_CHAR, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&file_in[index].chunks, 1, MPI_INT, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            file_in[index].hashes = (char **)malloc(file_in[index].chunks * sizeof(char *));
            for (int k = 0; k < file_in[index].chunks; k++)
            {
                file_in[index].hashes[k] = (char *)malloc(HASH_SIZE * sizeof(char));
                MPI_Recv(file_in[index].hashes[k], HASH_SIZE, MPI_CHAR, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            index++;
        }
    }

    for (int i = 1; i < numtasks; i++) // trimitem ACK
    {
        int ok = 1;
        MPI_Send(&ok, 1, MPI_INT, i, i, MPI_COMM_WORLD);
    }

    for (int i = 1; i < numtasks; i++)
    {
        struct file_out file_out_struct;
        MPI_Recv(&file_out_struct.files_out, 1, MPI_INT, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // primim numarul de fisiere pe care trebuie sa le scriem
        file_out_struct.file_name = (char **)malloc(file_out_struct.files_out * sizeof(char *));
        for (int j = 0; j < file_out_struct.files_out; j++)
        {
            file_out_struct.file_name[j] = (char *)malloc(MAX_FILENAME * sizeof(char));
            MPI_Recv(file_out_struct.file_name[j], MAX_FILENAME, MPI_CHAR, i, i, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // primim numele fisierelor pe care trebuie sa le scriem
        }

        int counter = 0;

        for (int j = 0; j < file_out_struct.files_out; j++)
        {
            for (int k = 0; k < files_in; k++)
            {
                if (strcmp(file_out_struct.file_name[j], file_in[k].filename) == 0 && file_out_struct.rank != file_in[k].rank)
                {
                    counter++;
                }
            }
        }

        MPI_Send(&counter, 1, MPI_INT, i, i, MPI_COMM_WORLD);

        for (int j = 0; j < file_out_struct.files_out; j++) // trimitem fisierele de intrare
        {
            for (int k = 0; k < files_in; k++)
            {
                if (strcmp(file_out_struct.file_name[j], file_in[k].filename) == 0 && file_out_struct.rank != file_in[k].rank)
                {
                    MPI_Send(&file_in[k].rank, 1, MPI_INT, i, i, MPI_COMM_WORLD);
                    MPI_Send(file_in[k].filename, MAX_FILENAME, MPI_CHAR, i, i, MPI_COMM_WORLD);
                    MPI_Send(&file_in[k].chunks, 1, MPI_INT, i, i, MPI_COMM_WORLD);
                    for (int l = 0; l < file_in[k].chunks; l++)
                    {
                        MPI_Send(file_in[k].hashes[l], HASH_SIZE, MPI_CHAR, i, i, MPI_COMM_WORLD);
                    }
                }
            }
        }
    }
    ACK_while(files_in, numtasks, file_in); // asteptam ACK-uri

    for (int i = 0; i < files_in; i++)
    {
        for (int j = 0; j < file_in[i].chunks; j++)
        {
            free(file_in[i].hashes[j]);
        }
        free(file_in[i].hashes);
    }

    free(file_in);
}

void peer(int numtasks, int rank)
{
    pthread_t download_thread;
    pthread_t upload_thread;
    int r;

    FILE *f; // luam fisierul de intrare cu numele fisierului si un buffer
    char filename[MAX_FILENAME];
    char buffer[HASH_SIZE + 3];

    sprintf(filename, "in%d.txt", rank); // numele fisierului de intrare
    f = fopen(filename, "r");            // deschidere fisier

    if (f == NULL)
    {
        printf("Eroare la deschiderea fisierului %s\n", filename);
        exit(-1);
    }

    fgets(buffer, HASH_SIZE + 1, f); // citim numarul de fisiere de intrare

    int files_in = atoi(buffer);

    struct file_struct *file_in;                                                   // structura cu fisierele de intrare pe care le putem folosi
    file_in = (struct file_struct *)malloc(files_in * sizeof(struct file_struct)); // alocare memorie

    for (int i = 0; i < files_in; i++) // citim fisierele de intrare
    {
        file_in[i].rank = rank;
        fgets(buffer, HASH_SIZE + 1, f);
        char *token = strtok(buffer, " ");
        strcpy(file_in[i].filename, token);
        token = strtok(NULL, " ");
        file_in[i].chunks = atoi(token);
        file_in[i].hashes = (char **)malloc(file_in[i].chunks * sizeof(char *));
        for (int j = 0; j < file_in[i].chunks; j++)
        {
            file_in[i].hashes[j] = (char *)malloc(HASH_SIZE * sizeof(char));
            fgets(buffer, HASH_SIZE + 2, f);
            buffer[strlen(buffer) - 1] = '\0'; // stergem \n de la sfarsitul lui buffer
            strcpy(file_in[i].hashes[j], buffer);
        }
    }

    fgets(buffer, HASH_SIZE + 1, f); // citim numarul de fisiere pe care trebuie sa le scriem

    int files_out = atoi(buffer);

    struct file_out file_out_struct;
    file_out_struct.file_name = (char **)malloc(files_out * sizeof(char *));
    file_out_struct.files_out = files_out;
    file_out_struct.rank = rank;

    for (int i = 0; i < files_out; i++) // citim numele fisierelor pe care trebuie sa le scriem
    {
        fgets(buffer, HASH_SIZE + 1, f);
        file_out_struct.file_name[i] = (char *)malloc(HASH_SIZE * sizeof(char));
        if (buffer[strlen(buffer) - 1] == '\n')
            buffer[strlen(buffer) - 1] = '\0';
        strcpy(file_out_struct.file_name[i], buffer);
    }

    fclose(f);

    MPI_Send(&files_in, 1, MPI_INT, TRACKER_RANK, rank, MPI_COMM_WORLD); // trimitem numarul de fisiere de intrare

    MPI_Send(&files_in, 1, MPI_INT, TRACKER_RANK, rank, MPI_COMM_WORLD);

    for (int i = 0; i < files_in; i++) // trimitem fisierele de intrare
    {
        MPI_Send(file_in[i].filename, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, rank, MPI_COMM_WORLD);
        MPI_Send(&file_in[i].chunks, 1, MPI_INT, TRACKER_RANK, rank, MPI_COMM_WORLD);
        for (int j = 0; j < file_in[i].chunks; j++)
        {
            MPI_Send(file_in[i].hashes[j], HASH_SIZE, MPI_CHAR, TRACKER_RANK, rank, MPI_COMM_WORLD);
        }
    }

    int ok = 0;

    MPI_Recv(&ok, 1, MPI_INT, TRACKER_RANK, rank, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // primeste ACK de la tracker

    if (ok == 0)
    {
        printf("EROARE\n");
        exit(-1);
    }

    struct upload_struct upload_struct;
    upload_struct.rank = rank;
    upload_struct.numtasks = numtasks;

    r = pthread_create(&download_thread, NULL, download_thread_func, (void *)&file_out_struct);
    if (r)
    {
        printf("Eroare la crearea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *)&upload_struct);
    if (r)
    {
        printf("Eroare la crearea thread-ului de upload\n");
        exit(-1);
    }

    r = pthread_join(download_thread, (void *)&file_out_struct);
    if (r)
    {
        printf("Eroare la asteptarea thread-ului de download\n");
        exit(-1);
    }

    r = pthread_join(upload_thread, (void *)&upload_struct);
    if (r)
    {
        printf("Eroare la asteptarea thread-ului de upload\n");
        exit(-1);
    }

}

int main(int argc, char *argv[])
{
    int numtasks, rank;

    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE)
    {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK)
    {
        tracker(numtasks, rank);
    }
    else
    {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
