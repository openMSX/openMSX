// $Id$

#ifndef ALIGNED_HH
#define ALIGNED_HH

#ifndef _MSC_VER
#define ALIGNED(EXPRESSION, ALIGNMENT) EXPRESSION __attribute__((__aligned__((ALIGNMENT))));
#else
#define ALIGNED(EXPRESSION, ALIGNMENT) __declspec (align(ALIGNMENT)) EXPRESSION
#endif

#endif
