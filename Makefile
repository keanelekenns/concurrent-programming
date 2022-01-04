UTHREAD = ./lib
TARGETS = smoke_uthread smoke_pthread pc_sem_uthread pc_sem_pthread pc_spinlock_uthread pc_mutex_cond_uthread pc_mutex_cond_pthread

OBJS = $(UTHREAD)/uthread.o $(UTHREAD)/uthread_mutex_cond.o $(UTHREAD)/uthread_sem.o
JUNKF = $(OBJS) *~
JUNKD = *.dSYM
ifdef VERBOSE
	CFLAGS  += -D VERBOSE -g -std=gnu11 -I$(UTHREAD)
else
	CFLAGS  += -g -std=gnu11 -I$(UTHREAD)
endif
UNAME = $(shell uname)
ifeq ($(UNAME), Linux)
LDFLAGS += -pthread
endif
all: $(TARGETS)
$(TARGETS): $(OBJS)
tidy:
	rm -f $(JUNKF); rm -rf $(JUNKD)
clean:
	rm -f $(JUNKF) $(TARGETS); rm -rf $(JUNKD)


