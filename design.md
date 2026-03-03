# Overview

This document presents the design overview of rmalloc. rmalloc is designed 
to be a fast concurrent memory allocator with low fragmentation, low latency 
and high throughput. rmalloc draws inspiration from mainly mimalloc, jemalloc,
slab allocator and tcmalloc. rmalloc was created by Romario Newell
as an exploration into the memory allocator design space. Romario designed
this allocator with simplicity in mind making it approachable for anyone 
wanting to understand and develop a viable production-quality memory allocator. 

To give a comprehensive overview of rmalloc the core data structures will
be discussed first and then an explanation presented of how memory is requested
and freed.

# Core Data Structures

## Superblock

Superblocks are a central part of rmalloc. A superblock in rmalloc is very
similar to a heap in mimalloc. A superblock is thread local and is only 
accessible by the owning thread. Superblocks are created the first 
time a thread requests memory. The god superblock, a statically
created superblock, is responsible for the creation of other superblocks.

The superblock contains a unique key (superblock key) that identifies it 
and the owning thread. In addition to the superblock key, it also 
houses the pool structure, statistics structure, status of the superblock,
recycling strategy, and other important metadata related to the thread.
The layout of the superblock descriptor is below:

| Superblock Descriptor              |
|------------------------------------|
| sk (superblock key)                |
| caches (pool)                      |   
| status (superblock status)         |
| next (listnode)                    |
| time (last time recycled)          |
| dslabs (total dirty slabs)         |
| reserved (total reserved memory)   |
| dirty (dirty flag)                 |
| rs (recycling strategy)            |
| stat (statistics)                  |

## Extent

An extent is a large contiguous region of memory aligned to a 16MiB address.
Extents can either be 16MiB or a custom size. The size of an extent
depends on the memory request. When both the superblock and recycle bin
are out of allocatable slabs, a new extent is created by the superblock. 
A request size that is <= 64KiB causes the creation of a 16MiB extent while
a request size > 64KiB causes the creation of a custom extent. The size of
a custom extent is always 64KiB plus the amount of memory requested.
The first 64KiB in an extent is always reserved for metadata storage.
The metadata stored at the beginning of an extent is the extent descriptor and
the slab descriptors. In memory an extent is laid out as: 

|  Extent             |
|:-------------------:|
|  Extent descriptor  |
|  Slab descriptor 0  |
|  Slab descriptor 1  |
|          .          |
|          .          |
|          .          |
| Slab descriptor N-1 |
|        Slab 1       |
|          .          |
|          .          |
|          .          |
|       Slab N-1      |

The allocatable region in an extent starts after the first 64KiB i.e.
it starts at slab 1.

Extent Descriptor in memory below:

| Extent Descriptor                         |
|-------------------------------------------|
| base (base address)                       |
| slabs (pointer to slab descriptors array) |
| esize (extent size)                       |
| ssize (slab size)                         |
| sk (superblock key)                       |
| rslabs (number of reserved slabs)         |
| tslabs (total slabs in extent)            |

## Slab

A slab is a contiguous region of memory that stores objects of the same size. 
Slabs and extents share a special relationship; an extent is divided into slabs
as shown by the extent layout picture. The first slab or slab 0 is always
dedicated to storing metadata and the other slabs are used for fulfilling 
memory requests. 

rmalloc uses two types of slab which are normal slabs and large slabs. 
Normal slabs store any object <= 64KiB and large slabs store objects > 64KiB. 
Extents that are 16MiB in size use normal slabs; normal slabs are 
64KiB in size. On the other hand, custom extents use large slabs (large slabs
are the same size as the memory request). The slab descriptor stores important 
metadata regarding a slab and is laid out as such in memory:

| Slab Descriptor                                             	|
|---------------------------------------------------------------|
| aobj (current count of allocated objects)                   	|
| tobj (total objects in slab)                                	|
| robj (total remote objects in remote free list)             	|
| fast (dictates whether to use bump allocation or free list) 	|
| aligned (aligned allocations)                               	|
| base (base address)                                         	|
| osize (object size)                                         	|
| ssize (slab size)                                           	|
| bump (current bump pointer location)                        	|
| local (local free list)                                     	|
| remote (remote free list)                                   	|
| next (listnode)                                             	|
| cache (pointer to current cache)                            	|
| sk (superblock key)                                         	|
| frag (amount of bytes fragmented in the slab)               	|
| ext (pointer to the extent descriptor managing slab)        	|
| cpos (cache index)                                          	|
| dirty (dirty flag)                                          	|
| init (init flag)                                            	|
| mtcl (move to correct list flag)                            	|
| cached (cached flag)                                        	|
| status (slab status)                                          |

## Cache

A cache keeps track of partial and full slabs. Active non-empty normal slabs 
reside in the cache bouncing between the partial and full list. Active empty
normal slabs live on the global list inside the pool data structure. 
When rmalloc is fulfilling a request it maps the request size to a cache 
position and then indexes the appropriate cache descriptor inside the pool 
data structure. The cache descriptor contains a pointer to the hot slab 
descriptor (current slab being used by that cache descriptor); rmalloc uses
the hot slab descriptor to allocate an object. If the hot slab is full or hot slab
descriptor is null, rmalloc checks the partial list or the global list 
for an allocatable slab. If rmalloc succeeds in finding an allocatable slab it
updates the hot slab and allocates the object. Otherwise it runs the 
maintenance procedure which moves empty or partial slabs from the full list to
the correct list. After the completion of the maintenance procedure if any empty
or partial slabs were discovered, rmalloc uses one to fulfill the request. 

rmalloc allows slabs to be on the wrong list for efficiency and simplicity.
Whenever rmalloc undertakes a free operation, it categorizes the free 
operation as either a local free or remote free. If the thread that initiated
free operation is the owning thread of the memory then it is considered a local
free. A remote free is the opposite of a local free where the freeing thread
did not request the memory. During remote frees slabs can't make the appropriate
state transitions because that would take coordination between the freeing 
thread and the owning thread of the memory. If the slab becomes empty or 
partial before a local free happens it then sits on the wrong list until 
the maintenance procedure is run during a memory request.


Cache descriptor layout in memory:

| Cache descriptor                     |
|--------------------------------------|
| index (position)                     |
| osize (object size)                  |
| hot (hot slab pointer)               |
| partial (list of partial slabs)      |
| full (list of full slabs)            |
| pool (pointer to pool structure)     |
| mtcl (number of slabs on wrong list) |

## Pool

The pool data structure is a special data structure which resides inside
the superblock. The pool houses 52 cache descriptors which means rmalloc 
has 52 size classes. These 52 size classes are used to fulfill small or medium
requests. On the other hand, fulfilling large requests involves scanning
the large list inside the pool descriptor to find an allocatable slab; 
rmalloc uses first fit-search to find allocatable slabs on the large list.

Pool descriptor layout in memory below:

| Pool Descriptor                      |
|:------------------------------------:|
|slabs[52]  (cache array)              |
|global (list of empty normal slabs)   |
|large (list of empty large slabs)     |
|lock (mutex to lock the large list)   |


## Recycle Bin

The recycle bin is the area where normal slabs are offloaded by superblocks.
Slabs are offloaded to the recycle bin in two instances. The first instance 
occurs when a superblock reaches a threshold of empty slabs, this causes
release of normal slabs to the recycle bin. The second instance occurs when 
a thread dies, a dying thread abandons its superblock. During abandonment 
the dying thread offloads empty slabs to the recycle bin and non-empty slabs 
are lazily placed in the recycle bin by deallocating threads. 

The main motivation of the recycle bin is to promote reuse of slabs by either
superblocks or the operating system. A superblock that runs out of normal 
allocatable slabs first checks the recycle bin for an allocatable slab and upon
failure it resolves to allocating an extent. A slab in the recycle bin
for an extended duration will eventually be recycled to the operating 
system either synchronously or asynchronously. rmalloc aims to be an excellent
memory allocator and as such it tries hard to release idle memory back 
to the operating system so it can be used by another process.

## Recycle bin descriptor layout:

| Recycle Bin Descriptor                                  |
|---------------------------------------------------------|
| bins[52] (array of slab descriptors)                    |
| global (list of empty slabs)                            |
| frag (total fragmented bytes)                           |
| inuse (total bytes in use)                              |
| capacity (total bytes that can be reused)               |
| tslabs (total slabs)                                    |
| linuse (large bytes in use)                             |
| lactive (number of large active slabs)                  |
| lcapacity (total bytes that can be used for large slabs)|
| caches[52] (each cell contains statistics for each bin) |

 

# Allocation Requests

Whenever memory is requested the first data structure encountered is the 
superblock. If the allocating thread does not have a superblock, rmalloc 
creates a new unique superblock and assigns it to the thread. The allocated
superblock is now owned by that thread for the duration of its lifetime:
future allocations/deallocations from that thread will use that superblock.

Once it is established a thread has a superblock, the next step is to
determine whether the memory request can be satisfied by normal
slabs or large slabs. Normal allocations are treated differently from 
large allocations.

## Normal Allocation Requests

After determining the type of slab needed to satisfy the allocation request,
the next determination is the location of that slab. To determine the location
of the slab the memory request size needs to be mapped to a cache descriptor
position in the pool data structure. Once the correct cache descriptor is 
found rmalloc accesses the hot slab to fulfill the request. A request that 
can not be fulfilled by the hot slab because the hot slab is non-existent
or full, causes a search for a new hot slab. The first place rmalloc looks
for a new hot slab is the partial list in the cache. If the partial list
has an available slab then it is used to fulfill the request. 

Upon failure to find a slab in the partial list, rmalloc checks the global list
in the pool data structure. If an active empty slab is found it is used to
fulfill the request. Failure to find a slab on the global list results in
the recycle bin being checked for a slab. If a slab is found in the recycle bin
it is used to fulfill the request. When no slab can be reused from the recycle 
bin, the maintenance procedure is run to discover empty or partial slabs on the
full list. If the maintenance procedure discovers an allocatable slab, 
one of the discovered slabs is used to fulfill the request. The last option
is to allocate a new extent if no allocatable slab has been found. 
A slab from the new extent is then used to fulfill the request.


## Large Requests

Large slabs are located on the large list inside the pool data structure. 
Instead of mapping the request size to a cache position the request size
is used to perform a first-fit search of the large list. If a large slab 
exists that can fulfill the request then the slab descriptor is unlinked
from the large list and the corresponding slab is used to fulfill the request. 

On the other hand, if no large slab exists that can fulfill the request then
a new extent of a custom size is allocated. A slab from the new extent is 
used to fulfil the request.



# Free requests

rmalloc starts every free operation with a pointer to the object that needs 
freeing and the superblock key of the freeing thread. The pointer is used to 
locate the slab descriptor in O(1) time using bit manipulation and alignment 
tricks. All extents are aligned on a 16MiB address, this fact makes 
it trivial to locate the start of an extent using bit masking. Additionally, 
the first 64KiB is always reserved to store metadata which means the slab 
descriptor is always in the first 64KiB. Using more bit manipulation tricks 
again the slab descriptor can easily be found. 

After the correct slab descriptor is found, the superblock key in 
the slab descriptor is compared to the superblock key of the freeing 
thread. If both keys are equal that means rmalloc is performing a local
free otherwise remote free is being performed. Local frees and remote 
frees are very similar except for the list that the object is stored on.
In a local free the object is stored on the local free list while a 
remote free stores the object on the remote free list. One other key 
difference is that local frees can move a slab from the full to the partial or
global list or partial to the global list while remote frees set a flag 
on the slab that indicates it needs to be transferred to the correct list
on the next local free operation or during the maintenance procedure. 
When rmalloc places the object on the correct list and possibly transferring
the slab to the correct list that concludes the freeing operation.
