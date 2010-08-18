
// ARM production line coding challenge

// Notes:
// - fetch randomness via a helper function to allow different sources in the future
// - start pointer advances "left" around the ring, and deposits a new item on the belt
// - workers are then selected in random order, advanced one slot, and tested to see if they
//	 can act upon the new slot, this allows for the possibility of them being able to access multiple
//   slots in future.
// - the end pointer then collects whatever is coming off the end of the belt and records it.

// Data storage:
// - Conveyor belt modelled as an pointer array, 
// - The belt is represented by a C++ class, using opaque handles for the items, which would allow the items
//   to be of any size and disposition and stored in any manner underneath
// - The workers are separate instances of a Worker class, they call out for their needs to functions for
//   randomness and also handlers to store and assemble a list of item types.
// - The belt contains instances of the item class, which has an opaque item ID, the item class can be queried to
//   see if it can be combined with an other item.

// Expansion possibilities:
// - The simulation can be expanded to allow workers to "see" and use multiple slots at once, like a peephole
// - The Worker class can deal with one-handed workers, workers with a storage bucket and differing speeds of work,
//   including breaks, where the worker is notionally absent from teh production line for a number of intervals
// - Sources of entropy include /dev/random style inputs, also thread scheduling entropy if multiple threads and
//   condition variables are used to dispatch the workers during each interval. To avoid cpu thrashing, this should
//   include use of backoff algorithms and scheduling yield() calls.

#include <sys/types.h> // For u_int64_t etc
#include <stdio.h> // for printf, stdin, stdout etc
#include <string.h> // for strdup
#include <stdlib.h> // for atoi
#include <sys/time.h> // for gettimeofday etc (on Linux builds)

// GK: For portability, use internal type names and map them here
typedef u_int64_t u64int;
typedef u_int32_t u32int;
typedef u_int8_t u8int;
typedef u_int8_t ascii;
typedef float probability;

#define NULL_ITEM_ID ~0

//#define DEBUG printf
#define DEBUG(...) {}

// Randomness helper function, must return a probability between 0.0 and 1.0
probability getRandomNumber()
{
  static int seeded = 0;
  probability p = (probability) 0.0;
  
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
  p = ((probability) random()) / ((probability) RAND_MAX);
  
  return (p);  
}

class ItemType
{
private:
  ascii name; // For now we use a single char for the name, for this simple version
  u32int id;
  u32int weight;
  float generationProbability; // A classic probabilty number (0 = never, 1 = certainty) of whether this item
                              // will be generated in any given instance.
  u32int numberCollected; // Records how many of this item were counted off the end of the belt
  ItemType **componentsRequired; // In the case where this a composite item, this is the NULL terminated array
                                // of the components required to complete it.
  
public:
  ItemType *nextItemType;
  
  ItemType()
  {
    // With no constructor args, we create a null item
    id = NULL_ITEM_ID; // Eye-catching constant for a null item ID.
    name = '\0';
    nextItemType = NULL;
    numberCollected = 0;
    weight = 0;
    componentsRequired = NULL;
  }
  
  ItemType( ascii itemType )
  {
    // Constructed with an item name, also convert to an id for convenience
    id = (u32int) itemType;
    name = itemType;
    nextItemType = NULL;
    numberCollected = 0;
    weight = 0;
  }
  
  ascii getName ()
  {
    return name;
  }
  
  u32int getId ()
  {
    return id;
  }
  
  void setWeighting ( u32int w )
  {
    weight = w;
  }
  
  u32int getWeighting ()
  {
    return weight;
  }
  
  void setGenerationProbability ( probability p )
  {
    generationProbability = p;
  }
  
  probability getGenerationProbability ()
  {
    return generationProbability;
  }
  
  void incrementNumberCollected()
  {
    numberCollected++;
  }
  
  u32int getNumberCollected()
  {
    return numberCollected;
  }
    
  void deleteNextItem()
  {
    if ( nextItemType != NULL )
    {
      // If there is more than one more in the list, recurse down.
      if (nextItemType->nextItemType != NULL )
      {
        nextItemType->deleteNextItem();
      }        
      // By now, the others will have been deleted, so delete the neighbour.
      delete nextItemType;
    }
  }
  
  void setComponentsRequired (  ItemType **crqd )
  {
    componentsRequired = crqd;
  }
  
  ItemType *assemble ( ItemType **componentsAvailable )
  {    
    if ( componentsRequired == NULL )
    {
      return NULL;
    }
    
    DEBUG("assemble called\n");
    
    // For each required component, search the array of components available
    u32int x = 0;
    while ( componentsRequired[x] != NULL)
    {
      bool componentFound = false;

      u32int y = 0;
      while ( componentsAvailable[y] != NULL )
      {
        DEBUG("it = %p, nextRequiredComponent = %p\n",componentsAvailable[y], componentsRequired[x]);
        if (componentsAvailable[y]->getId() == componentsRequired[x]->getId())
        {
          // We've got one of these
          componentFound = true;
          break;
        }
        else
        {
          componentFound = false;
        }
        y++; // Move down the array;
      }
      
      // If we're missing any of the components, return NULL
      if ( componentFound == false )
      {
        return NULL;
      }
      x++;
    }
    
    return this; // Return me (a finished component) if assembly was successful.
  }
  
  ~ItemType()
  {
    // Nothing to be done to destruct yet.
  }
    
};

#define ASSEMBLE_TIME 4 // It takes us four cycles to build, once we have the necessary pieces

class Worker
{
private:
	ascii *name;
  u32int position;
  u32int weight;
  probability workProbability; // A classic probabilty number (0.0 = never, 1.0 = certainty) of whether this worker
                          // will do work in any given instance.
  bool doneWork;
  u32int amAssembling; // Zero indicates we are not assembling, a positive integer indicates how many cycles are left before the
                      // assembly is finished.
  ItemType *leftHand, *rightHand;
  
public:
  Worker *nextWorker;
  
  Worker()
  {
    position = 0;
    workProbability = 0.0;
    weight = 0;
    amAssembling = 0; 
    doneWork = false;
    leftHand = NULL, rightHand = NULL;
  }
  
  void setPosition ( u32int p)
  {
    position = p;
  }

  u32int getPosition ()
  {
    return position;
  }
  
  void setWorkProbability ( probability p )
  {
    workProbability = p;
  }
  
  probability getWorkProbability ()
  {
    return workProbability;
  }
  
  void setWeighting ( u32int w )
  {
    weight = w;
  }
  
  u32int getWeighting ()
  {
    return weight;
  }
  
  void setHasDoneWork(bool w)
  {
    doneWork = w;
  }
  
  bool getHasDoneWork()
  {
    return doneWork;
  }
  
  // GK: TODO, this function is oversimplified currently, dealing with only one finished item and two hands. 
  ItemType *doWork ( ItemType *it /* New item offered */, ItemType *finishedProductsToBuild /* Things we are trying to make */ )
  {
    ItemType *out = it; // out is what we "return" to the belt
    ItemType *finishedThing = finishedProductsToBuild; // GK: Bodge, I was running out of time, take the head of the list.
    
    // First mark ourselves as having done work (or at least attempted it).
    doneWork = true;
    
    // If we are assembling, decrement the cycle counter and just return, we can't do anything else
    if ( amAssembling > 0 )
    {
      amAssembling--;
      
      // If we just now finished assembling, place the finished item on the belt
      if (amAssembling == 0)
      {
        out = finishedThing;
      }
      
      return out;
    }
    
    // Is the belt slot empty ?
    if (!(it == NULL || it->getId() == NULL_ITEM_ID)) // There can be two sorts of empty slot
    {
      // Slot has contents, so do we have an empty hand to put "it" in ?
      if ( leftHand == NULL)
      {
        // Check we don't already have an "it" in the other hand
        if (it != rightHand)
        {
          // Put the it in our left hand
          leftHand = it;
          out = NULL; // As it stands currently, we have emptied the slot
        }
      }
      if ( rightHand == NULL)
      {
        // Check we don't already have an "it" in the other hand
        if (it != leftHand)
        {
          // Put the it in our right hand
          rightHand = it;
          out = NULL; // As it stands currently, we have emptied the slot
        }
      }
    }
    
    // Now check to see if we can start assembling
    if ( leftHand != NULL && rightHand != NULL)
    {
      ItemType *hands[] = { leftHand, rightHand, NULL };
      // See if the items can be combined
      if (finishedThing->assemble (hands) != NULL)
      {
        // Start assembling.
        amAssembling = ASSEMBLE_TIME; // GK: TODO, make this flexible
      }
    }
      
    return out;
  }
  
  void deleteNextWorker()
  {
    if ( nextWorker != NULL )
    {
      // If there is more than one more in the list, recurse down.
      if (nextWorker->nextWorker != NULL )
      {
        nextWorker->deleteNextWorker();
      }        
    }
  }
};

class Belt
{
private:
	u32int numberOfSlots;
  Worker *workers;
  ItemType *itemsToMake;
  ItemType *finishedItems;
  u64int totalItemWeighting;
  u64int totalWorkerWeighting;
  probability currentWorkerMaxProbability; // Starts off at 1.0 and is reduced to match the workers yet to work in the current interval
  ItemType **beltSlots;
  
public:
    
  Belt(int slots = 3)
  {
    workers = NULL;
    itemsToMake = NULL;
    totalItemWeighting = 0;
    totalWorkerWeighting =0;      
    numberOfSlots = slots;
    currentWorkerMaxProbability = 1.0;

    // GK: Use malloc here because I don't want to call the destructor on the items in the slots upon deletion
    beltSlots = (ItemType** ) malloc (slots * sizeof(ItemType));
    
    for (int i = 0; i < slots; i++)
    {
      beltSlots[i]= NULL ; // Fill the slots with the NULL pointer for now.
    }
  }
  
  void addWorker( Worker *newWorker, u32int position, u32int weighting = 50 /* Even, middle of the road weighting by default */)
  {
    // Add the new worker to the start of the worker list (simpler and faster than finding the end)
    Worker *oldHead = workers;
    
    workers = newWorker; // the newWorker is the head of the worker list
    newWorker->nextWorker = oldHead; // attach the other workers onto the new one.
    
    newWorker->setPosition(position);
  
    // Remember its weighting in the object
    newWorker->setWeighting ( weighting );
    
    // Keep a record of the total weight (so relative probabilities can be calculated ).
    totalWorkerWeighting += weighting;
    
    // Recalculate the item type probabilities
    Worker *wk = workers;
    
    while ( wk != NULL )
    {
      probability p; // Probability for this item type to be generated in any given iteration
      
      p =  ((probability) wk->getWeighting() ) / ((probability) totalWorkerWeighting);
      wk->setWorkProbability (p);
      DEBUG("Probability of worker %p working = %f\n", wk, wk->getWorkProbability());
      
      wk = wk->nextWorker;
    }
    // Recalculating the probabilities is based on a max of 1.0
    currentWorkerMaxProbability = 1.0;
  
  }

  void addItemFactory ( ItemType *newType, u32int weighting )
  {
    ItemType *oldHead = itemsToMake;
    
    // Add the new item factory to the head of the list.
    itemsToMake = newType;
    newType->nextItemType = oldHead;
    
    // Remember its weighting in the object
    newType->setWeighting ( weighting );
    
    // Keep a record of the total weight (so relative probabilities can be calculated ).
    totalItemWeighting += weighting;
    
    // Recalculate the item type probabilities
    ItemType *it = itemsToMake;
    
    while ( it != NULL )
    {
      probability p; // Probability for this item type to be generated in any given iteration
      
      p =  ((probability) it->getWeighting() ) / ((probability) totalItemWeighting);
      it->setGenerationProbability (p);
      
      it = it->nextItemType;
    }
  }

  void addFinishedItem ( ItemType *fit)
  {
    ItemType *oldHead = finishedItems;
    
    // Add the new finished item type to the head of the list.
    finishedItems = fit;
    fit->nextItemType = oldHead;
  }
  
  ItemType *getFinishedItems ()
  {
    return finishedItems;
  }
  
  ItemType *getNextItem()
  {
    // We select the next item randomly but according to probability weight
    // this is done by laying out the itemTypes across the probability space between 0.0 and 1.0
    // The total probabilities should already add up to 1.0 as they are calculated afresh as each
    // item type is added.
    probability p = getRandomNumber ();
    probability cumulative = (probability) 0.0;
    
    ItemType *it = itemsToMake;
    while ( it != NULL )
    {
      cumulative += it->getGenerationProbability();
      if ( p <= cumulative )
      {
        return it;
      }
      it = it->nextItemType;
    }
    printf ("Eeek, probability p (%f) did not hit any items in the probability space.\n",p);
    
    return NULL;
  }
  
  Worker *getNextWorker()
  {
    // We select the next worker to act in any given instance randomly from the list of workers
    // taking into account probability weighting, much as with the items.
    // Once a worker has been selected, they are marked as such and removed from the probability
    // pool, until the last worker has 100% probabilty of being next. Advancing the belt resets
    // all workers to the unselected state.
    
    probability p = getRandomNumber ();
    probability cumulative = (probability) 0.0;
    
    // Adjust p by the maximum probability space (so for instance, if the first worker in this instance
    // (with a 0.2 probability)  has done work, the maximum space is now 0.8
    p = p * currentWorkerMaxProbability;
    
    Worker *wk = workers;
    while ( wk != NULL )
    {
      if (wk->getHasDoneWork() == true)
      {
        // This worker has already done work, so skip
        wk = wk->nextWorker;
        continue;
      }
      
      cumulative += wk->getWorkProbability();
      if ( p <= cumulative )
      {
        // p fell into the bucket for this worker, so select, recalcuate the max probability space for everyone else
        // and return.
        
        // Adjust the probability space.
        currentWorkerMaxProbability -= wk->getWorkProbability();
        
        return wk;
      }
      wk = wk->nextWorker;
    }
    printf ("Eeek, probability p (%f) did not hit any items in the probability space.\n",p);
    
    return NULL;
  }
  
  void setSlot ( ItemType *itemType, u32int slot )
  {
    beltSlots[slot] = itemType;
  }
  
  ItemType *getSlot ( u32int slot )
  {
    return beltSlots[slot];
  }

  void advanceBelt ( int n )
  {
    // Move the "belt" along by n slots, new slots are zero'd, items coming off the belt are counted by type
    // Also resets the workers along the belt into their unselected state.
    
    // First, take the n last slots off the belt and count any items
    u32int lastSlot = (numberOfSlots - 1); /* Zero based array */
    for (int i = lastSlot; i > (lastSlot - n); i--)
    {
      ItemType *it = beltSlots[i];
      
      if ( it != NULL )
      {
        it->incrementNumberCollected();
      }
    }
    
    // Now move all the pointers down n slots
    for (int i = numberOfSlots; i > 0; i--)
    {
      // Determine whether we are at least n slots from the start of the belt
      // this avoids copying from negative slot positions, which is a nonsense.
      if ( i >= n )
      {
        // Copy the pointers from the adjacent slots        
        beltSlots[i] = beltSlots[i-n];
      }
      else
      {
        // We need to zero-fill at the start of the belt
        beltSlots[i] = NULL;
      }
    }
    
    // Now reset the worker probability space
    currentWorkerMaxProbability = 1.0;
    
    // Also reset the workers to not having done work
    Worker *wk = workers;
    
    while ( wk != NULL )
    {
      wk->setHasDoneWork (false);
      wk = wk->nextWorker;
    }
    
  }
  
  void printItemFactoryCounts ()
  {
    ItemType *it = itemsToMake;
    
    while ( it != NULL )
    {
      printf("Item \"%c\", was collected off the belt %d times\n", it->getName(), it->getNumberCollected());
      
      it = it->nextItemType;
    }
  }

  void printFinishedItemCounts ()
  {
    ItemType *it = finishedItems;
    
    while ( it != NULL )
    {
      printf("Item \"%c\", was collected off the belt %d times\n", it->getName(), it->getNumberCollected());
      
      it = it->nextItemType;
    }
  }
  
  ~Belt()
  {
    // We get each worker in the list to recurse and delete its neighbour, then we delete the final one.
    if ( workers != NULL )
    {
      workers->deleteNextWorker();
      delete workers;
    }
    
    // We get each item in the list to recurse and delete its neighbour, then we delete the final one.
    if ( itemsToMake != NULL )
    {
      itemsToMake->deleteNextItem();
      delete itemsToMake;
    }
    
    free (beltSlots); // Use free here to match the malloc
  }

};

class ProductionLine
{
private:

  Belt *belt;
public:
  
  ProductionLine()
  {
    belt = NULL;
  }
  
  void addBelt ( Belt *b)
  {
    belt = b;
  }
  
  ~ProductionLine()
  {
    if ( belt != NULL )
    {
      delete belt;
    }
  }
  
  void runSim ( u32int steps)
  {
     
    for (int i = 0; i < steps; i++)
    {
   
      // Get the next item to place on the belt
      ItemType *next = belt->getNextItem();
      if (next == NULL )
      {
        printf("error: getNextItem failed to produce anything\n");
        return;
      }
      DEBUG("Next item is \"%c\".\n", next->getName()); 
   
      // Advance the belt (and count things coming off the end)
      belt->advanceBelt( 1 /* one step */);
      
      // Insert the new item into the entry slot
      belt->setSlot( next, 0 /* slot zero - entry slot */);
      
      // Now prod each worker into doing work 
      
      Worker *wk = belt->getNextWorker();
      
      // Find out which position this worker is at and return the contents of that slot
      u32int workerPosition = wk->getPosition();
      ItemType *it = belt->getSlot( workerPosition );
      ItemType *newIt = wk->doWork ( it, belt->getFinishedItems() );
      
      // Offer the item to the worker, worker returns the new contents of the slot ( which may be the same thing we gave them )
      belt->setSlot ( newIt, workerPosition );
      
    }
    
  }
  
  void printResults()
  {
    belt->printItemFactoryCounts();
    
    // Now print the number of finished items
    belt->printFinishedItemCounts();
  }
};


#define NUMBER_OF_STEPS 100 
        
int main (void)
{
  printf("ARM production line coding challenge\n\n");
  
  // In this simple sim we have two item types 
  
  ItemType *itemA = new ItemType( 'A' ); // Component A
  ItemType *itemB = new ItemType( 'B' ); // Component B
  ItemType *itemP = new ItemType( 'P' ); // A finished component, consisting in the simple case of A + B

  // P consists of components A and B.
  ItemType *componentsNeededForP[] = { itemA, itemB, NULL };
  itemP->setComponentsRequired ( componentsNeededForP );
  
  ItemType *nullItem = new ItemType( /* NULL item */ );
  
  // We have a belt with 5 slots
  Belt *belt = new Belt( 5 /* 5 slots, space for three pairs of workers, plus an entry and an exit slot */ );
  
  // Add item factories to the belt, in the simple sim, giving them all the same weighting makes them equally likely to appear.
  // so the chance of say 'A' appearing is 50 / 150 ( weighting / total weighting ).
  belt->addItemFactory ( itemA, 50 /* Probability weighting (from 0 to 100 (most likely)) */ );
  belt->addItemFactory ( itemB, 50 );
  belt->addItemFactory ( nullItem, 50 );

  DEBUG("Probability of component A appearing = %f\n", itemA->getGenerationProbability());
  DEBUG("Probability of component B appearing = %f\n", itemB->getGenerationProbability());
  DEBUG("Probability of no component appearing = %f\n", nullItem->getGenerationProbability());
  
  // Add a finished item type to the belt.
  belt->addFinishedItem ( itemP );
  
  // Add our six workers, in pairs of two per slot. We don't care to model here which side of the belt they stand on,
  // for the simulation in question, it matters not.  The Belt class keeps track of the workers, and will delete them
  // on our behalf once it is itself destroyed.  We instantiate these workers with default parameters and expecting them to be identical.
  
  belt->addWorker( new Worker(), 1 /* position in the line */);
  belt->addWorker( new Worker(), 1 /* position in the line */);

  belt->addWorker( new Worker(), 2 /* position in the line */);
  belt->addWorker( new Worker(), 2 /* position in the line */);

  belt->addWorker( new Worker(), 3 /* position in the line */);
  belt->addWorker( new Worker(), 3 /* position in the line */);
  
  // Setup and run the production line sim
  
  ProductionLine *sim = new ProductionLine();
  
  // the production line class remembers to delete its belt (if present) when destroyed.
  sim->addBelt ( belt );
  
  printf("Running production line for %d steps\n",NUMBER_OF_STEPS);
  sim->runSim( NUMBER_OF_STEPS /* iterations of the conveyor belt */);
  sim->printResults();
  
  delete sim;
  
  return (0); // Tell the shell we were successful.
}


