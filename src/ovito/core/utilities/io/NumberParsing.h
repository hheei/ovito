// SPDX-FileCopyrightText: 2026 OVITO GmbH, Germany
// SPDX-License-Identifier: GPL-3.0-only OR MIT

#pragma once


#include <ovito/core/Core.h>

#include <boost/spirit/include/qi.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/version.hpp>

namespace Ovito {

/******************************************************************************
 * Helper function that converts a string to a floating-point number.
 *****************************************************************************/
inline bool parseFloatType(const char* s, const char* s_end, float& f)
{
    const char* s_orig = s; // Make a copy, because parse() modifies its argument.
    if(!boost::spirit::qi::parse(s, s_end, boost::spirit::qi::float_, f)) {
        // Fall back to string stream if Boost.Spirit parser fails (e.g. for very small numbers like 1e-204).
        std::istringstream iss(std::string(s_orig, s_end - s_orig));
        iss.imbue(std::locale::classic());
        double d;
        iss >> d;
        if(!iss) return false;
        f = static_cast<float>(d);
    }
    return true;
}

/******************************************************************************
 * Helper function that converts a string to a floating-point number.
 *****************************************************************************/
inline bool parseFloatType(QLatin1StringView s, float& f)
{
    return parseFloatType(s.data(), s.data() + s.size(), f);
}

/******************************************************************************
 * Helper function that converts a string to a floating-point number.
 *****************************************************************************/
inline bool parseFloatType(std::string_view s, float& f)
{
    return parseFloatType(s.data(), s.data() + s.size(), f);
}

/******************************************************************************
 * Helper function that converts a string to a floating-point number.
 *****************************************************************************/
inline bool parseFloatType(const char* s, const char* s_end, double& f)
{
    const char* s_orig = s; // Make a copy, because parse() modifies its argument.
    if(!boost::spirit::qi::parse(s, s_end, boost::spirit::qi::double_, f)) {
        // Fall back to string stream if Boost.Spirit parser fails (e.g. for very small numbers like 1e-204).
        std::istringstream iss(std::string(s_orig, s_end - s_orig));
        iss.imbue(std::locale::classic());
        iss >> f;
        if(!iss) return false;
    }
    return true;
}

/******************************************************************************
 * Helper function that converts a string to a floating-point number.
 *****************************************************************************/
inline bool parseFloatType(QLatin1StringView s, double& f)
{
    return parseFloatType(s.data(), s.data() + s.size(), f);
}

/******************************************************************************
 * Helper function that converts a string to a floating-point number.
 *****************************************************************************/
inline bool parseFloatType(std::string_view s, double& f)
{
    return parseFloatType(s.data(), s.data() + s.size(), f);
}

/******************************************************************************
 * Helper function that converts a string to an integer number.
 *****************************************************************************/
inline bool parseInt32(const char* s, const char* s_end, int32_t& i)
{
    return boost::spirit::qi::parse(s, s_end, boost::spirit::qi::int_, i);
}

/******************************************************************************
 * Helper function that converts a string to an integer number.
 *****************************************************************************/
inline bool parseInt32(QLatin1StringView s, int32_t& i)
{
    return parseInt32(s.data(), s.data() + s.size(), i);
}

/******************************************************************************
 * Helper function that converts a string to an integer number.
 *****************************************************************************/
inline bool parseInt32(std::string_view s, int32_t& i)
{
    return parseInt32(s.data(), s.data() + s.size(), i);
}

/******************************************************************************
 * Helper function that converts a string to a 64-bit integer number.
 *****************************************************************************/
inline bool parseInt64(const char* s, const char* s_end, int64_t& i)
{
    return boost::spirit::qi::parse(s, s_end, boost::spirit::qi::long_long, i);
}

/******************************************************************************
 * Helper function that converts a string to a 64-bit integer number.
 *****************************************************************************/
inline bool parseInt64(QLatin1StringView s, int64_t& i)
{
    return parseInt64(s.data(), s.data() + s.size(), i);
}

/******************************************************************************
 * Helper function that converts a string to a 64-bit integer number.
 *****************************************************************************/
inline bool parseInt64(std::string_view s, int64_t& i)
{
    return parseInt64(s.data(), s.data() + s.size(), i);
}

/******************************************************************************
 * Helper function that converts a string repr. of a bool ('T' or 'F') to an int
 *****************************************************************************/
inline bool parseBool(const char* s, const char* s_end, int& d)
{
    if(s_end != s + 1) return false;
    if(s[0] == 'T') {
        d = 1;
        return true;
    }
    else if(s[0] == 'F') {
        d = 0;
        return true;
    }
    return false;
}

/******************************************************************************
 * Helper function that converts a string repr. of a bool ('T' or 'F') to an int
 *****************************************************************************/
inline bool parseBool(QLatin1StringView s, int& d)
{
    return parseBool(s.data(), s.data() + s.size(), d);
}

/******************************************************************************
 * Helper function that converts a string repr. of a bool ('T' or 'F') to an int
 *****************************************************************************/
inline bool parseBool(std::string_view s, int& d)
{
    return parseBool(s.data(), s.data() + s.size(), d);
}

}   // End of namespace
