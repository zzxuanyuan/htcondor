#ifndef CONDOR_SYSCALL_MACROS_H
#define CONDOR_SYSCALL_MACROS_H

#define REMAP_ZERO(oldfunc,newfunc,type_return) \
type_return newfunc() \
{ \
return oldfunc(); \
}

#define REMAP_ZERO_VOID(oldfunc,newfunc,type_return) \
type_return newfunc() \
{ \
oldfunc(); \
}

#define REMAP_ONE(oldfunc,newfunc,type_return,type_1) \
type_return newfunc( type_1 arg_1 ) \
{ \
return oldfunc( arg_1 ); \
}

#define REMAP_ONE_VOID(oldfunc,newfunc,type_return,type_1) \
type_return newfunc( type_1 arg_1 ) \
{ \
oldfunc( arg_1 ); \
}

#define REMAP_TWO(oldfunc,newfunc,type_return,type_1,type_2) \
type_return newfunc( type_1 arg_1, type_2 arg_2 ) \
{ \
return oldfunc( arg_1, arg_2 ); \
}

#define REMAP_THREE(oldfunc,newfunc,type_return,type_1,type_2,type_3) \
type_return newfunc( type_1 arg_1, type_2 arg_2, type_3 arg_3 ) \
{ \
return oldfunc( arg_1, arg_2, arg_3 ); \
}

#define REMAP_FOUR(oldfunc,newfunc,type_return,type_1,type_2,type_3,type_4) \
type_return newfunc( type_1 arg_1, type_2 arg_2, type_3 arg_3, type_4 arg_4 ) \
{ \
return oldfunc( arg_1, arg_2, arg_3, arg_4 ); \
}

#define REMAP_FOUR_VOID(oldfunc,newfunc,type_return,type_1,type_2,type_3,type_4) \
type_return newfunc( type_1 arg_1, type_2 arg_2, type_3 arg_3, type_4 arg_4 ) \
{ \
oldfunc( arg_1, arg_2, arg_3, arg_4 ); \
}

#define REMAP_FIVE(oldfunc,newfunc,type_return,type_1,type_2,type_3,type_4,type_5) \
type_return newfunc( type_1 arg_1, type_2 arg_2, type_3 arg_3, type_4 arg_4, type_5 arg_5 ) \
{ \
return oldfunc( arg_1, arg_2, arg_3, arg_4, arg_5 ); \
}

#define REMAP_SIX(oldfunc,newfunc,type_return,type_1,type_2,type_3,type_4,type_5,type_6) \
type_return newfunc( type_1 arg_1, type_2 arg_2, type_3 arg_3, type_4 arg_4, type_5 arg_5, type_6 arg_6 ) \
{ \
return oldfunc( arg_1, arg_2, arg_3, arg_4, arg_5, arg_6 ); \
}

#endif /* CONDOR_SYSCALL_MACROS_H */

