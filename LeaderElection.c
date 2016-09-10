/* CS 370
   Project #4 
   Gerardo Gomez-Martinez
   November 4, 2014

   Program implements Peterson's distributed leader election algorithm, using threads and
   semaphores for critical sections and synchronization. Program accepts a file via input
   redirection containing the number of nodes in the network in the first line and the uid
   for each node in each subsequent line.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <semaphore.h>
#include <pthread.h>

/* Struct for each node in the network */
struct node
{
  int uid; /* Node's original uid */
  int tempUid; /* Node's temporary uid */
  bool active; /* Whether the node is currently active or not */
};

/* Linked list node for information on the channel */
struct channelNode
{
  int info; /* Information on the channel */
  struct channelNode *next; /* Link to next node */
};

/* Struct for a channel*/
struct channel
{
  struct channelNode *values; /* Linked list to hold the information */
  sem_t syncSem; /* Synchronization semaphores */
  sem_t raceSem; /* Race condition semaphores */
};

/* Struct to pass information to threaded function */
struct id
{
  int size; /* Number of nodes in the channel */
  int tid; /* The thread's ID */
  bool finished; /* If a leader has been found */
  int leader; /* Network's leader */
  int *phase; /* Array of the phase each thread is on */
  sem_t idSem; /* Thread ID semaphores */
  struct channel *ch; /* Array of channels for each node */
  struct node *nd; /* Array of nodes */
};

/* Adds a value to the channel linked list */
void addV(struct channelNode **currChannel, int value)
{
  struct channelNode *newNode; /* new node to be added to the list */
  struct channelNode *curr; /* place where the node should be added */
  
  /* Creates the new node to be added */
  newNode = (struct channelNode *) malloc(sizeof(struct channelNode));
  (*newNode).info = value;
  (*newNode).next = NULL;

  /* Finds the correct place and inserts the new node at the end of the list */
  if(*currChannel == NULL)
    {
      *currChannel = newNode;
    }
  else
    {
      curr = *currChannel;
      while((*curr).next != NULL)
        curr = (*curr).next;
        
      (*curr).next = newNode;
    }
}

/* Removes and returns a value from the channel linked list */
int removeV(struct channelNode **currChannel)
{
  int value; /* Value to be removed */
  struct channelNode *curr; /* Next value in the list */
  
  /* Finds the value on the front of the list and returns it */
  if ((*currChannel) != NULL)
    {
      value = (*currChannel)->info;
      curr = (*currChannel);
  
      *currChannel = (*currChannel)->next;
  
      free(curr);
  
      return value;
    }
  else
    return -1;
}

/* Accepts a channel as an arguments and reads and returns the next value in the channel */
int read(struct channel *currChannel)
{
  int value; /* Value to be read */

  /* Semaphores for synchronization and race condition*/
  sem_wait(&(*currChannel).syncSem);
  sem_wait(&(*currChannel).raceSem);
  
  /* calls removeV to get and remove the next value in the list */
  value = removeV(&(*currChannel).values);
  
  sem_post(&(*currChannel).raceSem);

  return value;
}

/* Accepts a channel and a value as an argument and writes the value into the channel */
void write(struct channel *currChannel, int value)
{
  /* Semaphores for race condition*/
  sem_wait(&(*currChannel).raceSem);

  /* calls addV to add the value to the list */
  addV(&(*currChannel).values, value);
  
  sem_post(&(*currChannel).raceSem);
  sem_post(&(*currChannel).syncSem);
}

/* Function for the execution of each thread */
void process(void *info)
{
  struct id *threads = info; /* Information about the network */
  int n = threads->size; /* Number of nodes in the network */
  int thread; /* This thread's ID */
  int temp; /* Node's temporary UID */
  int oneHop; /* Node's one hop UID */
  int twoHop; /* Node's two hop UID */
  
  /* Assigns a new id to each thread */
  sem_wait(&(*threads).idSem);
  thread = threads->tid; 
  threads->tid++;
  sem_post(&(*threads).idSem);
  
  /* Executes the algorithm until a leader is found */
  while (threads->finished == false)
    {
      /* node is active */
      if ((*threads).nd[thread].active == true)
        {
          temp = (*threads).nd[thread].tempUid;
          /* Prints the current phase, node's uid and node's temp uid */
          printf("[%d] [%d] [%d]\n", threads->phase[thread], 
                (*threads).nd[thread].uid, (*threads).nd[thread].tempUid);
      
          /* write node's temp uid */
          write(&(*threads).ch[(thread+1)%n], temp);
          /* read one hop temp uid */
          oneHop = read(&(*threads).ch[thread]);
          /* write one hop temp uid */
          write(&(*threads).ch[(thread+1)%n], oneHop);
          /* read two hop temp uid */
          twoHop = read(&(*threads).ch[thread]);
        
          if (oneHop == temp)
            {
              /* this node is the leader, sets finished to true and prints its uid */
              threads->finished = true;
              threads->leader = (*threads).nd[thread].uid;
              printf("Leader: %d\n", threads->leader);
              /* frees other nodes waiting to read from channel */
              for (temp = 0; temp < n; temp++)
                {
                  sem_post(&(*threads).ch[temp].syncSem);
                  
                }
            }
          else if (oneHop > twoHop && oneHop > temp)
            {
              /* node remains active and its one hop uid becomes its temp uid */
              (*threads).nd[thread].tempUid = oneHop;
            }
          else
            {   
              /* node becomes a relay node */
              (*threads).nd[thread].active = false;
            }  
        }
      else
        { 
          /* node is a relay node */
          /* reads temp uid */
          temp = read(&(*threads).ch[thread]);
          if (threads->finished != true)
           {
             /* writes temp uid */ 
             write(&(*threads).ch[(thread+1)%n], temp);
             /* reads temp uid */
             temp = read(&(*threads).ch[thread]);
             /* writes temp uid */
             write(&(*threads).ch[(thread+1)%n], temp);
           }
        }
    
      /* Moves to the next phase */
      threads->phase[thread]++;
    }
  
  /* Exits the thread */
  pthread_exit(NULL);
}

int main()
{
  char buffer[50]; /* buffer for input */
  int n; /* number of nodes in the network */
  int i; /* loop control variable */
  int num; /* the current number being read */
  struct node *nodes; /* Array of nodes */
  struct channel *channels; /* Array of channels */
  struct id threads; /* information about the threads */
  pthread_t *threadIds; /* thread IDs */
  
  /* Reads the number of nodes from file */
  fgets(buffer, sizeof(buffer), stdin);
  sscanf(buffer, "%d", &n);
  
  /* Allocates enough memory for the channels, nodes, and threadIDs, and phase arrays */
  channels = (struct channel *) malloc(sizeof(struct channel)*n);
  nodes = (struct node *) malloc(sizeof(struct node)*n);
  threadIds = (pthread_t *) malloc(sizeof(pthread_t)*n);
  threads.phase = (int *) malloc(sizeof(int)*n);
  
  /* Adds the info to the id struct to be passed to thread function */
  threads.size = n;
  threads.ch = channels;
  threads.nd = nodes;
  threads.tid = 0; 
  threads.finished = false;
  
  
  /* Executes for each node */
  for(i = 0; i < n; i++)
    {
      /* Reads the next node's uid from the file */
      fgets(buffer, sizeof(buffer), stdin);
      sscanf(buffer, "%d", &num);
      
      /* Sets the node's uid and temp uid and it's status to active */
      nodes[i].uid = num;
      nodes[i].tempUid = num;
      nodes[i].active = true;
      
      /* Sets the node's phase to one */
      threads.phase[i] = 1;
      
      /* Initializes this node's channel sempahores */
      sem_init(&channels[i].syncSem, 0, 1);
      sem_init(&channels[i].raceSem, 1, 1);
      sem_init(&threads.idSem, 0, 1);
      /* Initializes the channel's list no NULL */
      channels[i].values = NULL;
      
      /* Initializes the channel's sync semaphore to wait */
      sem_wait(&channels[i].syncSem);
      
      /* Creates the thread */
      pthread_create(&threadIds[i], NULL, (void *)process, &threads);
    }
  
  /* Waits for the threads to finish executing */  
  for(i = 0; i < n; i++)
    {
      pthread_join(threadIds[i], NULL);
    }

  return(0);
}
