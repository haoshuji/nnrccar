//
//  main.cpp
//
//  Created by David Singleton on 06/03/2012.
//

#include "PracticalSocket.h"  // For Socket, ServerSocket, and SocketException
#include <iostream>           // For cout, cerr
#include <cstdlib>            // For atoi()  
#include <pthread.h>          // For POSIX threads  
#include <stdio.h>

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#else
#include <cblas.h>
#endif

#include "FeatureStreamer.h"
#include "Mailbox.h"
#include "NeuralNetwork.h"

double m[] = {
    3, 1, 3,
    1, 5, 9,
    2, 6, 5
};

double x[] = {
    -1, -1, 1
};

double y[] = {
    0, 0, 0
};

int test_blas() {
  int i, j;
    
  for (i=0; i<3; ++i) {
    for (j=0; j<3; ++j) printf("%5.1f", m[i*3+j]);
    putchar('\n');
  }
    
  cblas_dgemv(CblasRowMajor, CblasNoTrans, 3, 3, 1.0, m, 3,
	      x, 1, 0.0, y, 1);
    
  for (i=0; i<3; ++i) {
    printf("%5.1f\n", y[i]);
  }

  return 0;
}


void HandleTCPClient(TCPSocket *sock);     // TCP client handling function
void *ThreadMain(void *arg);               // Main program of a thread  
void *ConsumerThreadMain(void *arg);               // Main program of a thread  

struct Handle {
  TCPSocket* socket;
  Mailbox<Frame>* mailbox;
};

int main(int argc, char *argv[]) {
  test_blas();

  if (argc != 2) {                 // Test for correct number of arguments  
    cerr << "Usage: " << argv[0] << " <Server Port> " << endl;
    exit(1);
  }
    
  unsigned short echoServPort = atoi(argv[1]);    // First arg:  local port

  Mailbox<Frame> mailbox;

  pthread_t consumerThreadID;
  Handle consumerHandle;
  consumerHandle.socket = NULL;
  consumerHandle.mailbox = &mailbox;

  if (pthread_create(&consumerThreadID, NULL, ConsumerThreadMain, 
		     (void *) &consumerHandle) != 0) {
    cerr << "Unable to create thread" << endl;
    exit(1);
  }

  try {
    TCPServerSocket servSock(echoServPort);   // Socket descriptor for server  
        
    for (;;) {      // Run forever  
      // Create separate memory for client argument  
      TCPSocket *clntSock = servSock.accept();
      
      // Create client thread  
      pthread_t threadID;              // Thread ID from pthread_create()
      Handle handle;
      handle.socket = clntSock;
      handle.mailbox = &mailbox;
      if (pthread_create(&threadID, NULL, ThreadMain, 
			 (void *) &handle) != 0) {
	cerr << "Unable to create thread" << endl;
	exit(1);
      }
    }
  } catch (SocketException &e) {
    cerr << e.what() << endl;
    exit(1);
  }
  // NOT REACHED
  return 0;
}

// TCP client handling function
void HandleTCPClient(Handle* handle) {
  TCPSocket* sock = handle->socket;
  cout << "Handling client ";
  try {
    cout << sock->getForeignAddress() << ":";
  } catch (SocketException &e) {
    cerr << "Unable to get foreign address" << endl;
  }
  try {
    cout << sock->getForeignPort();
  } catch (SocketException &e) {
    cerr << "Unable to get foreign port" << endl;
  }
  cout << " with thread " << pthread_self() << endl;
  
  FeatureStreamer* streamer = new FeatureStreamer(sock, handle->mailbox);
  streamer->Stream();

  delete streamer;
}

void *ThreadMain(void *handl) {
  // Guarantees that thread resources are deallocated upon return  
  pthread_detach(pthread_self()); 
  
  Handle* handle = (Handle*) handl;
  // Extract socket file descriptor from argument  
  HandleTCPClient(handle);
  
  delete (TCPSocket *) handle->socket;
  return NULL;
}

void *ConsumerThreadMain(void *handl) {
  // Guarantees that thread resources are deallocated upon return  
  pthread_detach(pthread_self()); 
  
  Handle* handle = (Handle*) handl;

  NeuralNetwork* nn = new NeuralNetwork(string("data/theta1.dat"),
					string("data/theta2.dat"));

  time_t lap = time(NULL);
  int count = 0;
  while(true) {
    Frame* frame = handle->mailbox->blockingFetch();
    count++;
    cout << "Frame! " << frame->width_ << "x" << frame->height_ << endl;
    double* res = nn->predict(frame);
    cout << " P: " << res[0] << " " << res[1]
	 << " " << res[2] << " " << res[3] << endl;
    delete res;
    delete frame;

    time_t now = time(NULL);
    if ((now - 10) >= lap) {
      cerr << "nn: " << (count / (now - lap)) << " fps" << endl;
      count = 0;
      lap = time(NULL);
    }
  }

  cout << "Consumer Thread exiting" << endl;
  delete nn;
  return NULL;
}
