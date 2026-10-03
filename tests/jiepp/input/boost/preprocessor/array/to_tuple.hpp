// # /* **************************************************************************
// #  *                                                                          *
// #  *     (C) Copyright Edward Diener 2011.                                    *
// #  *     (C) Copyright Paul Mensonides 2011.                                  *
// #  *     Distributed under the Boost Software License, Version 1.0. (See      *
// #  *     accompanying file LICENSE_1_0.txt or copy at                         *
// #  *     http://www.boost.org/LICENSE_1_0.txt)                                *
// #  *                                                                          *
// #  ************************************************************************** */
// #
// # /* See http://www.boost.org for most recent version. */
// #
{# ifndef BOOST_PREPROCESSOR_ARRAY_TO_TUPLE_HPP}
{# define BOOST_PREPROCESSOR_ARRAY_TO_TUPLE_HPP}
// #
{# include <boost/preprocessor/array/data.hpp>}
{# include <boost/preprocessor/array/size.hpp>}
{# include <boost/preprocessor/control/if.hpp>}
// #
// # /* BOOST_PP_ARRAY_TO_TUPLE */
// #
{#    define BOOST_PP_ARRAY_TO_TUPLE(array) /*n*/        BOOST_PP_IF /*n*/            ( /*n*/            BOOST_PP_ARRAY_SIZE(array), /*n*/            BOOST_PP_ARRAY_DATA, /*n*/            BOOST_PP_ARRAY_TO_TUPLE_EMPTY /*n*/            ) /*n*/        (array) /*n*//**/}
{#    define BOOST_PP_ARRAY_TO_TUPLE_EMPTY(array)}
// #
{# endif}
