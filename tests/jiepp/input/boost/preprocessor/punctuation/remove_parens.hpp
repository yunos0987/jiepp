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
{#ifndef BOOST_PREPROCESSOR_REMOVE_PARENS_HPP}
{#define BOOST_PREPROCESSOR_REMOVE_PARENS_HPP}

{#include <boost/preprocessor/config/config.hpp>}
{#include <boost/preprocessor/control/iif.hpp>}
{#include <boost/preprocessor/facilities/identity.hpp>}
{#include <boost/preprocessor/punctuation/is_begin_parens.hpp>}
{#include <boost/preprocessor/tuple/enum.hpp>}

{#define BOOST_PP_REMOVE_PARENS(param) /*n*/    BOOST_PP_IIF /*n*/      ( /*n*/      BOOST_PP_IS_BEGIN_PARENS(param), /*n*/      BOOST_PP_REMOVE_PARENS_DO, /*n*/      BOOST_PP_IDENTITY /*n*/      ) /*n*/    (param)() /*n*//**/}

{#define BOOST_PP_REMOVE_PARENS_DO(param) /*n*/  BOOST_PP_IDENTITY(BOOST_PP_TUPLE_ENUM(param)) /*n*//**/}

{#endif /* BOOST_PREPROCESSOR_REMOVE_PARENS_HPP */}
