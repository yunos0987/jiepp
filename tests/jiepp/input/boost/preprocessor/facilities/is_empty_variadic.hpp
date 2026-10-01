// # /* **************************************************************************
// #  *                                                                          *
// #  *     (C) Copyright Edward Diener 2014,2019.
// #  *     Distributed under the Boost Software License, Version 1.0. (See
// #  *     accompanying file LICENSE_1_0.txt or copy at
// #  *     http://www.boost.org/LICENSE_1_0.txt)
// #  *                                                                          *
// #  ************************************************************************** */
// #
// # /* See http://www.boost.org for most recent version. */
// #
{# ifndef BOOST_PREPROCESSOR_FACILITIES_IS_EMPTY_VARIADIC_HPP}
{# define BOOST_PREPROCESSOR_FACILITIES_IS_EMPTY_VARIADIC_HPP}
// #
{# include <boost/preprocessor/config/config.hpp>}
{# include <boost/preprocessor/punctuation/is_begin_parens.hpp>}
{# include <boost/preprocessor/facilities/detail/is_empty.hpp>}
// #
{#if BOOST_PP_VARIADICS_MSVC \and\ _MSC_VER <= 1400}
// #
{#define BOOST_PP_IS_EMPTY(param) /*n*/    BOOST_PP_DETAIL_IS_EMPTY_IIF /*n*/      ( /*n*/      BOOST_PP_IS_BEGIN_PARENS /*n*/        ( /*n*/        param /*n*/        ) /*n*/      ) /*n*/      ( /*n*/      BOOST_PP_IS_EMPTY_ZERO, /*n*/      BOOST_PP_DETAIL_IS_EMPTY_PROCESS /*n*/      ) /*n*/    (param) /*n*//**/}
{#define BOOST_PP_IS_EMPTY_ZERO(param) 0}
{# else}
{# if defined(__cplusplus) \and\ __cplusplus > 201703}
{# include <boost/preprocessor/variadic/has_opt.hpp>}
{#define BOOST_PP_IS_EMPTY(...) /*n*/    BOOST_PP_DETAIL_IS_EMPTY_IIF /*n*/      ( /*n*/      BOOST_PP_VARIADIC_HAS_OPT() /*n*/      ) /*n*/      ( /*n*/      BOOST_PP_IS_EMPTY_OPT, /*n*/      BOOST_PP_IS_EMPTY_NO_OPT /*n*/      ) /*n*/    (__VA_ARGS__) /*n*//**/}
{#define BOOST_PP_IS_EMPTY_FUNCTION2(...) /*n*/    __VA_OPT__(0,) 1 /*n*//**/}
{#define BOOST_PP_IS_EMPTY_FUNCTION(...) /*n*/    BOOST_PP_IS_EMPTY_FUNCTION2(__VA_ARGS__) /*n*//**/}
{#define BOOST_PP_IS_EMPTY_OPT(...) /*n*/    BOOST_PP_VARIADIC_HAS_OPT_ELEM0(BOOST_PP_IS_EMPTY_FUNCTION(__VA_ARGS__),) /*n*//**/}
{# else}
{#define BOOST_PP_IS_EMPTY(...) /*n*/    BOOST_PP_IS_EMPTY_NO_OPT(__VA_ARGS__) /*n*//**/}
{# endif /* defined(__cplusplus) \and\ __cplusplus > 201703 */}
{#define BOOST_PP_IS_EMPTY_NO_OPT(...) /*n*/    BOOST_PP_DETAIL_IS_EMPTY_IIF /*n*/      ( /*n*/      BOOST_PP_IS_BEGIN_PARENS /*n*/        ( /*n*/        __VA_ARGS__ /*n*/        ) /*n*/      ) /*n*/      ( /*n*/      BOOST_PP_IS_EMPTY_ZERO, /*n*/      BOOST_PP_DETAIL_IS_EMPTY_PROCESS /*n*/      ) /*n*/    (__VA_ARGS__) /*n*//**/}
{#define BOOST_PP_IS_EMPTY_ZERO(...) 0}
{# endif /* BOOST_PP_VARIADICS_MSVC \and\ _MSC_VER <= 1400 */}
{# endif /* BOOST_PREPROCESSOR_FACILITIES_IS_EMPTY_VARIADIC_HPP */}
