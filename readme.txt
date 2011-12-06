/*
 * vmpager.cpp
 * Ryan Konkul
 * rkonku2
 * CS 385
 * Homework 5: procsync
 */

The program may be compiled with

make procsync

The command line argument required is nBuffer which must be a prime number greater or equal to 5. Optional arguments are -lock or -nolock and a third argument which is the time for each process to sleep in nano seconds. The greater the length, the more likely it is for the processes to have read errors. 

Known errors/issues: none

The program tests process synchronization. A number of processes are forked and read or write to a shared memory segment. First shared memory is acquired then the id is used to attach to the parent's address space. The forked children do not need to attach again since parent has already done that. The shared memory is initialized to zero. Then the message queue is acquired and attached as well. Then semaphores are acquired and initialized with one resource each.

The important part of the program loops and forks off nBuffer / 2 children to read or write to an array of ints of size nBuffer. buff_to_access is which section of the array the child is to read/write to. read_or_write is an int that has 3 values: 0, 1, or 2. 0 or 1 means the child is to read. 2 means the child will write. After every read and write, read_or_write is incremented and modded by 3. In this way, every child will write once for every 2 reads.

Reads are done by saving the value in the buffer, sleeping then comparing the value in the buffer with the value saved. If the is a difference, a message is placed in the message queue which guesses which child changed the buffer. 

If semaphore locks are to be used, the process first decrements the semaphore to lock. Then the process does its read or write, and unlocks the semaphore.

The parent continually checks for messages in the message queue. If the first character is a Q, the number of children waiting for is decremented and the exited child is wait()ed for. If the first character is an 'E', then the number of read errors is incremented. When all children have sent a 'Q' message, the parent calculates how many write errors occurred and displays the results. 