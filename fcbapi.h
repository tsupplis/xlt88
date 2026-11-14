#ifndef fcbapi_h
#define fcbapi_h

/* Source BDOS operations supported by the native DOS FCB lowering contract.
 * Analysis and lowering must use the same set; this says nothing about
 * whether a particular source object's representation has been validated. */
static inline int
fcb_function(long function)
{
	return function == 15 || function == 16 || function == 20 ||
	    function == 21 || function == 22 ||
	    (function >= 33 && function <= 36);
}

#endif
