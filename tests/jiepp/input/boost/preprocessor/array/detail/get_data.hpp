// # /* **************************************************************************
// #  *                                                                          *
// #  *     (C) Copyright Edward Diener 2014.                                    *
// #  *     Distributed under the Boost Software License, Version 1.0. (See      *
// #  *     accompanying file LICENSE_1_0.txt or copy at                         *
// #  *     http://www.boost.org/LICENSE_1_0.txt)                                *
// #  *                                                                          *
// #  ************************************************************************** */
// #
// # /* See http://www.boost.org for most recent version. */
// #
{# ifndef BOOST_PREPROCESSOR_ARRAY_DETAIL_GET_DATA_HPP}
{# define BOOST_PREPROCESSOR_ARRAY_DETAIL_GET_DATA_HPP}
// #
{# include <boost/preprocessor/config/config.hpp>}
{# include <boost/preprocessor/tuple/rem.hpp>}
{# include <boost/preprocessor/control/if.hpp>}
{# include <boost/preprocessor/control/iif.hpp>}
{# include <boost/preprocessor/facilities/is_1.hpp>}
// #
// # /* BOOST_PP_ARRAY_DETAIL_GET_DATA */
// #
{# define BOOST_PP_ARRAY_DETAIL_GET_DATA_NONE(size, data)}

{# if \not\(BOOST_PP_VARIADICS_MSVC \and\ _MSC_VER <= 1400)}
{#    if BOOST_PP_VARIADICS_MSVC}
{#       define BOOST_PP_ARRAY_DETAIL_GET_DATA_ANY_VC_DEFAULT(size, data) BOOST_PP_TUPLE_REM(size) data}
{#       define BOOST_PP_ARRAY_DETAIL_GET_DATA_ANY_VC_CAT(size, data) BOOST_PP_TUPLE_REM_CAT(size) data}
{#       define BOOST_PP_ARRAY_DETAIL_GET_DATA_ANY(size, data) /*n*/            BOOST_PP_IIF /*n*/                ( /*n*/                BOOST_PP_IS_1(size), /*n*/                BOOST_PP_ARRAY_DETAIL_GET_DATA_ANY_VC_CAT, /*n*/                BOOST_PP_ARRAY_DETAIL_GET_DATA_ANY_VC_DEFAULT /*n*/                ) /*n*/            (size,data) /*n*//**/}
{#    else}
{#       define BOOST_PP_ARRAY_DETAIL_GET_DATA_ANY(size, data) BOOST_PP_TUPLE_REM(size) data}
{#    endif}
{# else}
{#    define BOOST_PP_ARRAY_DETAIL_GET_DATA_ANY(size, data) BOOST_PP_TUPLE_REM(size) data}
{# endif}

{# define BOOST_PP_ARRAY_DETAIL_GET_DATA(size, data) /*n*/    BOOST_PP_IF /*n*/        ( /*n*/        size, /*n*/        BOOST_PP_ARRAY_DETAIL_GET_DATA_ANY, /*n*/        BOOST_PP_ARRAY_DETAIL_GET_DATA_NONE /*n*/        ) /*n*/    (size,data) /*n*//**/}
// #
{# endif /* BOOST_PREPROCESSOR_ARRAY_DETAIL_GET_DATA_HPP */}
