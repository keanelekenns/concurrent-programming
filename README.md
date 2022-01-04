# concurrent-programming
This repository contains solutions to basic [producer-consumer problems](https://en.wikipedia.org/wiki/Producer%E2%80%93consumer_problem) as well as the [smoker's problem](https://en.wikipedia.org/wiki/Cigarette_smokers_problem). The solutions are implemented with [POSIX threads](https://en.wikipedia.org/wiki/Pthreads) as well as uthreads, which are user level threads implemented by [Mike Feeley](https://www.cs.ubc.ca/~feeley/) at the University of British Columbia.

In the top level directory, run ```make``` or ```make VERBOSE=true``` to compile the programs and ```make clean``` to remove the object and binary files.

To run a program, simply use ```./program_name``` (e.g. ```./smoke_pthread.c```).

Note that the ```VERBOSE``` option enables additional output and may need to be written to a file.