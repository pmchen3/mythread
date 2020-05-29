# MyThread
MyThread is a custom user-level thread library.

## Usage
A set of functions are available that can be used to create and manage threads in a program.

* MyThreadInit - Create and run the "main" thread
* MyThreadCreate - Create a new thread
* MyThreadYield - Yield invoking thread
* MyThreadJoin - Join with a child thread
* MyThreadJoinAll - Join with all children
* MyThreadExit - Terminate invoking thread

Include the header file and compile together with your program. Refer to the Makefile for compiling with an example fib.c program. The fib.c program demonstrates how the thread library can be used in evaluating a Fibonacci number. Additional examples are available in the test folder.

A set of semaphore functions are also included in addition to the thread functions.
