/*
 * vmpager.cpp
 * Ryan Konkul
 * rkonku2
 * CS 385
 * Homework 5: procsync
 */

#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctime>
#include <cmath>

using namespace std;

struct message {
    long type;
    char text[128];
};

bool IsPrime(int num) {
    if(num == 0 || num == 1) return false;
    for(int i=2; i < (num/2)+1; ++i) {
    	if(num % i == 0)
    		return false;
    }
    return true;
}

void delete_shared_memory(int shared_memory_id, int msg_id, int sem_id) {
	shmctl(shared_memory_id, IPC_RMID, NULL);
	msgctl(msg_id, IPC_RMID, NULL);
	semctl(sem_id, IPC_RMID, NULL);
}

void send_msg(int msg_id, int msg_type, string s) {
	message msg;
	msg.type = msg_type;
	strcpy(msg.text, s.c_str());
	if(msgsnd(msg_id, &msg, sizeof(msg.text), 0) < 0) {
		printf( "Error doing msgsnd(): %s\n", strerror(errno));
	}
}

int main(int argc, char ** argv) {
	cout << "Ryan Konkul\nrkonku2\nCS 385\nHomework 5: procsync\n\n";
	int initialtime = time(NULL);
	//number of buffers
	int nBuffer = 5;
	//to use locks or not
	bool lock = false;
	//children to sleep for K usecs
	int K = 10000;

	if(argc < 2) {
		cout << "Command argument nBuffer is required" << endl;
		return 1;
	}
	if(argc > 2) {
		string s(argv[2]);
		if(s == "-lock") {
			lock = true;
		}
	}
	if(argc > 3) {
		K = atoi(argv[3]);
	}
	nBuffer = atoi(argv[1]);
	if(!IsPrime(nBuffer) || nBuffer < 5) {
		cout << "nBuffer must be a prime number >= 5" << endl;
		return 2;
	}
	//get shared memory
	int shared_memory_id;
    if ((shared_memory_id = shmget(IPC_PRIVATE, nBuffer, IPC_CREAT | 0666)) < 0) {
       cout <<"Error doing shmget()" << endl;
       return 3;
    }
	//attach the shared memory to address space
	int* shared_memory;
	if((shared_memory = (int*)shmat(shared_memory_id, NULL, 0)) < 0) {
		cout <<"Error doing shmat()" << endl;
		shmctl(shared_memory_id, IPC_RMID, NULL);
		return 4;
	}
	//initialize shared memory to zero
	for(int i=0; i<5; i++) {
		shared_memory[i] = 0;
	}
	//get msg Q
	int msg_id;
	if ((msg_id = msgget(IPC_PRIVATE, 0400 | 0200)) < 0) {
		cout << "Error doing msgget()" << endl;
		shmctl(shared_memory_id, IPC_RMID, NULL);
		return 5;
	}
	//get semaphores
	int sem_id;
	struct sembuf sem_buf;
	if(lock) {
		//get the semaphores
		if((sem_id = semget(IPC_PRIVATE, nBuffer, 0666 | IPC_CREAT | IPC_EXCL)) < 0) {
			cout << "Error doing semget()" << endl;
			shmctl(shared_memory_id, IPC_RMID, NULL);
			return 6;
		}
		//initialize them with one
		sem_buf.sem_flg = 0;
		for(sem_buf.sem_num = 0; sem_buf.sem_num < nBuffer; sem_buf.sem_num++) {
			sem_buf.sem_op = 1; //add one resource
			if(semop(sem_id, &sem_buf, 1) < 0) {
				cout << "Error doing semop: initializing with one resource";
				delete_shared_memory(shared_memory_id, msg_id, sem_id);
				return 7;
			}
		}
	}

	int nChild = nBuffer / 2;
	//fork off children
	pid_t pid;
	for(int i=1; i <= nChild; ++i) {
		if((pid = fork()) < 0) {
			printf( "Error doing fork(): %s\n", strerror(errno));
			delete_shared_memory(shared_memory_id, msg_id, sem_id);
			return 6;
		}
		//if its a child
		if(pid == 0) {
			int child_id = i;
			stringstream ss (stringstream::in | stringstream::out);
			ss << "Child " << child_id << " initialized.";
			send_msg(msg_id, 1, ss.str());
			int buff_to_access = 0;
			//0 or 1 to read, 2 to write
			int read_or_write = 0;
			buff_to_access = child_id % nBuffer;
			//for 3 times buffer size to write once to each
			for(int j=0; j<(3 * nBuffer); ++j) {

				stringstream ss1 (stringstream::in | stringstream::out);
				if(read_or_write == 0 || read_or_write == 1) {
					//read
					int read_value, next_read_value;
					//if use semaphores
					if(lock) {
						sem_buf.sem_op = -1; //take resource
						sem_buf.sem_num = buff_to_access;
						//lock the resource
						if(semop(sem_id, &sem_buf, 1) < 0) {
							ss1 << "Q Error locking in read child ";
							ss1 << child_id;
							send_msg(msg_id, 1, ss1.str());
							return 9;
						}
						//do the read
						read_value = shared_memory[buff_to_access];
						usleep(K * child_id);
					    next_read_value = shared_memory[buff_to_access];
						sem_buf.sem_op = 1; //free
						//unlock the resource
						if(semop(sem_id, &sem_buf, 1) < 0) {
							ss1 << "Q Error unlocking in read";
							send_msg(msg_id, 1, ss1.str());
							return 9;
						}
					}
					else { //nolocking read
						read_value = shared_memory[buff_to_access];
						usleep(K * child_id);
						next_read_value = shared_memory[buff_to_access];
					}
					if(read_value != next_read_value) {
						//calculate which child changed the buffer
						int difference = next_read_value - read_value;
						int bad_child = 0;
						while(difference > 0) {
							bad_child++;
							difference = difference >> 1;
						}
						stringstream ss (stringstream::in | stringstream::out);
						ss << "E Child " << child_id << " read buffer[" << buff_to_access
						   << "] changed. Initial value: " << read_value
						   << " Final value: " << shared_memory[buff_to_access]
						   << " Suspect: Child " << bad_child;
						string s2(ss.str());
						send_msg(msg_id, 1, s2);
					}
				}
				else { //read_or_write == 2
					//write
					if(lock) {
						sem_buf.sem_op = -1; //take resource operation
						sem_buf.sem_num = buff_to_access;
						if(semop(sem_id, &sem_buf, 1) < 0) {
							ss1 << "Q Error locking in write";
							send_msg(msg_id, 1, ss1.str());
							return 8;
						}
						int read_value = shared_memory[buff_to_access];
						usleep(K * child_id);
						int shifter = (1 << (child_id - 1));
						shared_memory[buff_to_access] = read_value + shifter;
						sem_buf.sem_op = 1; //free operation
						if(semop(sem_id, &sem_buf, 1) < 0) {
							ss1 << "Q Error unlocking in write";
							send_msg(msg_id, 1, ss1.str());
							return 8;
						}
					}
					else { //not locked
						int read_value = shared_memory[buff_to_access];
						usleep(K * child_id);
						int shifter = (1 << (child_id - 1));
						shared_memory[buff_to_access] = read_value + shifter;
					}
				}
				read_or_write = (read_or_write + 1) % 3;
				buff_to_access = (child_id + buff_to_access) % nBuffer;
			}
			stringstream ss1 (stringstream::in | stringstream::out);
			ss1 << "Q Child " << child_id << " is exiting.";
			//send exit message
			send_msg(msg_id, 1, ss1.str());
			return 0; //children return here normally
		}
	}
	//only parent executes after for loop
	if(pid == 0) cout << "error: child is outside of for loop";
	//if children are still running
	bool children_running = true;
	int count_children = nChild;
	//busy waiting loop for children
	//continually checks for messages
	int num_read_errors = 0;
	while(children_running) {
		message f;
		if(msgrcv(msg_id, &f, sizeof(f.text), 1, IPC_NOWAIT) < 0) {
			//no message received
		}
		else {
			cout << "got message:\n" << f.text << endl;
			//if see exit message
			if(f.text[0] == 'Q') {
				--count_children;
				int a;
				//wait on exited child
				wait(&a);
			}
			else if(f.text[0] == 'E') {
				num_read_errors++;
			}
			if(count_children <= 0)
				children_running = false;
		}
	}
	cout << "\nFinished" << endl;
	double correct_value = pow(2.0, (double)(nChild));
	correct_value--;
	int num_write_errors = 0;
	for(int i=0; i<nBuffer; ++i) {
		if(shared_memory[i] != correct_value) {
			num_write_errors++;
			cout << "\nwrite error: mem[" << i << "] = " << shared_memory[i];
		}
	}
	if(num_write_errors > 0) {
		cout << "\nCorrect value: " << correct_value << endl;
	}
	int finaltime = time(NULL);
	cout << num_read_errors << " read errors occurred.\n" << num_write_errors
			 << " write errors occurred.\nSpent "
			 << finaltime - initialtime << " second";
	if((finaltime - initialtime) > 1) {
		cout << "s";
	}
	cout << endl;
	delete_shared_memory(shared_memory_id, msg_id, sem_id);
	return 0;
}
