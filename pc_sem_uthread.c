#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "uthread.h"
#include "uthread_sem.h"

#define MAX_ITEMS 10
const int NUM_ITERATIONS = 200;
const int NUM_CONSUMERS  = 2;
const int NUM_PRODUCERS  = 2;

int histogram [MAX_ITEMS+1]; // histogram [i] == # of times list stored i items

uthread_sem_t mutex;
uthread_sem_t full;
uthread_sem_t empty;

int items = 0;

void* producer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
	  uthread_sem_wait(empty);
	  uthread_sem_wait(mutex);
	  items++;
	  histogram[items]++;
	  uthread_sem_signal(mutex);
	  uthread_sem_signal(full);
	  
	  assert (!(items < 0) && !(items > MAX_ITEMS));
  }
  return NULL;
}

void* consumer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
	  uthread_sem_wait(full);
	  uthread_sem_wait(mutex);
	  items--;
	  histogram[items]++;
	  uthread_sem_signal(mutex);
	  uthread_sem_signal(empty);
	  
	  assert (!(items < 0) && !(items > MAX_ITEMS));
  }
  return NULL;
}

int main (int argc, char** argv) {
  
  mutex = uthread_sem_create(1);
  full = uthread_sem_create(0);
  empty = uthread_sem_create(MAX_ITEMS);
  
  uthread_t t[NUM_CONSUMERS + NUM_PRODUCERS];

  uthread_init (4);
  
  // TODO: Create Threads and Join
  for(int i = 0; i < NUM_PRODUCERS; i++){
	t[i] = uthread_create(producer, NULL);
  }
  for(int i = NUM_PRODUCERS; i < NUM_CONSUMERS + NUM_PRODUCERS; i++){
	t[i] = uthread_create(consumer, NULL);
  }
  for(int i = 0; i < NUM_CONSUMERS + NUM_PRODUCERS; i++){
	if( uthread_join(t[i], NULL) == -1){
		fprintf(stderr, "Couldn't join thread %d\n", i+1);
	}
  }
  
  printf ("items value histogram:\n");
  int sum=0;
  for (int i = 0; i <= MAX_ITEMS; i++) {
    printf ("  items=%d, %d times\n", i, histogram [i]);
    sum += histogram [i];
  }
  
  uthread_sem_destroy(empty);
  uthread_sem_destroy(full);
  uthread_sem_destroy(mutex);
  
  assert (sum == sizeof (t) / sizeof (uthread_t) * NUM_ITERATIONS);
}
