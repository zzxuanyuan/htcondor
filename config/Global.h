/*************
**
** Global macros
**
*************/
#define YES	1
#define NO	0

/* Concat - concatenates two strings.  */
#ifndef Concat
#if (__STDC__ && !defined(UnixCpp)) || defined(AnsiCpp)
#define Concat(a,b)a##b
#else
#define Concat(a,b)a/**/b
#endif
#endif

/* Concat3 - concatenates three strings.  */
#ifndef Concat3
#if (__STDC__ && !defined(UnixCpp)) || defined(AnsiCpp)
#define Concat3(a,b,c)a##b##c
#else
#define Concat3(a,b,c)a/**/b/**/c
#endif
#endif

/* Concat4 - concatenates four strings.  */
#ifndef Concat4
#if (__STDC__ && !defined(UnixCpp)) || defined(AnsiCpp)
#define Concat4(a,b,c,d)a##b##c##d
#else
#define Concat4(a,b,c,d)a/**/b/**/c/**/d
#endif
#endif

/* Concat5 - concatenates five strings.  */
#ifndef Concat5
#if (__STDC__ && !defined(UnixCpp)) || defined(AnsiCpp)
#define Concat5(a,b,c,d,e)a##b##c##d##e
#else
#define Concat5(a,b,c,d,e)a/**/b/**/c/**/d/**/e
#endif
#endif

/* Concat6 - concatenates six strings.  */
#ifndef Concat6
#if (__STDC__ && !defined(UnixCpp)) || defined(AnsiCpp)
#define Concat6(a,b,c,d,e,f)a##b##c##d##e##f
#else
#define Concat6(a,b,c,d,e,f)a/**/b/**/c/**/d/**/e/**/f
#endif
#endif



/***************************************************************************
** Undefine any symbols the compiler might have defined for us since
** they may trip us up later.
***************************************************************************/
#undef sun
#undef sparc
#undef i386		
#undef i686
#undef hp9000s800
#undef __alpha
#undef sgi
#undef alpha
#undef hpux
#undef linux
#undef mips
#undef aix
#undef ia64
