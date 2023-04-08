#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/shm.h>		// This is necessary for using shared memory constructs
#include <semaphore.h>		// This is necessary for using semaphore
#include <fcntl.h>			// This is necessary for using semaphore
#include <pthread.h>        // This is necessary for Pthread          
#include <string.h>
#include "helpers.h"

#define PARAM_ACCESS_SEMAPHORE "/param_access_semaphore_50"

struct thread_data {
	int thread_id;
	int num;
};

long int global_param = 0;

sem_t *semaphores[8];
pthread_t threads[8];
pthread_mutex_t mutex;

/**
* This function should be implemented by yourself. It must be invoked
* in the child process after the input parameter has been obtained.
* @parms: The input parameter from the terminal.
*/
void multi_threads_run(long int input_param, char* argv);

void *thread_function(void* arg);

int main(int argc, char **argv)
{
	int shmid, status;
	long int local_param = 0;
	long int *shared_param_p, *shared_param_c;

	if (argc < 2) {
		printf("Please enter an eight-digit decimal number as the input parameter.\nUsage: ./main <input_param>\n");
		exit(-1);
	}

   	/*
		Creating semaphores. Mutex semaphore is used to acheive mutual
		exclusion while processes access (and read or modify) the global
		variable, local variable, and the shared memory.
	*/ 

	// Checks if the semaphore exists, if it exists we unlink him from the process.
	sem_unlink(PARAM_ACCESS_SEMAPHORE);
	
	// Create the semaphore. sem_init() also creates a semaphore. Learn the difference on your own.
	sem_t *param_access_semaphore = sem_open(PARAM_ACCESS_SEMAPHORE, O_CREAT|O_EXCL, S_IRUSR|S_IWUSR, 1);

	// Check for error while opening the semaphore
	if (param_access_semaphore != SEM_FAILED){
		printf("Successfully created new semaphore!\n");
	}	
	else if (errno == EEXIST) {   // Semaphore already exists
		printf("Semaphore appears to exist already!\n");
		param_access_semaphore = sem_open(PARAM_ACCESS_SEMAPHORE, 0);
	}
	else {  // An other error occured
		assert(param_access_semaphore != SEM_FAILED);
		exit(-1);
	}

	/*  
	    Creating shared memory. 
        The operating system keeps track of the set of shared memory
	    segments. In order to acquire shared memory, we must first
	    request the shared memory from the OS using the shmget()
      	system call. The second parameter specifies the number of
	    bytes of memory requested. shmget() returns a shared memory
	    identifier (SHMID) which is an integer. Refer to the online
	    man pages for details on the other two parameters of shmget()
	*/
	shmid = shmget(IPC_PRIVATE, sizeof(long int), 0666|IPC_CREAT); // We request an array of one long integer

	/* 
	    After forking, the parent and child must "attach" the shared
	    memory to its local data segment. This is done by the shmat()
	    system call. shmat() takes the SHMID of the shared memory
	    segment as input parameter and returns the address at which
	    the segment has been attached. Thus shmat() returns a char
	    pointer.
	*/

	if (fork() == 0) { // Child Process
        
		printf("Child Process: Child PID is %jd\n", (intmax_t) getpid());
		
		/*  shmat() returns a long int pointer which is typecast here
		    to long int and the address is stored in the long int pointer shared_param_c. */
        shared_param_c = (long int *) shmat(shmid, 0, 0);

		while (1) // Loop to check if the variables have been updated.
		{
			// Get the semaphore
			sem_wait(param_access_semaphore);
			printf("Child Process: Got the variable access semaphore.\n");

			if ( (global_param != 0) || (local_param != 0) || (shared_param_c[0] != 0) )
			{
				printf("Child Process: Read the global variable with value of %ld.\n", global_param);
				printf("Child Process: Read the local variable with value of %ld.\n", local_param);
				printf("Child Process: Read the shared variable with value of %ld.\n", shared_param_c[0]);

                // Release the semaphore
                sem_post(param_access_semaphore);
                printf("Child Process: Released the variable access semaphore.\n");
                
				break;
			}

			// Release the semaphore
			sem_post(param_access_semaphore);
			printf("Child Process: Released the variable access semaphore.\n");
		}

        /**
         * After you have fixed the issue in Problem 1-Q1, 
         * uncomment the following multi_threads_run function 
         * for Problem 1-Q2. Please note that you should also
         * add an input parameter for invoking this function, 
         * which can be obtained from one of the three variables,
         * i.e., global_param, local_param, shared_param_c[0].
         */
		multi_threads_run(shared_param_c[0], argv[2]);

		/* each process should "detach" itself from the 
		   shared memory after it is used */

		shmdt(shared_param_c);

		exit(0);
	}
	else { // Parent Process

		printf("Parent Process: Parent PID is %jd\n", (intmax_t) getpid());

		/*  shmat() returns a long int pointer which is typecast here
		    to long int and the address is stored in the long int pointer shared_param_p.
		    Thus the memory location shared_param_p[0] of the parent
		    is the same as the memory locations shared_param_c[0] of
		    the child, since the memory is shared.
		*/
		shared_param_p = (long int *) shmat(shmid, 0, 0);

		// Get the semaphore first
		sem_wait(param_access_semaphore);
		printf("Parent Process: Got the variable access semaphore.\n");

		global_param = strtol(argv[1], NULL, 10);
		local_param = strtol(argv[1], NULL, 10);
		shared_param_p[0] = strtol(argv[1], NULL, 10);

		// Release the semaphore
		sem_post(param_access_semaphore);
		printf("Parent Process: Released the variable access semaphore.\n");
        
		wait(&status);

		/* each process should "detach" itself from the 
		   shared memory after it is used */

		shmdt(shared_param_p);

		/* Child has exited, so parent process should delete
		   the cretaed shared memory. Unlike attach and detach,
		   which is to be done for each process separately,
		   deleting the shared memory has to be done by only
		   one process after making sure that noone else
		   will be using it 
		 */

		shmctl(shmid, IPC_RMID, 0);

        // Close and delete semaphore. 
        sem_close(param_access_semaphore);
        sem_unlink(PARAM_ACCESS_SEMAPHORE);

		exit(0);
	}

	exit(0);
}

/**
* This function should be implemented by yourself. It must be invoked
* in the child process after the input parameter has been obtained.
* @parms: The input parameter from terminal.
*/
void multi_threads_run(long int input_param, char* argv)
{
	// TODO: Implement this function.
	int j;
	long int result;
	int temp;
	long num;
	num = strtol(argv, NULL, 10);

	// create 8 semaphores
	//sem_t *semaphores[8];
	char semaphore_name[20];

	// extract eight digit from input_param from left to right
	int digit[8];
	int i;

	for (i = 0; i < 8; i++) {
		digit[i] = input_param % 10;
		input_param /= 10;
	}

	// init semaphores
	j = 7;
	for (int i = 0; i < 8; i++) {
		sprintf(semaphore_name, "semaphore_%d", i+1);
		semaphores[i] = sem_open(semaphore_name, O_CREAT, 0644, digit[j]);
		if (semaphores[i] == SEM_FAILED) {
			perror("sem_open");
			exit(1);
		} else {
			printf("Child Process: Created semaphore %s with initial value %d.\n", semaphore_name, digit[j]);
		}
		j--;
	}

	// init mutex
	pthread_mutex_init(&mutex, NULL);

	// create 8 threads
	//pthread_t threads[8];
	for (int i = 0; i < 8; i++) {
		struct thread_data *data = (struct thread_data *)malloc(sizeof(struct thread_data));
		data->thread_id = i+1;
		data->num = num;
		pthread_create(&threads[i], NULL, thread_function, (void *)data);
		printf("Child Process: Created thread %d.\n", data->thread_id);
		pthread_join(threads[i], NULL);
	} 

	// find result
	result = 0;
	for (int i = 0; i < 8; i++) {
		sem_getvalue(semaphores[i], &temp);
		result = result * 10 + temp;
	}
	saveResult("p1_result.txt", result);

	printf("Child Process: Result is %ld.\n", result);

	//destroy semaphores
	for (int i = 0; i < 8; i++) {
		sprintf(semaphore_name, "semaphore_%d", i+1);
		sem_close(semaphores[i]);
		sem_unlink(semaphore_name);
		printf("child process: Destroyed semaphore %s.\n", semaphore_name);
	}

	pthread_mutex_destroy(&mutex);
}

void *thread_function(void* arg) {
	int thread_id = ((struct thread_data *)arg)->thread_id;
	int num = ((struct thread_data *)arg)->num;
	int val1, val2;

	printf("Thread %d: Thread %d is running.\n", 1, thread_id);

	int digit1 = thread_id - 1;
	int digit2 = (thread_id) % 8;

	printf("Thread %d: Thread %d is waiting for semaphore %d.\n", 1, thread_id, digit1);
	printf("Thread %d: Thread %d is waiting for semaphore %d.\n", 1, thread_id, digit2);

	pthread_mutex_lock(&mutex);

	while (num > 0) {
		//printf("%d", num);
		sem_getvalue(semaphores[digit1], &val1);
		sem_getvalue(semaphores[digit2], &val2);
		printf("Thread %d: Thread %d got semaphore %d with value %d.\n", 1, thread_id, digit1, val1);
		printf("Thread %d: Thread %d got semaphore %d with value %d.\n", 1, thread_id, digit2, val2);

		//handle the case when the value of the semaphore is 9
		if (val1 + 1 == 10) {
			for (int i = 0; i < 9; i++) {
				sem_wait(semaphores[digit1]);
			}
			num--;
		} else {
			sem_post(semaphores[digit1]);
		}

		if (val2 + 1 == 10) {
			for (int i = 0; i < 9; i++) {
				sem_wait(semaphores[digit2]);
			}
			num--;
		} else {
			sem_post(semaphores[digit2]);
		}

		sem_getvalue(semaphores[digit1], &val1);
		sem_getvalue(semaphores[digit2], &val2);
		printf("Thread %d: Thread %d got semaphore %d with value %d.\n", 1, thread_id, digit1, val1);
		printf("Thread %d: Thread %d got semaphore %d with value %d.\n", 1, thread_id, digit2, val2);
	}

	pthread_mutex_unlock(&mutex);

	pthread_exit(NULL);
}
