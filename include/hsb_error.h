
#ifndef _HSB_ERROR_H_
#define _HSB_ERROR_H_

typedef enum {
	HSB_E_OK = 0,
	HSB_E_BAD_PARAMETERS,	/* 1 */
	HSB_E_NO_MEMORY,	/* 2 */
	HSB_E_NOT_SUPPORTED,	/* 3 */
	HSB_E_ENTRY_EXISTS,	/* 4 */
	HSB_E_NULL_PTR,		/* 5 */
	HSB_E_OTHERS,		/* 6 */
	HSB_E_LAST,
} HSB_ERROR_NO_T;

#endif

