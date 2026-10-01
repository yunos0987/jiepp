// # /* **************************************************************************
// #  *                                                                          *
// #  *     (C) Copyright Edward Diener 2014.
// #  *     Distributed under the Boost Software License, Version 1.0. (See
// #  *     accompanying file LICENSE_1_0.txt or copy at
// #  *     http://www.boost.org/LICENSE_1_0.txt)
// #  *                                                                          *
// #  ************************************************************************** */
// #
// # /* See http://www.boost.org for most recent version. */
// #
{# ifndef BOOST_PREPROCESSOR_IS_BEGIN_PARENS_HPP}
{# define BOOST_PREPROCESSOR_IS_BEGIN_PARENS_HPP}

{#include <boost/preprocessor/config/config.hpp>}
{#include <boost/preprocessor/punctuation/detail/is_begin_parens.hpp>}

{#if BOOST_PP_VARIADICS_MSVC \and\ _MSC_VER <= 1400}

{#define BOOST_PP_IS_BEGIN_PARENS(param) /*n*/    BOOST_PP_DETAIL_IBP_SPLIT /*n*/      ( /*n*/      0, /*n*/      BOOST_PP_DETAIL_IBP_CAT /*n*/        ( /*n*/        BOOST_PP_DETAIL_IBP_IS_VARIADIC_R_, /*n*/        BOOST_PP_DETAIL_IBP_IS_VARIADIC_C param /*n*/        ) /*n*/      ) /*n*//**/}

{#else}

{#define BOOST_PP_IS_BEGIN_PARENS(...) /*n*/    BOOST_PP_DETAIL_IBP_SPLIT /*n*/      ( /*n*/      0, /*n*/      BOOST_PP_DETAIL_IBP_CAT /*n*/        ( /*n*/        BOOST_PP_DETAIL_IBP_IS_VARIADIC_R_, /*n*/        BOOST_PP_DETAIL_IBP_IS_VARIADIC_C __VA_ARGS__ /*n*/        ) /*n*/      ) /*n*//**/}

{#endif /* BOOST_PP_VARIADICS_MSVC \and\ _MSC_VER <= 1400 */}
{#endif /* BOOST_PREPROCESSOR_IS_BEGIN_PARENS_HPP */}
