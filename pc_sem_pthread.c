#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h> 

#define MAX_ITEMS 10
const int NUM_ITERATIONS = 200;
const int NUM_CONSUMERS  = 2;
const int NUM_PRODUCERS  = 2;

int histogram [MAX_ITEMS+1]; // histogram [i] == # of times list stored i items

sem_t mutex;
sem_t full;
sem_t empty;

int items = 0;

void* producer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
	  sem_wait(&empty);
	  sem_wait(&mutex);
	  items++;
	  histogram[items]++;
	  sem_post(&mutex);
	  sem_post(&full);
	  
	  assert (!(items < 0) && !(items > MAX_ITEMS));
  }
  return NULL;
}

void* consumer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
	  sem_wait(&full);
	  sem_wait(&mutex);
	  items--;
	  histogram[items]++;
	  sem_post(&mutex);
	  sem_post(&empty);

	  assert (!(items < 0) && !(items > MAX_ITEMS));
  }
  return NULL;
}

int main (int argc, char** argv) {
  
  sem_init(&mutex, 0, 1);
  sem_init(&full, 0, 0);
  sem_init(&empty, 0, MAX_ITEMS);
  
  pthread_t t[NUM_CONSUMERS + NUM_PRODUCERS];
  
  for(int i = 0; i < NUM_PRODUCERS; i++){
	  if(pthread_create(&t[i], NULL, producer, NULL) != 0){
		  perror("Couldn't create thread");
	  }
  }
  for(int i = NUM_PRODUCERS; i < NUM_CONSUMERS + NUM_PRODUCERS; i++){
	  if(pthread_create(&t[i], NULL, consumer, NULL) != 0){
		  perror("Couldn't create thread");
	  }
  }
  for(int i = 0; i < NUM_CONSUMERS + NUM_PRODUCERS; i++){
	if(pthread_join(t[i], NULL) != 0){
		perror("Couldn't join thread");
	}
  }
  
  printf ("items value histogram:\n");
  int sum=0;
  for (int i = 0; i <= MAX_ITEMS; i++) {
    printf ("  items=%d, %d times\n", i, histogram [i]);
    sum += histogram [i];
  }
  
  sem_destroy(&empty);
  sem_destroy(&full);
  sem_destroy(&mutex);
  
  assert (sum == sizeof (t) / sizeof (pthread_t) * NUM_ITERATIONS);
}