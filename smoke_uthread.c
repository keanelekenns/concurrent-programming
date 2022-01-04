#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uthread.h"
#include "uthread_mutex_cond.h"

#define NUM_ITERATIONS 1000

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

struct Agent {
  uthread_mutex_t mutex;
  uthread_cond_t  match;
  uthread_cond_t  paper;
  uthread_cond_t  tobacco;
  uthread_cond_t  smoke;
};

struct Smoker {
  int smoker_type;//the number indicating what resource this smoker has an infinite supply of (i.e. MATCH, PAPER, or TOBACCO)
  uthread_mutex_t mutex;// this is assigned to a smoker upon initialization, all smokers have the same mutex
  uthread_mutex_t agent_mutex;//when initializing, smokers must all be given the agent's mutex to access the smoke convar
  uthread_cond_t  resource_combo; // a non-specific version of matchANDpaper, matchANDtobacco, or paperANDtobacco
  uthread_cond_t  smoke;	//when initializing, smokers must all be given the agent's smoke condition variable
};

struct Monitor{
	int resource_number; //the number indicating what resource this monitor is monitoring
	uthread_mutex_t smoker_mutex;
	uthread_mutex_t agent_mutex;
	uthread_cond_t  resource;
	uthread_cond_t  matchANDpaper;
	uthread_cond_t  matchANDtobacco;
	uthread_cond_t  paperANDtobacco;
};

struct Agent* createAgent() {
  struct Agent* agent = malloc (sizeof (struct Agent));
  agent->mutex   = uthread_mutex_create();
  agent->paper   = uthread_cond_create (agent->mutex);
  agent->match   = uthread_cond_create (agent->mutex);
  agent->tobacco = uthread_cond_create (agent->mutex);
  agent->smoke   = uthread_cond_create (agent->mutex);
  return agent;
}

void destroyAgent(struct Agent* agent) {
  uthread_mutex_destroy(agent->mutex);
  uthread_cond_destroy(agent->paper);
  uthread_cond_destroy(agent->match);
  uthread_cond_destroy(agent->tobacco);
  uthread_cond_destroy(agent->smoke);
  free(agent);
}

struct Smoker* createSmoker(int smoker_type_number, uthread_cond_t agent_smoke,
										uthread_mutex_t smoker_mutex, uthread_mutex_t agent_mutex) {
  struct Smoker* smoker 	= malloc (sizeof (struct Smoker));
  smoker->smoker_type		= smoker_type_number;
  smoker->mutex 			= smoker_mutex;
  smoker->agent_mutex		= agent_mutex;
  smoker->resource_combo 	= uthread_cond_create (smoker->mutex);
  smoker->smoke 			= agent_smoke;
  return smoker;
}

void destroySmoker(struct Smoker* smoker) {
  uthread_cond_destroy(smoker->resource_combo);
  free(smoker);
}

struct Monitor* createMonitor(int assigned_resource_number, uthread_cond_t assigned_resource,
										uthread_mutex_t smoker_mutex, uthread_mutex_t agent_mutex,
											uthread_cond_t paperANDtobacco, uthread_cond_t matchANDtobacco, uthread_cond_t matchANDpaper) {
  struct Monitor* monitor 	= malloc (sizeof (struct Monitor));
  monitor->resource_number	= assigned_resource_number;
  monitor->smoker_mutex		= smoker_mutex;
  monitor->agent_mutex		= agent_mutex;
  monitor->resource			= assigned_resource; 
  monitor->matchANDpaper 	= matchANDpaper;
  monitor->matchANDtobacco	= matchANDtobacco;
  monitor->paperANDtobacco	= paperANDtobacco;
  return monitor;
}

void destroyMonitor(struct Monitor* monitor) {
  free(monitor);
}

/**
 * You might find these declarations helpful.
 *   Note that Resource enum had values 1, 2 and 4 so you can combine resources;
 *   e.g., having a MATCH and PAPER is the value MATCH | PAPER == 1 | 2 == 3
 */
enum Resource            {    MATCH = 1, PAPER = 2,   TOBACCO = 4};
char* resource_name [] = {"", "match",   "paper", "", "tobacco"};

int signal_count [5];  // # of times resource signalled
int smoke_count  [5];  // # of times smoker with resource smoked
int available = 0; //this is used by the monitors to determine what is available
int ready = 0; //insures monitors and smokers begin waiting for their convars before the agent thread is created

/**
 * This is the agent procedure.  It is complete and you shouldn't change it in
 * any material way.  You can re-write it if you like, but be sure that all it does
 * is choose 2 random reasources, signal their condition variables, and then wait
 * wait for a smoker to smoke.
 */
void* agent (void* av) {
  struct Agent* a = av;
  static const int choices[]         = {MATCH|PAPER, MATCH|TOBACCO, PAPER|TOBACCO};
  static const int matching_smoker[] = {TOBACCO,     PAPER,         MATCH};
  
  uthread_mutex_lock (a->mutex);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
      int r = random() % 3;
      signal_count [matching_smoker [r]] ++;
      int c = choices [r];
      if (c & MATCH) {
        VERBOSE_PRINT ("match available\n");
        uthread_cond_signal (a->match);
      }
      if (c & PAPER) {
        VERBOSE_PRINT ("paper available\n");
        uthread_cond_signal (a->paper);
      }
      if (c & TOBACCO) {
        VERBOSE_PRINT ("tobacco available\n");
        uthread_cond_signal (a->tobacco);
      }
      VERBOSE_PRINT ("agent is waiting for smoker to smoke\n");
      uthread_cond_wait (a->smoke);
    }
	
  uthread_cond_signal (a->match);// these will make sure the last monitor stops waiting on a signal
  uthread_cond_signal (a->paper);// these additional lines do not go against the imposed restrictions
  uthread_cond_signal (a->tobacco);
  
  uthread_mutex_unlock (a->mutex);
  VERBOSE_PRINT("agent is done\n");
  return NULL;
}


void* monitor(void* arg){
	struct Monitor* m = arg;
	VERBOSE_PRINT("Monitor waiting for agent mutex\n");
	uthread_mutex_lock(m->agent_mutex);
	ready++;
	while(1){
		if(signal_count [MATCH] + signal_count [PAPER] + signal_count [TOBACCO] == NUM_ITERATIONS){
			if(smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS){
				uthread_mutex_lock(m->smoker_mutex);//these will make sure the last smokers stop waiting on a signal
				uthread_cond_signal(m->matchANDpaper);
				uthread_cond_signal(m->matchANDtobacco);
				uthread_cond_signal(m->paperANDtobacco);
				uthread_mutex_unlock(m->smoker_mutex);
			}
			break;
		}
		VERBOSE_PRINT("Monitor waiting for resource signal\n");
		uthread_cond_wait(m->resource);
		VERBOSE_PRINT("Monitor grabbed the agent lock again\n");
		
		available = (available)|(m->resource_number); //only access available when we have the agent's mutex
		VERBOSE_PRINT("available value = %d \n", available);
		if(available == (MATCH|PAPER)){
			available = 0;
			uthread_mutex_lock(m->smoker_mutex);
			VERBOSE_PRINT("signalling match and paper\n");
			uthread_cond_signal(m->matchANDpaper);
			uthread_mutex_unlock(m->smoker_mutex);
			
		}else if(available == (MATCH|TOBACCO)){
			available = 0;
			uthread_mutex_lock(m->smoker_mutex);
			VERBOSE_PRINT("signalling match and tobacco\n");
			uthread_cond_signal(m->matchANDtobacco);
			uthread_mutex_unlock(m->smoker_mutex);
			
		}else if(available == (PAPER|TOBACCO)){
			available = 0;
			uthread_mutex_lock(m->smoker_mutex);
			VERBOSE_PRINT("signalling paper and tobacco\n");
			uthread_cond_signal(m->paperANDtobacco);
			uthread_mutex_unlock(m->smoker_mutex);
		}
	}
	uthread_mutex_unlock(m->agent_mutex);
	VERBOSE_PRINT("monitor is done\n");
	return NULL;
}

void* smoker(void* arg){
	struct Smoker* s = arg;
	uthread_mutex_lock(s->mutex);
	ready ++;
	while(1){
		VERBOSE_PRINT ("Smoker waiting for signal\n");
		uthread_cond_wait(s->resource_combo);
		if(smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS){
			break;
		}
		VERBOSE_PRINT ("Smoke!\n");
		smoke_count[s->smoker_type]++;

		uthread_mutex_lock(s->agent_mutex);
		VERBOSE_PRINT ("Smoke signal\n");
		uthread_cond_signal(s->smoke);
		uthread_mutex_unlock(s->agent_mutex);
		if(smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS){
			break;
		}
	}
	uthread_mutex_unlock(s->mutex);
	VERBOSE_PRINT("smoker is done\n");
	return NULL;
}


int main (int argc, char** argv) {
  uthread_init (7);
  
  struct Agent*  a = createAgent();
  uthread_mutex_t smoker_mutex = uthread_mutex_create();
  
  struct Smoker* match_smoker = createSmoker(MATCH, a->smoke, smoker_mutex, a->mutex);
  uthread_t match_smoker_thread = uthread_create(smoker, match_smoker);
  
  struct Smoker* paper_smoker = createSmoker(PAPER, a->smoke, smoker_mutex, a->mutex);
  uthread_t paper_smoker_thread = uthread_create(smoker, paper_smoker);
  
  struct Smoker* tobacco_smoker = createSmoker(TOBACCO, a->smoke, smoker_mutex, a->mutex);
  uthread_t tobacco_smoker_thread = uthread_create(smoker, tobacco_smoker);
  
  while(ready != 3){
	  //likely won't even spin, just makes 100% sure that the smokers reach their convar before the first signal comes
  }
  ready = 0;//don't need to gain mutex for this assignment because the smokers have already updated it
  
  struct Monitor* match_monitor = createMonitor(MATCH, a->match, smoker_mutex, a->mutex, match_smoker->resource_combo,
																	paper_smoker->resource_combo, tobacco_smoker->resource_combo);
  uthread_t match_monitor_thread = uthread_create(monitor, match_monitor);
  
  struct Monitor* paper_monitor = createMonitor(PAPER, a->paper, smoker_mutex, a->mutex, match_smoker->resource_combo,
																	paper_smoker->resource_combo, tobacco_smoker->resource_combo);
  uthread_t paper_monitor_thread = uthread_create(monitor, paper_monitor);
  
  struct Monitor* tobacco_monitor = createMonitor(TOBACCO, a->tobacco, smoker_mutex, a->mutex, match_smoker->resource_combo, 
																	paper_smoker->resource_combo, tobacco_smoker->resource_combo); 
  uthread_t tobacco_monitor_thread = uthread_create(monitor, tobacco_monitor);
  
  while(ready != 3){
	  //likely won't even spin, just makes 100% sure that the monitors reach their convar before the first signal comes
  }
  ready = 0;
  
  uthread_join (uthread_create (agent, a), 0);
  uthread_join(match_smoker_thread, NULL);
  uthread_join(paper_smoker_thread, NULL);
  uthread_join(tobacco_smoker_thread, NULL);
  uthread_join(match_monitor_thread, NULL);
  uthread_join(paper_monitor_thread, NULL);
  uthread_join(tobacco_monitor_thread, NULL);
  
  //remember to destroy them!!!
  destroyMonitor(match_monitor);
  destroyMonitor(paper_monitor);
  destroyMonitor(tobacco_monitor);
  
  destroySmoker(match_smoker);
  destroySmoker(paper_smoker);
  destroySmoker(tobacco_smoker);
  
  uthread_mutex_destroy(smoker_mutex);
  destroyAgent(a);
  
  assert (signal_count [MATCH]   == smoke_count [MATCH]);
  assert (signal_count [PAPER]   == smoke_count [PAPER]);
  assert (signal_count [TOBACCO] == smoke_count [TOBACCO]);
  assert (smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS);
  printf ("Smoke counts: %d matches, %d paper, %d tobacco\n",
          smoke_count [MATCH], smoke_count [PAPER], smoke_count [TOBACCO]);
}