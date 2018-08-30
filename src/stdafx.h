#ifndef INCL_ROYALE_STDAFX_H
#define INCL_ROYALE_STDAFX_H

#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <utility>

#define BOOST_COROUTINES_NO_DEPRECATION_WARNING

#include <boost/variant.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/type_index.hpp>

#include <boost/preprocessor/variadic/to_seq.hpp>
#include <boost/preprocessor/seq/variadic_seq_to_seq.hpp>
#include <boost/preprocessor/punctuation/comma.hpp>
#include <boost/preprocessor/facilities/overload.hpp>
#include <boost/preprocessor/seq/for_each.hpp>

#include "nlohmann/json.hpp"

#define SPDLOG_DISABLE_TID_CACHING
#define SPDLOG_NO_THREAD_ID
#include "spdlog/fmt/ostr.h"
#include "spdlog/spdlog.h"

#define ROYALE_LOG_MIN 0

#ifdef SPDLOG_TRACE_ON
#define ROYALE_LOG_MAX 6
#else
#ifdef SPDLOG_DEBUG_ON
#define ROYALE_LOG_MAX 5
#else
#define ROYALE_LOG_MAX 4
#endif
#endif


#endif
