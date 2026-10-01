// # /* **************************************************************************
// #  *                                                                          *
// #  *     (C) Copyright Paul Mensonides 2002-2011.                             *
// #  *     (C) Copyright Edward Diener 2011-2020.                               *
// #  *     Distributed under the Boost Software License, Version 1.0. (See      *
// #  *     accompanying file LICENSE_1_0.txt or copy at                         *
// #  *     http://www.boost.org/LICENSE_1_0.txt)                                *
// #  *                                                                          *
// #  ************************************************************************** */
// #
// # /* See http://www.boost.org for most recent version. */
// #
{# ifndef BOOST_PREPROCESSOR_CONFIG_CONFIG_HPP}
{# define BOOST_PREPROCESSOR_CONFIG_CONFIG_HPP}
// #
// # /* BOOST_PP_CONFIG_FLAGS */
// #
{# define BOOST_PP_CONFIG_STRICT() 16#0001}
{# define BOOST_PP_CONFIG_IDEAL() 16#0002}
// #
{# define BOOST_PP_CONFIG_MSVC() 16#0004}
{# define BOOST_PP_CONFIG_MWCC() 16#0008}
{# define BOOST_PP_CONFIG_BCC() 16#0010}
{# define BOOST_PP_CONFIG_EDG() 16#0020}
{# define BOOST_PP_CONFIG_DMC() 16#0040}
// #
{# ifndef BOOST_PP_CONFIG_FLAGS}
{#    if defined(__GCCXML__) \or\ defined(__WAVE__) \or\ defined(__MWERKS__) \and\ __MWERKS__ >= 16#3200}
{#        define BOOST_PP_CONFIG_FLAGS() (BOOST_PP_CONFIG_STRICT())}
{#    elif defined(__EDG__) \or\ defined(__EDG_VERSION__)}
{#        if defined(_MSC_VER) \and\ \not\defined(__clang__) \and\ (defined(__INTELLISENSE__) \or\ __EDG_VERSION__ >= 308)}
{#           if \not\defined(_MSVC_TRADITIONAL) \or\ _MSVC_TRADITIONAL}
{#               define BOOST_PP_CONFIG_FLAGS() (BOOST_PP_CONFIG_MSVC())}
{#           else}
{#               define BOOST_PP_CONFIG_FLAGS() (BOOST_PP_CONFIG_STRICT())}
{#           endif}
{#        else}
{#            define BOOST_PP_CONFIG_FLAGS() (BOOST_PP_CONFIG_EDG()  or  BOOST_PP_CONFIG_STRICT())}
{#        endif}
{#    elif defined(_MSC_VER) \and\ defined(__clang__)}
{#        define BOOST_PP_CONFIG_FLAGS() (BOOST_PP_CONFIG_STRICT())}
{#    elif defined(__MWERKS__)}
{#        define BOOST_PP_CONFIG_FLAGS() (BOOST_PP_CONFIG_MWCC())}
{#    elif defined(__DMC__)}
{#        define BOOST_PP_CONFIG_FLAGS() (BOOST_PP_CONFIG_DMC())}
{#    elif defined(__BORLANDC__) \and\ __BORLANDC__ >= 16#581}
{#        define BOOST_PP_CONFIG_FLAGS() (BOOST_PP_CONFIG_STRICT())}
{#    elif defined(__BORLANDC__) \or\ defined(__IBMC__) \or\ defined(__IBMCPP__) \or\ defined(__SUNPRO_CC)}
{#        define BOOST_PP_CONFIG_FLAGS() (BOOST_PP_CONFIG_BCC())}
{#    elif defined(_MSC_VER)}
{#        if \not\defined(_MSVC_TRADITIONAL) \or\ _MSVC_TRADITIONAL}
{#           define BOOST_PP_CONFIG_FLAGS() (BOOST_PP_CONFIG_MSVC())}
{#        else}
{#           define BOOST_PP_CONFIG_FLAGS() (BOOST_PP_CONFIG_STRICT())}
{#        endif}
{#    else}
{#        define BOOST_PP_CONFIG_FLAGS() (BOOST_PP_CONFIG_STRICT())}
{#    endif}
{# endif}
// #
// # /* BOOST_PP_CONFIG_EXTENDED_LINE_INFO */
// #
{# ifndef BOOST_PP_CONFIG_EXTENDED_LINE_INFO}
{#    define BOOST_PP_CONFIG_EXTENDED_LINE_INFO 0}
{# endif}
// #
// # /* BOOST_PP_CONFIG_ERRORS */
// #
{# ifndef BOOST_PP_CONFIG_ERRORS}
{#    ifdef NDEBUG}
{#        define BOOST_PP_CONFIG_ERRORS 0}
{#    else}
{#        define BOOST_PP_CONFIG_ERRORS 1}
{#    endif}
{# endif}
// #
// # /* BOOST_PP_VARIADICS */
// #
{# if defined BOOST_PP_VARIADICS}
{#    undef BOOST_PP_VARIADICS}
{# endif}
{# if defined BOOST_PP_VARIADICS_MSVC}
{#    undef BOOST_PP_VARIADICS_MSVC}
{# endif}
{# define BOOST_PP_VARIADICS 1}
{# if defined _MSC_VER \and\ _MSC_VER >= 1400 \and\ \not\defined(__clang__) \and\ (defined(__INTELLISENSE__) \or\ (defined(__INTEL_COMPILER) \and\ __INTEL_COMPILER >= 1700) \or\ \not\(defined __EDG__ \or\ defined __GCCXML__ \or\ defined __PATHSCALE__ \or\ defined __DMC__ \or\ defined __CODEGEARC__ \or\ defined __BORLANDC__ \or\ defined __MWERKS__ \or\ defined __SUNPRO_CC \or\ defined __HP_aCC \or\ defined __MRC__ \or\ defined __SC__ \or\ defined __IBMCPP__ \or\ defined __PGI)) \and\ (\not\defined(_MSVC_TRADITIONAL) \or\ _MSVC_TRADITIONAL)}
{#     define BOOST_PP_VARIADICS_MSVC 1}
{# else}
{#     define BOOST_PP_VARIADICS_MSVC 0}
{# endif}
// #
{# if BOOST_PP_CONFIG_FLAGS() & BOOST_PP_CONFIG_STRICT()}
{# define BOOST_PP_IS_STANDARD() 1}
{# else}
{# define BOOST_PP_IS_STANDARD() 0}
{# endif}
// #
{# endif}
