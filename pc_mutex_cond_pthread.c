#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

#define MAX_ITEMS 10
const int NUM_ITERATIONS = 200;
const int NUM_CONSUMERS  = 2;
const int NUM_PRODUCERS  = 2;

int producer_wait_count = 0;     // # of times producer had to wait 
int consumer_wait_count = 0;     // # of times consumer had to wait 
int histogram [MAX_ITEMS+1]; // histogram [i] == # of times list stored i items

pthread_mutex_t mutex;
pthread_mutex_t wait_mutex;
pthread_cond_t	item_available;
pthread_cond_t	space_available;

int items = 0;

void* producer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
	  pthread_mutex_lock(&mutex);
	  while(items == MAX_ITEMS){
		  pthread_mutex_lock(&wait_mutex);
		  producer_wait_count++;
		  pthread_mutex_unlock(&wait_mutex);
		  pthread_cond_wait(&space_available, &mutex);
	  }
	  items++;
	  histogram[items]++;
	  pthread_cond_signal(&item_available);
	  pthread_mutex_unlock(&mutex);
	  assert (!(items < 0) && !(items > MAX_ITEMS));
  }
  return NULL;
}

void* consumer (void* v) {
  for (int i=0; i<NUM_ITERATIONS; i++) {
	  pthread_mutex_lock(&mutex);
	  while(items == 0){
		  pthread_mutex_lock(&wait_mutex);
		  consumer_wait_count++;
		  pthread_mutex_unlock(&wait_mutex);
		  pthread_cond_wait(&item_available, &mutex);
	  }
	  items--;
	  histogram[items]++;
	  pthread_cond_signal(&space_available);
	  pthread_mutex_unlock(&mutex);
	  assert (!(items < 0) && !(items > MAX_ITEMS));
  }
  return NULL;
}

int main (int argc, char** argv) {
  
  pthread_mutex_init(&mutex, NULL);
  pthread_mutex_init(&wait_mutex, NULL);

  pthread_cond_init(&item_available, NULL);
  pthread_cond_init(&space_available, NULL);
  
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
  printf ("producer_wait_count=%d\nconsumer_wait_count=%d\n", producer_wait_count, consumer_wait_count);
  printf ("items value histogram:\n");
  int sum=0;
  for (int i = 0; i <= MAX_ITEMS; i++) {
    printf ("  items=%d, %d times\n", i, histogram [i]);
    sum += histogram [i];
  }
  
  pthread_cond_destroy(&space_available);
  pthread_cond_destroy(&item_available);
  
  pthread_mutex_destroy(&wait_mutex);
  pthread_mutex_destroy(&mutex);
  
  assert (sum == sizeof (t) / sizeof (pthread_t) * NUM_ITERATIONS);
}