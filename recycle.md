Every good allocator needs a good recycling strategy to recycle memory.
Memory can be recycled for reuse within the allocator or be recycled 
to the operating system for reuse with a different process that needs
memory. rmalloc does both and it can acheive both synchronously or
asynchronously.

Slabs that move from partially allocated to empty are marked locally
as dirty by the owning the superblock. Any slab that is marked as dirty
is up for recycling. Whenever rmalloc reaches a threshold of 256 slabs
it sets the dirty flag on the superblock indicating the superblock 
needs transfer the dirty slabs to the recycle bin. The transferral is 
done locally as well.

Now, when dirty slabs are in the recycle bin they can either be reused 
by other superblocks or the backing memory is returned to the operating 
system. rmalloc returns memory to the operating system either in a 
cooperative synchronous fashion or asynchronously. The default option
is to return memory asynchronously using a background thread. Doing it 
asynchronously frees the freeing thread from returning memory to the 
OS. If for some reason rmalloc failed to create the background 
thread for recycling then it frees memory in a cooperative synchronous 
manner.

Freeing memory in a cooperative synchronous manner means each allocating
thread returns some memory to the operating system. At some interval
each thread will return memory from the recycle bin to the operating 
system, this helps spreads the work between all allocating thread. 