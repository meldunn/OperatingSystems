#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define NUMREADERS 500
#define NUMWRITERS 10
static int glob = 0;
sem_t rw_mutex;
sem_t mutex;
int read_count = 0; //num of readers accessing the data

int read_repeat_count;
int writer_repeat_count;

double read_time_max = -1;
double read_time_min = 10000000;
double read_time_sum = 0;

double write_time_max = -1;
double write_time_min = 10000000;
double write_time_sum = 0;

void *reader(void *arg)
{

  int *id = (int *)arg;
  int currcount = 0;
  while (currcount < read_repeat_count)
  {
    currcount++;                       //increment amount of tries
    int sleep_time = rand() % 100 + 1; //generate random number between 1 and 100

    clock_t begin = clock();

    sem_wait(&mutex); //lock reading semaphore to update reader count
    read_count = read_count + 1;
    if (read_count == 1)
    {
      sem_wait(&rw_mutex); //lock writing semaphore
    }
    sem_post(&mutex); //unlock reading semaphore

    clock_t end = clock();
    double time = (double)(end - begin) / CLOCKS_PER_SEC;

    //check if need to update max/min wait
    if (time > read_time_max)
    {
      read_time_max = time;
    }
    if (time < read_time_min)
    {
      read_time_min = time;
    }

    //increase sum of total time
    read_time_sum = read_time_sum + time;

    //printf("global var is %d from read %d\n", glob, (int) *id);

    sem_wait(&mutex);            //lock reading semaphore
    read_count = read_count - 1; //update reader count
    if (read_count == 0)
    {
      sem_post(&rw_mutex); //release writing semaphore
    }
    sem_post(&mutex);   //release reading semaphore
    usleep(sleep_time); //sleep for sleep_time amount of milliseconds
  }
  //return NULL;
}

void *writer(void *arg)
{
  int *id = (int *)arg;
  int curr_count = 0;
  while (curr_count < writer_repeat_count)
  {
    curr_count++;
    int sleep_time = rand() % 100 + 1; //generate random number between 1 and 100

    clock_t begin = clock();
    sem_wait(&rw_mutex); //lock writing semaphore
    clock_t end = clock();

    double time = (double)(end - begin) / CLOCKS_PER_SEC;

    //check if need to update max/min wait
    if (time > write_time_max)
    {
      write_time_max = time;
    }
    if (time < write_time_min)
    {
      write_time_min = time;
    }
    //increase sum of total time
    write_time_sum = write_time_sum + time;

    //critical

    glob = glob + 10;
    //printf("Data writen by the writer%d is %d\n", (int)*id, glob);

    sem_post(&rw_mutex); //unlock writing semaphore
    usleep(sleep_time);  //sleep for sleep_time amount of milliseconds
  }
  return NULL;
}

int main(int argc, char *argv[])
{
  pthread_t readers[NUMREADERS], writers[NUMWRITERS];
  int s, i, j, r[NUMREADERS], w[NUMWRITERS];

  writer_repeat_count = atoi(argv[1]);
  read_repeat_count = atoi(argv[2]);

  if ((sem_init(&rw_mutex, 0, 1) == -1) || (sem_init(&mutex, 0, 1) == -1))
  {
    printf("Error, init semaphore\n");
    exit(1);
  }

  for (i = 0; i < NUMREADERS; i++)
  {
    r[i] = i;
    s = pthread_create(&readers[i], NULL, reader, (void *)&r[i]);
    if (s != 0)
    {
      printf("Error, creating threads\n");
      exit(1);
    }
  }

  for (j = 0; j < NUMWRITERS; j++)
  {
    w[j] = j;
    s = pthread_create(&writers[j], NULL, writer, (void *)&w[j]);
    if (s != 0)
    {
      printf("Error, creating threads\n");
      exit(1);
    }
  }

  for (j = 0; j < NUMWRITERS; j++)
  {
    s = pthread_join(writers[j], NULL);
    if (s != 0)
    {
      printf("Error, creating threads\n");
      exit(1);
    }
  }
  for (i = 0; i < NUMREADERS; i++)
  {
    s = pthread_join(readers[i], NULL);
    if (s != 0)
    {
      printf("Error, creating threads\n");
      exit(1);
    }
  }

  sem_destroy(&rw_mutex);
  sem_destroy(&mutex);

  double write_avg = write_time_sum / (NUMWRITERS * writer_repeat_count);
  double read_avg = read_time_sum / (NUMREADERS * read_repeat_count);
  printf("\nRead times\n");
  printf("max: %f \t min: %f \t avg: %f\n", read_time_max, read_time_min, read_avg);
  printf("\nWrite times\n");
  printf("max: %f \t min: %f \t avg: %f\n", write_time_max, write_time_min, write_avg);

  return 0;
}
