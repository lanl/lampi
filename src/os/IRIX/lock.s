
#include <asm.h>
#include <regdef.h>
/*
 A spin lock function using the LL/SC mechanism. This
 is a test-and-test-and-set lock.

 We assume the lock is a longword, with the address passed
 via a0. We assume that the 'winning content' of the spin
 lock is passed in a1. This winning content must not be  zero.

*/
	/*move	t2, 1*/		/* get second param, the "winning content"  */
	

	LEAF(spinlock)
$448:
	INT_LL	t1, 0(a0)	/* load counter from param 1 */
	dli     t2, 1
	.set noreorder
	bne	t1, 0, $451
	nop
	.set reorder
	INT_SC	t2, 0(a0)
	.set noreorder
	beq	t2, 0, $451	/* SCD will set T2 to zero if conflict */
	nop
	jr	ra
	nop
	.set reorder
	
$451:	INT_LL	t0, 0(a0)
	.set noreorder
	beq	t0, 0, $448
	nop
	jr	$451
	nop
	.set reorder

	END(spinlock)

		
/*
 as above, but don't loop waiting for lock to be free.
 return ZERO if lock is not acquires, ONE if it is acquired
*/ 

	LEAF(spintrylock)
    move    t3, zero
$490:
	INT_LL	t1, 0(a0)
	dli     t2, 1
	.set noreorder
	bne	t1, 0, $500
	nop
	.set reorder
	INT_SC	t2, 0(a0)
	.set noreorder
	beq	t2, 0, $500	/* SCD will set T2 to zero if conflict */
	or	v0, zero, 1	/* success, return 1 */
	jr	ra
	nop
	.set reorder

$500:	move	v0, zero		/* failure, return 0  */
    INT_ADD t3, t3, 1
	.set noreorder
    ble     t3, 5, $490
    nop
	jr	ra
	nop
	.set reorder

	END(spintrylock)

	/* atomicly get the old content of the first argument a0
	   decrement the content
	*/

	LEAF(fetchNadd)
$600:	INT_LL t1, 0(a0)	# load counter
	move v0, t1		# get original value; 
	INT_ADD t2, t1, a1	# t2 <= t1+param2
	INT_SC t2, 0(a0)	# try to store, checking for atomicity
	.set noreorder	
	beq t2, 0, $600		# if not atomic (i.e, !=0), try again
	nop
	jr ra
	nop
	.set reorder
	END(fetchNadd)


	/* atomicly get the old content of the first argument a0
	   and increment the count - long int assumed.
	*/

	LEAF(fetchNaddLong)
$700:	LONG_LL t1, 0(a0)	# load counter
	move v0, t1		# get original value; 
	LONG_ADD t2, t1, a1	# t2 <= t1+param2
	LONG_SC t2, 0(a0)	# try to store, checking for atomicity
	.set noreorder	
	beq t2, 0, $700		# if not atomic (i.e, !=0), try again
	nop
	jr ra
	nop
	.set reorder
	END(fetchNaddLong)

	LEAF(fetchNset)
$800:	INT_LL t1, 0(a0)	# load counter
	move v0, t1		# get original value; 
	move t2, a1		# t2 <= param2
	INT_SC t2, 0(a0)	# try to store, checking for atomicity
	.set noreorder	
	beq t2, 0, $800		# if not atomic (i.e, !=0), try again
	nop
	jr ra
	nop
	.set reorder
	END(fetchNset)

	LEAF(fetchNsetLong)
$900:	LONG_LL t1, 0(a0)	# load counter
	move v0, t1		# get original value; 
	move t2, a1		# t2 <= param2
	LONG_SC t2, 0(a0)	# try to store, checking for atomicity
	.set noreorder	
	beq t2, 0, $900		# if not atomic (i.e, !=0), try again
	nop
	jr ra
	nop
	.set reorder
	END(fetchNsetLong)
