
// ARM production line coding challenge

// Conveyor belt modelled as a belt, so a circular buffer in C, to ensure only the start
// and end pointers need move around

// Notes:
// - fetch randomness via a helper function to allow different sources in the future
// - start pointer advances "left" around the ring, and deposits a new item on the belt
// - workers are then selected in random order, advanced one slot, and tested to see if they
//	 can act upon the new slot, this allows for the possibility of them being able to access multiple
//   slots in future.
// - the end pointer then collects whatever is coming off the end of the belt and records it.

// Data storage:
// - The belt is represented by a C++ class, using opaque handles for the items, which would allow the items
//   to be of any size and disposition and stored in any manner underneath
// - The workers are separate instances of a Worker class, they call out for their needs to functions for
//   randomness and also handlers to store and assemble a list of item types, returning the time taken, if relevant.
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

// GK: For portability, use internal type names and map them here
typedef u_int64_t u64int;
typedef u_int32_t u32int;
typedef unsigned char ascii;

class Worker  {
	ascii *name;
};

class Item {
	u64int	id;
};

class Belt {
	u32int	length;
};



