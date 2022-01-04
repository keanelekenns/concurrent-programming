#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"
#include "spinlock.h"

#define MAX_ITEMS 10
const int NUM_ITERATIONS = 200;
const int NUM_CONSUMERS  = 2;
const int NUM_PRODUCERS  = 2;

int producer_wait_count = 0;     // # of times producer had to wait
int consumer_wait_count = 0;     // # of times consumer had to wait
int histogram [MAX_ITEMS+1]; // histogram [i] == # of times list stored i items

uthread_mutex_t mutex;
uthread_mutex_t wait_mutex;
uthread_cond_t	item_available;
uthread_cond_t	space_available;

int items = 0;

void* producer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
	  uthread_mutex_lock(mutex);
	  while(items == MAX_ITEMS){
		  uthread_mutex_lock(wait_mutex);
		  producer_wait_count++;
		  uthread_mutex_unlock(wait_mutex);
		  uthread_cond_wait(space_available);
	  }
	  items++;
	  histogram[items]++;
	  uthread_cond_signal(item_available);
	  uthread_mutex_unlock(mutex);
	  assert (!(items < 0) && !(items > MAX_ITEMS));
  }
  return NULL;
}

void* consumer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
	  uthread_mutex_lock(mutex);
	  while(items == 0){
		  uthread_mutex_lock(wait_mutex);
		  consumer_wait_count++;
		  uthread_mutex_unlock(wait_mutex);
		  uthread_cond_wait(item_available);
	  }
	  items--;
	  histogram[items]++;
	  uthread_cond_signal(space_available);
	  uthread_mutex_unlock(mutex);
	  assert (!(items < 0) && !(items > MAX_ITEMS));
  }
  return NULL;
}

int main (int argc, char** argv) {
  
  wait_mutex = uthread_mutex_create();
  mutex = uthread_mutex_create();
  
  item_available = uthread_cond_create(mutex);
  space_available = uthread_cond_create(mutex);
  
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
  printf ("producer_wait_count=%d\nconsumer_wait_count=%d\n", producer_wait_count, consumer_wait_count);
  printf ("items value histogram:\n");
  int sum=0;
  for (int i = 0; i <= MAX_ITEMS; i++) {
    printf ("  items=%d, %d times\n", i, histogram [i]);
    sum += histogram [i];
  }
  
  uthread_cond_destroy(item_available);
  uthread_cond_destroy(space_available);
  uthread_mutex_destroy(mutex);
  uthread_mutex_destroy(wait_mutex);
  
  assert (sum == sizeof (t) / sizeof (uthread_t) * NUM_ITERATIONS);
}
