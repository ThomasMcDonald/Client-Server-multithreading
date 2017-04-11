# SystemProgrammingAssignment2

## PURPOSE ###
This is a client server program created using shared memory and multithreading.
The client sends the server an unsigned long and will start up either the number of specified threads if given or as many
threads as there are binary digits(i.e 32 threads). Each thread is responsible for factorising an integer derived from the input number
that is rotated right by a different number of bits. The client will immediately report andy responses from the server.


The server can handle 10 simultaneous requests without blocking.
The client is non-blocking, and up to 10 server reponses may be outstandiong at any time.


## INSTRUCTIONS ##

### Compiling the Server/Client ###
  Linux:
  To compile the software simply use the provided Makefile for each using the “make” command in a linux terminal.
  Windows:
  Compile using visual studio
### Running the Server ###
  The server program does not require user input to run properly, It is optional to add how many threads you would like to run per thread.
  If at any point you need to stop the server simply use CTRL^C.
### Running the Client ###
  To start the client you need to start the executable.
  If at any point you need to stop the client you need to input the quit command, or simply use
  CTRL^C. 
### Commands ### 
The user can input the commands:
  q – Will quit the program, this is has the same affect as CTRL ^ C 
  quit - Will quit the program, this is has the same affect as CTRL ^ C
  
A number can be inputted into the program, it will then be sent to the server to perform the factorisation and rotations on.

If the user inputs a 0 it will activate the servers test mode, which will block all user input and count from 0-29 using different threads.
