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

int producer_wait_count = 0;     // # of times producer had to wait (only counts waiting for items to be correct value, not waiting for spinlock)
int consumer_wait_count = 0;     // # of times consumer had to wait (only counts waiting for items to be correct value, not waiting for spinlock)
int histogram [MAX_ITEMS+1]; // histogram [i] == # of times list stored i items

spinlock_t lock;
spinlock_t wait_lock; //use this lock to update the wait counts without interfering with the item updates

int items = 0;

void* producer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
	  while(1){
		  while(items >= MAX_ITEMS){
			  spinlock_lock(&wait_lock);
			  producer_wait_count++;
			  spinlock_unlock(&wait_lock);
		  }
		  spinlock_lock(&lock);
		  if(items >= MAX_ITEMS){
			  spinlock_lock(&wait_lock);
			  producer_wait_count++;
			  spinlock_unlock(&wait_lock);
			  spinlock_unlock(&lock);
		  }else{
			  items++;
			  histogram[items]++;
			  spinlock_unlock(&lock);
			  break;
		  }
	  }
	  assert (!(items < 0) && !(items > MAX_ITEMS));
  }
  return NULL;
}

void* consumer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
	  while(1){
		  while(items <= 0){
			  spinlock_lock(&wait_lock);
			  consumer_wait_count++;
			  spinlock_unlock(&wait_lock);
		  }
		  spinlock_lock(&lock);
		  if(items <= 0){
			  spinlock_lock(&wait_lock);
			  consumer_wait_count++;
			  spinlock_unlock(&wait_lock);
			  spinlock_unlock(&lock);
		  }else{
			  items--;
			  histogram[items]++;
			  spinlock_unlock(&lock);
			  break;
		  }
	  }
	  assert (!(items < 0) && !(items > MAX_ITEMS));
  }
  return NULL;
}

int main (int argc, char** argv) {
  
  spinlock_create(&lock);
  spinlock_create(&wait_lock);
  
  uthread_t t[4];

  uthread_init (4);
  
  // TODO: Create Threads and Join
  for(int i = 0; i < 2; i++){
	t[i] = uthread_create(producer, NULL);
  }
  for(int i = 2; i < 4; i++){
	t[i] = uthread_create(consumer, NULL);
  }
  for(int i = 0; i < 4; i++){
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
  assert (sum == sizeof (t) / sizeof (uthread_t) * NUM_ITERATIONS);
}
