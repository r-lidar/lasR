#ifndef MACROS_H
#define MACROS_H

#define EPSILON 2e-8

#define ROUNDANY(x,m) round((x) / m) * m

#ifndef MAX
#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#define MAX(a,b) (((a)>(b)) ? (a) : (b))
#endif

#ifndef MAX3
#define MAX3(a,b,c) (((a)>(b)) ? (((a)>(c)) ? (a) : (c)) : (((b)>(c)) ? (b) : (c)));
#define MIN3(a,b,c) (((a)>(b)) ? (((b)>(c)) ? (c) : (b)) : (((a)>(c)) ? (c) : (a)));
#endif

#define INFD std::numeric_limits<double>::infinity();

#define ASSERT_VALID_POINTER(x) if (!is_valid_pointer(x)) return false;

#endif