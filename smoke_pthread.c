#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#define NUM_ITERATIONS 1000

#ifdef VERBOSE
#define VERBOSE_PRINT(S, ...) printf (S, ##__VA_ARGS__);
#else
#define VERBOSE_PRINT(S, ...) ;
#endif

//NOTE: I didn't know there was another mechanism for managing threads called a monitor, so I accidentally named my other threads monitors

typedef pthread_cond_t* convar_pointer;//these typedefs make porting my uthread code over easier
typedef pthread_mutex_t* mutex_pointer;

struct Agent {
  mutex_pointer mutex;
  convar_pointer  match;
  convar_pointer  paper;
  convar_pointer  tobacco;
  convar_pointer  smoke;
};

struct Smoker {
  int smoker_type;//the number indicating what resource this smoker has an infinite supply of (i.e. MATCH, PAPER, or TOBACCO)
  mutex_pointer mutex;// this is assigned to a smoker upon initialization, all smokers have the same mutex
  mutex_pointer agent_mutex;//when initializing, smokers must all be given the agent's mutex to access the smoke convar
  convar_pointer  resource_combo; // one of matchANDpaper, matchANDtobacco, or paperANDtobacco
  convar_pointer  smoke;	//when initializing, smokers must all be given the agent's smoke condition variable
};

struct Monitor{
	int resource_number; //the number indicating what resource this monitor is monitoring
	mutex_pointer smoker_mutex;
	mutex_pointer agent_mutex;
	convar_pointer  resource;//the resource that this monitor monitors
	convar_pointer  matchANDpaper; //each monitor is assigned each smoker's convar
	convar_pointer  matchANDtobacco;
	convar_pointer  paperANDtobacco;
};

struct Agent* createAgent() {
  struct Agent* agent = malloc (sizeof (struct Agent));
  agent->match = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
  agent->paper = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
  agent->tobacco = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
  agent->smoke = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
  agent->mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
  pthread_cond_init(agent->match, NULL);
  pthread_cond_init(agent->paper, NULL);
  pthread_cond_init(agent->tobacco, NULL);
  pthread_cond_init(agent->smoke, NULL);
  pthread_mutex_init(agent->mutex, NULL);
  VERBOSE_PRINT("agent mutex in agent %p\n", agent->mutex);
  return agent;
}

void destroyAgent(struct Agent* agent) {
  pthread_mutex_destroy(agent->mutex);
  free(agent->mutex);
  pthread_cond_destroy(agent->paper);
  free(agent->paper);
  pthread_cond_destroy(agent->match);
  free(agent->match);
  pthread_cond_destroy(agent->tobacco);
  free(agent->tobacco);
  pthread_cond_destroy(agent->smoke);
  free(agent->smoke);
  free(agent);
}

struct Smoker* createSmoker(int smoker_type_number, convar_pointer agent_smoke,
										mutex_pointer smoker_mutex, mutex_pointer agent_mutex) {
  struct Smoker* smoker 	= malloc (sizeof (struct Smoker));
  smoker->smoker_type		= smoker_type_number;
  smoker->mutex 			= smoker_mutex;
  smoker->agent_mutex		= agent_mutex;
  smoker->resource_combo	= (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
  pthread_cond_init(smoker->resource_combo, NULL);
  smoker->smoke 			= agent_smoke;
  return smoker;
}

void destroySmoker(struct Smoker* smoker) {
  pthread_cond_destroy(smoker->resource_combo);
  free(smoker->resource_combo);
  free(smoker);
}

struct Monitor* createMonitor(int assigned_resource_number, convar_pointer assigned_resource,
										mutex_pointer smoker_mutex, mutex_pointer agent_mutex,
											convar_pointer paperANDtobacco, convar_pointer matchANDtobacco, convar_pointer matchANDpaper) {
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
int ready = 0; //used to ensure monitors and smokers begin waiting for their convars before the agent thread is created

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
  
  pthread_mutex_lock(a->mutex);
    for (int i = 0; i < NUM_ITERATIONS; i++) {
      int r = random() % 3;
      signal_count [matching_smoker [r]] ++;
      int c = choices [r];
      if (c & MATCH) {
        VERBOSE_PRINT ("match available\n");
        pthread_cond_signal(a->match);
      }
      if (c & PAPER) {
        VERBOSE_PRINT ("paper available\n");
        pthread_cond_signal (a->paper);
      }
      if (c & TOBACCO) {
        VERBOSE_PRINT ("tobacco available\n");
        pthread_cond_signal (a->tobacco);
      }
      VERBOSE_PRINT ("agent is waiting for smoker to smoke\n");
      pthread_cond_wait (a->smoke, a->mutex);
    }
	
  pthread_cond_signal (a->match);// these will make sure the last monitor stops waiting on a signal
  pthread_cond_signal (a->paper);// these additional lines do not go against the imposed restrictions
  pthread_cond_signal (a->tobacco);
  
  pthread_mutex_unlock (a->mutex);
  VERBOSE_PRINT("agent is done\n");
  return NULL;
}


void* monitor(void* arg){
	struct Monitor* m = arg;
	VERBOSE_PRINT("agent mutex in monitor %d func using m->agent_mutex %p\n", m->resource_number, m->agent_mutex);
	pthread_mutex_lock(m->agent_mutex);
	VERBOSE_PRINT("update ready %d\n", m->resource_number);
	ready++;
	while(1){
		if(signal_count [MATCH] + signal_count [PAPER] + signal_count [TOBACCO] == NUM_ITERATIONS){
			if(smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS){
				pthread_mutex_lock(m->smoker_mutex);//these will make sure the last smokers stop waiting on a signal
				pthread_cond_signal(m->matchANDpaper);
				pthread_cond_signal(m->matchANDtobacco);
				pthread_cond_signal(m->paperANDtobacco);
				pthread_mutex_unlock(m->smoker_mutex);
			}
			break;
		}
		VERBOSE_PRINT("Monitor waiting for resource signal\n");
		pthread_cond_wait(m->resource, m->agent_mutex);
		VERBOSE_PRINT("Monitor grabbed the agent lock again\n");
		
		available = (available)|(m->resource_number); //only access available when we have the agent's mutex
		VERBOSE_PRINT("available value = %d \n", available);
		if(available == (MATCH|PAPER)){
			available = 0;
			pthread_mutex_lock(m->smoker_mutex);
			VERBOSE_PRINT("signalling match and paper\n");
			pthread_cond_signal(m->matchANDpaper);
			pthread_mutex_unlock(m->smoker_mutex);
			
		}else if(available == (MATCH|TOBACCO)){
			available = 0;
			pthread_mutex_lock(m->smoker_mutex);
			VERBOSE_PRINT("signalling match and tobacco\n");
			pthread_cond_signal(m->matchANDtobacco);
			pthread_mutex_unlock(m->smoker_mutex);
			
		}else if(available == (PAPER|TOBACCO)){
			available = 0;
			pthread_mutex_lock(m->smoker_mutex);
			VERBOSE_PRINT("signalling paper and tobacco\n");
			pthread_cond_signal(m->paperANDtobacco);
			pthread_mutex_unlock(m->smoker_mutex);
		}
	}
	pthread_mutex_unlock(m->agent_mutex);
	VERBOSE_PRINT("monitor is done\n");
	return NULL;
}

void* smoker(void* arg){
	struct Smoker* s = arg;
	VERBOSE_PRINT("smoker waiting to get smoker lock\n");
	pthread_mutex_lock(s->mutex);
	ready ++;
	while(1){
		VERBOSE_PRINT ("Smoker waiting for signal\n");
		pthread_cond_wait(s->resource_combo, s->mutex);
		if(smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS){
			break;
		}
		VERBOSE_PRINT ("Smoke!\n");
		smoke_count[s->smoker_type]++;

		pthread_mutex_lock(s->agent_mutex);
		VERBOSE_PRINT ("Smoke signal\n");
		pthread_cond_signal(s->smoke);
		pthread_mutex_unlock(s->agent_mutex);
		if(smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS){
			break;
		}
	}
	pthread_mutex_unlock(s->mutex);
	VERBOSE_PRINT("smoker is done\n");
	return NULL;
}


int main (int argc, char** argv) { 
 
  struct Agent*  a = createAgent();
  
  mutex_pointer smoker_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));// make a common mutex for smokers
  pthread_mutex_init(smoker_mutex, NULL);
  
  struct Smoker* match_smoker = createSmoker(MATCH, a->smoke, smoker_mutex, a->mutex);
  pthread_t match_smoker_thread;
  pthread_create(&match_smoker_thread, NULL, smoker, match_smoker);
  
  struct Smoker* paper_smoker = createSmoker(PAPER, a->smoke, smoker_mutex, a->mutex);
  pthread_t paper_smoker_thread;
  pthread_create(&paper_smoker_thread, NULL, smoker, paper_smoker);
  
  struct Smoker* tobacco_smoker = createSmoker(TOBACCO, a->smoke, smoker_mutex, a->mutex);
  pthread_t tobacco_smoker_thread;
  pthread_create(&tobacco_smoker_thread, NULL, smoker, tobacco_smoker);
  
  while(ready != 3){
      //this likely won't even spin, it's just to be 100% sure that the smokers are waiting on their convar before their signal comes for the first time
  }
  ready = 0;//don't need to lock mutex for this assignment because the smokers have already updated it and cannot update it again
  
  struct Monitor* match_monitor = createMonitor(MATCH, a->match, smoker_mutex, a->mutex, match_smoker->resource_combo,
																	paper_smoker->resource_combo, tobacco_smoker->resource_combo);
  pthread_t match_monitor_thread;
  pthread_create(&match_monitor_thread, NULL, monitor, match_monitor);
  
  struct Monitor* paper_monitor = createMonitor(PAPER, a->paper, smoker_mutex, a->mutex, match_smoker->resource_combo,
																	paper_smoker->resource_combo, tobacco_smoker->resource_combo);
  pthread_t paper_monitor_thread;
  pthread_create(&paper_monitor_thread, NULL, monitor, paper_monitor);
  
  struct Monitor* tobacco_monitor = createMonitor(TOBACCO, a->tobacco, smoker_mutex, a->mutex, match_smoker->resource_combo, 
																	paper_smoker->resource_combo, tobacco_smoker->resource_combo);  
  pthread_t tobacco_monitor_thread;
  pthread_create(&tobacco_monitor_thread, NULL, monitor, tobacco_monitor);
  
  while(ready != 3){
	 //this likely won't even spin, it's just to be 100% sure that the monitors are waiting on their convar before their signal comes for the first time
  }
  ready = 0;
  
  VERBOSE_PRINT("Agent thread starting\n");
  pthread_t agent_thread;
  pthread_create(&agent_thread, NULL, agent, a);
  pthread_join(agent_thread, NULL);
  pthread_join(match_smoker_thread, NULL);
  pthread_join(paper_smoker_thread, NULL);
  pthread_join(tobacco_smoker_thread, NULL);
  pthread_join(match_monitor_thread, NULL);
  pthread_join(paper_monitor_thread, NULL);
  pthread_join(tobacco_monitor_thread, NULL);
  
  //remember to destroy them!!!
  destroyMonitor(match_monitor);
  destroyMonitor(paper_monitor);
  destroyMonitor(tobacco_monitor);
  
  destroySmoker(match_smoker);
  destroySmoker(paper_smoker);
  destroySmoker(tobacco_smoker);
  
  pthread_mutex_destroy(smoker_mutex);
  free(smoker_mutex);
  destroyAgent(a);
  
  assert (signal_count [MATCH]   == smoke_count [MATCH]);
  assert (signal_count [PAPER]   == smoke_count [PAPER]);
  assert (signal_count [TOBACCO] == smoke_count [TOBACCO]);
  assert (smoke_count [MATCH] + smoke_count [PAPER] + smoke_count [TOBACCO] == NUM_ITERATIONS);
  printf ("Smoke counts: %d matches, %d paper, %d tobacco\n",
          smoke_count [MATCH], smoke_count [PAPER], smoke_count [TOBACCO]);
}