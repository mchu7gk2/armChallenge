#include <strings.h>
#define __USE_BSD // To ensure we get the random() / srandom() functions out of stdlib.h
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

// We have three possible components on the production line

#define COMPONENT_A    1
#define COMPONENT_B    2
#define COMPONENT_P    3 // finished component P, a combination of A + B 
#define COMPONENT_NULL 0

#define TIME_TO_BUILD_COMPONENT_P 4 // Building P requires 4 additional iterations

// We have 5 slots on the belt, one input, three for three pairs of workers and one output
//     -------------------------------------------------
//  -> | Input |  Workers | Workers | Workers | Output | -> belt
//     -------------------------------------------------

// Define the array indices
#define INPUT_SLOT 0
#define OUTPUT_SLOT 4

// We run for 100 iterations, and count what the workers produce
#define ITERATIONS 100

// Define the belt
unsigned char belt[OUTPUT_SLOT + 1]; 

// Randomness helper function, must return a float between 0.0 and 1.0
float getRandomNumber();

// Small struct to hold the state of each worker
struct worker
{
  unsigned int pos; // Position of the worker
  unsigned char left,right; // What is in each hand
  unsigned int buildTimeLeft; // How much more time I need to build my piece
};

// Places a new item on the conveyor belt (at index INPUT_SLOT)
void placeNewItem ( char* belt );

// Attempts to make the specified worker do work on the specified belt
void doWork ( struct worker *w, char *belt );

#define NUMBER_OF_WORKERS 6 // Ensure this is updated if the number of workers is changed
int main (void)
{
  unsigned long compA = 0, compB = 0, compP = 0, compNull = 0;
  struct worker workers[NUMBER_OF_WORKERS]; // Allocate space for six workers, worker[0] to worker[5]
  
  // Clear the belt out
  bzero( belt, sizeof(belt));
  
  // Clear out the worker state
  bzero( workers, sizeof(workers));
  
  // Setup the workers in three pairs
  workers[0].pos = 1; // Matches up with the array index of the belt
  workers[1].pos = 1;
  workers[2].pos = 2;
  workers[3].pos = 2;
  workers[4].pos = 3;
  workers[5].pos = 3;
  
  // NB: The first ever iteration will always have a zero (i.e COMPONENT_NULL) in both
  // the input and output slots. This should be a valid situation, but for low numbers
  // of iterations, it might skew the results distribution.
  for ( int iteration = 0; iteration < ITERATIONS; iteration++)
  {
    // Count what is coming off the belt, hard coded to deal with just four cases
    if ( belt[OUTPUT_SLOT] == COMPONENT_A)
      compA++;
    if ( belt[OUTPUT_SLOT] == COMPONENT_B)
      compB++;
    if ( belt[OUTPUT_SLOT] == COMPONENT_P)
      compP++;
    if ( belt[OUTPUT_SLOT] == COMPONENT_NULL)
      compNull++;
    
    // Move the belt along
    for ( int bindex = OUTPUT_SLOT; bindex > 0 /* Make sure to stop at index 1 */; bindex--)
    {
      // Work backwards back from the output slot, moving the belt contents along
      belt[bindex] = belt[bindex - 1];
    }
    
    // Place a new item (or NULL item) in the input slot of the belt
    placeNewItem ( belt );
    

    // Make each worker do work, assuming workers are identical, in round-robin style.
    for ( int windex = 0; windex < NUMBER_OF_WORKERS; windex++ )
    {
      doWork ( &workers[windex], belt);
    }
    
  }
  
  // Print out results
  printf(" Belt statistics\n");
  
  printf("\tComponent A was untouched \t\t\t%ld times\n", compA);
  printf("\tComponent B was untouched \t\t\t%ld times\n", compB);
  printf("\tFinished Component P was counted off \t\t%ld times\n", compP);

  return (0);
}



// Attempts to make the specified worker do work on the specified belt
void doWork ( struct worker *w, char *belt )
{
  
  // First, check to see if the worker is assembling, if so, decrement the build time
  if ( w->buildTimeLeft > 0 )
  {
    // Decrement the build time
    w->buildTimeLeft--;
    
    // Check to see if we have just now finished assembling
    if ( w->buildTimeLeft == 0 )
    {
      // For simplicity, always place finished item P in right hand
      w->left = COMPONENT_NULL; // Left hand is now empty
      w->right = COMPONENT_P;
    }
    else
    {
      // Worker is still assembling, nothing more to do on this iteration
      return;
    }
  }
    
  // Has the worker got a finished item to place
  if ( w->right == COMPONENT_P )
  {
    // OK, worker has just finished assembling
    // attempt to place the finished item on the belt
    if ( belt[w->pos] == COMPONENT_NULL )
    {
      // Place the finished item on the belt
      belt[w->pos] = COMPONENT_P;
      
      // Reset the workers hands to empty
      w->left = COMPONENT_NULL;
      w->right = COMPONENT_NULL;
      
      return; // Component placed on belt
    }
    else
    {
      // Worker has a finished item to place but there is no room on the belt,
      // so hold onto it for now
      return;
    }
  }
  
  // If we reach here, the worker is neither assembling nor waiting to place a finished item
  // this means that they are waiting for both parts to begin assembly.
  // So collect any raw component in their slot on the belt, in the hope they can start assembling.
  // We always fill the right hand first, then the left.
  
  // Check there is actually something in the belt slot and that it is not a finished component
  // NB: In other words we filter out anything that is not a raw component for assembly.
  if ( (belt[w->pos] != COMPONENT_NULL )  && (belt[w->pos] != COMPONENT_P) )
  {
    // Try to place in our right hand
    if ( w->right == COMPONENT_NULL )
    {
      // Pick up the component with our right hand
      w->right = belt[w->pos];
      belt[w->pos] = COMPONENT_NULL; // Clear the belt slot
      return; // Picking up one item is all we can do in a single iteration, so return
    }
    
    // If we reach here, the right hand already had a component, check the belt contains a component
    // the worker doesn't already have in the right hand.
    // NB: Relies on the assumption there are only two real component types available at this point
    if ( w->right != belt[w->pos])
    {
      w->left = belt[w->pos]; // Pick up the item with our left hand
      belt[w->pos] = COMPONENT_NULL; // Clear the belt slot
      
      // Now, relying on the assumption there are only two component types, we must now have both
      // as we have two hands with dissimilar types, so start assembling the finished item
      w->buildTimeLeft = TIME_TO_BUILD_COMPONENT_P;
      return;
    }
  }
  
  // If we reach here, there is either nothing in the slot, or a finished component we don't want to
  // pick up and there is also no other work we can do.
}


// Places a new item on the conveyor belt (at index INPUT_SLOT)
#define ONE_THIRD ( 1.0 / 3.0 )
#define TWO_THIRDS ( 2.0 / 3.0 )
void placeNewItem ( char* belt )
{
  // This function is hard coded to deal with just three possible components
  
  // Probability space, split into thirds
  //  ---------------------------------------------
  //  0.0 | Component A |  Component B | NULL | 1.0
  //  ---------------------------------------------
  
  // Get a new floating point random number between 0.0 and 1.0
  float r = getRandomNumber();
  
  // if r is between 0.0 and ONE_THIRD, place component A onto the belt
  if ( r >= 0 && r < ONE_THIRD )
    belt[INPUT_SLOT] = COMPONENT_A;

  // if r is between ONE_THIRD and TWO_THIRDS, place component B onto the belt
  else if ( r >= ONE_THIRD && r <= TWO_THIRDS)
    belt[INPUT_SLOT] = COMPONENT_B;
  
  // otherwise, place a NULL component
  else  
    belt[INPUT_SLOT] = COMPONENT_NULL;
}

float getRandomNumber()
{
  static int seeded = 0;
  float p = (float) 0.0;
  
  if (seeded == 0)
  {
#if defined (__APPLE__)
    srandomdev(); // Seed the Standard C library randomness thing
#else
    struct timeval tv_struct[2];
    gettimeofday(tv_struct, 0);
    srandom (tv_struct[0].tv_sec % tv_struct[0].tv_usec);
#endif
    seeded = 1;
  }
  
  // rand() spews out an integer between 0 and RAND_MAX, so divide by RAND_MXA
  // to end up with something between 0.0 and 1.0.
  p = ((float) random()) / ((float) RAND_MAX);
  
  return (p);  
}
