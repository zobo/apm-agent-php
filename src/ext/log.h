/*
 * Licensed to Elasticsearch B.V. under one or more contributor
 * license agreements. See the NOTICE file distributed with
 * this work for additional information regarding copyright
 * ownership. Elasticsearch B.V. licenses this file to you under
 * the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#pragma once

#include "LogLevel.h"
#include <stdbool.h>
#include <stdarg.h>
#ifndef PHP_WIN32
#   include <syslog.h>
#endif
#include "ResultCode.h"
#include "basic_types.h"
#include "basic_macros.h" // ELASTIC_APM_PRINTF_ATTRIBUTE
#include "TextOutputStream.h"
#include "platform.h"

extern String logLevelNames[ numberOfLogLevels ];

enum LogSinkType
{
    logSink_stderr,

    #ifndef PHP_WIN32
    logSink_syslog,
    #endif

    #ifdef PHP_WIN32
    logSink_winSysDebug,
    #endif

    logSink_file,

    numberOfLogSinkTypes
};
typedef enum LogSinkType LogSinkType;

extern String logSinkTypeName[ numberOfLogSinkTypes ];
extern LogLevel defaultLogLevelPerSinkType[ numberOfLogSinkTypes ];

#define ELASTIC_APM_FOR_EACH_LOG_SINK_TYPE( logSinkTypeVar ) ELASTIC_APM_FOR_EACH_INDEX_EX( LogSinkType, logSinkTypeVar, numberOfLogSinkTypes )

struct LoggerConfig
{
    LogLevel levelPerSinkType[ numberOfLogSinkTypes ];
    String file;
};
typedef struct LoggerConfig LoggerConfig;

enum { maxLoggerReentrancyDepth = 2 };

struct Logger
{
    LoggerConfig config;
    char* messageBuffer;
    char* auxMessageBuffer;
    LogLevel maxEnabledLevel;
    UInt8 reentrancyDepth;
    bool fileFailed;
};
typedef struct Logger Logger;

ResultCode constructLogger( Logger* logger );
ResultCode reconfigureLogger( Logger* logger, const LoggerConfig* newConfig, LogLevel generalLevel );
void destructLogger( Logger* logger );

void logWithLogger(
        Logger* logger /* <- argument #1 */
        , bool isForced
        , LogLevel statementLevel
        , StringView category
        , StringView filePath
        , UInt lineNumber
        , StringView funcName
        , String msgPrintfFmt /* <- printf format is argument #8 */
        , ...                /* <- arguments for printf format placeholders start from argument #9 */
) ELASTIC_APM_PRINTF_ATTRIBUTE( /* printfFmtPos: */ 8, /* printfFmtArgsPos: */ 9 );

void vLogWithLogger(
        Logger* logger
        , bool isForced
        , LogLevel statementLevel
        , StringView category
        , StringView filePath
        , UInt lineNumber
        , StringView funcName
        , String msgPrintfFmt
        , va_list msgPrintfFmtArgs
);

LogLevel calcMaxEnabledLogLevel( LogLevel levelPerSinkType[ numberOfLogSinkTypes ] );

Logger* getGlobalLogger();

bool isInLogContext();

const char* logLevelToName( LogLevel level );

#define ELASTIC_APM_LOG_LINE_PREFIX_TRACER_PART "[Elastic APM PHP Tracer]"

#ifdef PHP_WIN32

#   define ELASTIC_APM_LOG_TO_BACKGROUND_SINK( statementLevel, fmt, ... )

#else // #ifdef PHP_WIN32

    int logLevelToSyslog( LogLevel level );

#   define ELASTIC_APM_LOG_WRITE_TO_SYSLOG( statementLevel, fmt, ... ) \
        syslog( \
            logLevelToSyslog( statementLevel ) \
            , ELASTIC_APM_LOG_LINE_PREFIX_TRACER_PART \
            " [PID: %d]" \
            " [TID: %d]" \
            " [%s] " \
            fmt \
            , (int)(getCurrentProcessId()) \
            , (int)(getCurrentThreadId()) \
            , logLevelToName( statementLevel ) \
            , ##__VA_ARGS__ )

extern LogLevel g_elasticApmDirectLogLevelSyslog;

#   define ELASTIC_APM_LOG_TO_BACKGROUND_SINK( statementLevel, fmt, ... ) \
        do { \
            if ( g_elasticApmDirectLogLevelSyslog >= (statementLevel) ) \
            { \
                ELASTIC_APM_LOG_WRITE_TO_SYSLOG( statementLevel, fmt, ##__VA_ARGS__ ); \
            } \
        } while ( 0 )

#endif // #ifdef PHP_WIN32

#define ELASTIC_APM_LOG_WRITE_TO_STDERR( statementLevel, fmt, ... ) \
    do { \
        fprintf( stderr \
            , ELASTIC_APM_LOG_LINE_PREFIX_TRACER_PART \
            " [PID: %d]" \
            " [TID: %d]" \
            " [%s] " \
            fmt "\n" \
            , (int)(getCurrentProcessId()) \
            , (int)(getCurrentThreadId()) \
            , logLevelToName( statementLevel ) \
            , ##__VA_ARGS__ ); \
        fflush( stderr ); \
    } while ( 0 )

extern LogLevel g_elasticApmDirectLogLevelStderr;

#define ELASTIC_APM_LOG_DIRECT( statementLevel, fmt, ... ) \
    do { \
        ELASTIC_APM_LOG_TO_BACKGROUND_SINK( (statementLevel), fmt, ##__VA_ARGS__ ); \
        if ( g_elasticApmDirectLogLevelStderr >= (statementLevel) ) \
        { \
            ELASTIC_APM_LOG_WRITE_TO_STDERR( (statementLevel), fmt, ##__VA_ARGS__ ); \
        } \
    } while ( 0 )

#define ELASTIC_APM_LOG_WITH_LEVEL( statementLevel, fmt, ... ) \
    do { \
        Logger* const globalStateLogger = getGlobalLogger(); \
        if ( globalStateLogger->maxEnabledLevel >= (statementLevel) ) \
        { \
            if ( isInLogContext() ) \
            { \
                ELASTIC_APM_LOG_DIRECT( statementLevel, fmt, ##__VA_ARGS__ ); \
            } \
            else \
            { \
                logWithLogger( \
                    globalStateLogger, \
                    /* isForced: */ false, \
                    (statementLevel), \
                    ELASTIC_APM_STRING_LITERAL_TO_VIEW( ELASTIC_APM_CURRENT_LOG_CATEGORY ), \
                    ELASTIC_APM_STRING_LITERAL_TO_VIEW( __FILE__ ), \
                    __LINE__, \
                    ELASTIC_APM_STRING_LITERAL_TO_VIEW( __FUNCTION__ ), \
                    (fmt) , ##__VA_ARGS__ ); \
            } \
        } \
    } while ( 0 )

#define ELASTIC_APM_LOG_CRITICAL( fmt, ... ) ELASTIC_APM_LOG_WITH_LEVEL( logLevel_critical, fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_LOG_ERROR( fmt, ... ) ELASTIC_APM_LOG_WITH_LEVEL( logLevel_error, fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_LOG_WARNING( fmt, ... ) ELASTIC_APM_LOG_WITH_LEVEL( logLevel_warning, fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_LOG_INFO( fmt, ... ) ELASTIC_APM_LOG_WITH_LEVEL( logLevel_info, fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_LOG_DEBUG( fmt, ... ) ELASTIC_APM_LOG_WITH_LEVEL( logLevel_debug, fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_LOG_TRACE( fmt, ... ) ELASTIC_APM_LOG_WITH_LEVEL( logLevel_trace, fmt, ##__VA_ARGS__ )

#define ELASTIC_APM_LOG_CONDITIONAL_CTX_SEPARATOR( separator, fmt ) \
    ( ELASTIC_APM_STATIC_ARRAY_SIZE( fmt ) > 1 ? (separator) : "" )

#define ELASTIC_APM_LOG_FUNCTION_PREFIX_MSG_WITH_LEVEL( statementLevel, prefix, fmt, ... ) \
    ELASTIC_APM_LOG_WITH_LEVEL( statementLevel, "%s%s" fmt, prefix, ELASTIC_APM_LOG_CONDITIONAL_CTX_SEPARATOR( "; ", fmt ), ##__VA_ARGS__ )

#define ELASTIC_APM_LOG_FUNCTION_ENTRY_MSG_WITH_LEVEL( statementLevel, fmt, ... ) \
    ELASTIC_APM_LOG_FUNCTION_PREFIX_MSG_WITH_LEVEL( statementLevel, "Entered", fmt, ##__VA_ARGS__ )

#define ELASTIC_APM_LOG_DEBUG_FUNCTION_ENTRY_MSG( fmt, ... ) ELASTIC_APM_LOG_FUNCTION_ENTRY_MSG_WITH_LEVEL( logLevel_debug, fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_LOG_TRACE_FUNCTION_ENTRY_MSG( fmt, ... ) ELASTIC_APM_LOG_FUNCTION_ENTRY_MSG_WITH_LEVEL( logLevel_trace, fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_LOG_DEBUG_FUNCTION_ENTRY() ELASTIC_APM_LOG_DEBUG_FUNCTION_ENTRY_MSG( "" )
#define ELASTIC_APM_LOG_TRACE_FUNCTION_ENTRY() ELASTIC_APM_LOG_TRACE_FUNCTION_ENTRY_MSG( "" )

#define ELASTIC_APM_LOG_FUNCTION_EXIT_MSG_WITH_LEVEL( statementLevel, fmt, ... ) \
    ELASTIC_APM_LOG_FUNCTION_PREFIX_MSG_WITH_LEVEL( statementLevel, "Exiting...", fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_LOG_DEBUG_FUNCTION_EXIT_MSG( fmt, ... ) ELASTIC_APM_LOG_FUNCTION_EXIT_MSG_WITH_LEVEL( logLevel_debug, fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_LOG_TRACE_FUNCTION_EXIT_MSG( fmt, ... ) ELASTIC_APM_LOG_FUNCTION_EXIT_MSG_WITH_LEVEL( logLevel_trace, fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_LOG_DEBUG_FUNCTION_EXIT() ELASTIC_APM_LOG_DEBUG_FUNCTION_EXIT_MSG( "" )
#define ELASTIC_APM_LOG_TRACE_FUNCTION_EXIT() ELASTIC_APM_LOG_TRACE_FUNCTION_EXIT_MSG( "" )


#define ELASTIC_APM_LOG_RESULT_CODE_FUNCTION_EXIT_MSG_WITH_LEVEL( successLevel, fmt, ... ) \
    ELASTIC_APM_LOG_FUNCTION_EXIT_MSG_WITH_LEVEL \
    ( \
        resultCode == resultSuccess ? (successLevel) : logLevel_error \
        , "resultCode: %s (%d); " fmt \
        , resultCodeToString( resultCode ), resultCode , ##__VA_ARGS__ \
    )

#define ELASTIC_APM_LOG_DEBUG_RESULT_CODE_FUNCTION_EXIT_MSG( fmt, ... ) ELASTIC_APM_LOG_RESULT_CODE_FUNCTION_EXIT_MSG_WITH_LEVEL( logLevel_debug, fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_LOG_TRACE_RESULT_CODE_FUNCTION_EXIT_MSG( fmt, ... ) ELASTIC_APM_LOG_RESULT_CODE_FUNCTION_EXIT_MSG_WITH_LEVEL( logLevel_trace, fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_LOG_DEBUG_RESULT_CODE_FUNCTION_EXIT() ELASTIC_APM_LOG_DEBUG_RESULT_CODE_FUNCTION_EXIT_MSG( "" )
#define ELASTIC_APM_LOG_TRACE_RESULT_CODE_FUNCTION_EXIT() ELASTIC_APM_LOG_TRACE_RESULT_CODE_FUNCTION_EXIT_MSG( "" )

#define ELASTIC_APM_FORCE_LOG_CRITICAL( fmt, ... ) \
    do { \
        logWithLogger( \
            getGlobalLogger(), \
            /* isForced: */ true, \
            logLevel_critical, \
            ELASTIC_APM_STRING_LITERAL_TO_VIEW( ELASTIC_APM_CURRENT_LOG_CATEGORY ), \
            ELASTIC_APM_STRING_LITERAL_TO_VIEW( __FILE__ ), \
            __LINE__, \
            ELASTIC_APM_STRING_LITERAL_TO_VIEW( __FUNCTION__ ), \
            (fmt) , ##__VA_ARGS__ ); \
    } while ( 0 )

static inline
String streamLogLevel( LogLevel level, TextOutputStream* txtOutStream )
{
    if ( level == logLevel_not_set )
        return streamStringView( ELASTIC_APM_STRING_LITERAL_TO_VIEW( "not_set" ), txtOutStream );

    if ( level >= numberOfLogLevels )
        return streamInt( level, txtOutStream );

    return streamString( logLevelToName( level ), txtOutStream );
}

static inline
LogLevel maxEnabledLogLevel()
{
    return getGlobalLogger()->maxEnabledLevel;
}

static inline
bool canLogSecuritySensitive()
{
    return maxEnabledLogLevel() >= logLevel_debug;
}

static inline
String logSecuritySensitiveOr( String securitySensitiveString, String notSecuritySensitiveAltString )
{
    return canLogSecuritySensitive() ? securitySensitiveString : notSecuritySensitiveAltString;
}

static inline
String logSecuritySensitive( String securitySensitiveString )
{
    return logSecuritySensitiveOr( securitySensitiveString, /* notSecuritySensitiveAltString */ "REDACTED" );
}

ResultCode resetLoggingStateInForkedChild();

#define ELASTIC_APM_LOG_CATEGORY_ASSERT "Assert"
#define ELASTIC_APM_LOG_CATEGORY_AUTO_INSTRUMENT "Auto-Instrument"
#define ELASTIC_APM_LOG_CATEGORY_BACKEND_COMM "Backend-Comm"
#define ELASTIC_APM_LOG_CATEGORY_CONFIG "Configuration"
#define ELASTIC_APM_LOG_CATEGORY_C_TO_PHP "C-to-PHP"
#define ELASTIC_APM_LOG_CATEGORY_EXT_API "Ext-API"
#define ELASTIC_APM_LOG_CATEGORY_EXT_INFRA "Ext-Infra"
#define ELASTIC_APM_LOG_CATEGORY_LIFECYCLE "Lifecycle"
#define ELASTIC_APM_LOG_CATEGORY_LOG "Log"
#define ELASTIC_APM_LOG_CATEGORY_MEM_TRACKER "Memory-Tracker"
#define ELASTIC_APM_LOG_CATEGORY_PLATFORM "Platform"
#define ELASTIC_APM_LOG_CATEGORY_SUPPORT "Supportability"
#define ELASTIC_APM_LOG_CATEGORY_SYS_METRICS "System-Metrics"
#define ELASTIC_APM_LOG_CATEGORY_UTIL "Util"

#define ELASTIC_APM_LOG_DIRECT_CRITICAL( fmt, ... ) ELASTIC_APM_LOG_DIRECT( logLevel_critical, fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_LOG_DIRECT_INFO( fmt, ... ) ELASTIC_APM_LOG_DIRECT( logLevel_info, fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_LOG_DIRECT_DEBUG( fmt, ... ) ELASTIC_APM_LOG_DIRECT( logLevel_debug, fmt, ##__VA_ARGS__ )

#ifdef PHP_WIN32

#define ELASTIC_APM_SIGNAL_SAFE_LOG_CRITICAL( fmt, ... )
#define ELASTIC_APM_SIGNAL_SAFE_LOG_WARNING( fmt, ... )
#define ELASTIC_APM_SIGNAL_SAFE_LOG_DEBUG( fmt, ... )

#else // #ifdef PHP_WIN32

#define ELASTIC_APM_SIGNAL_SAFE_LOG_CRITICAL( fmt, ... ) ELASTIC_APM_LOG_TO_BACKGROUND_SINK( logLevel_critical, fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_SIGNAL_SAFE_LOG_WARNING( fmt, ... ) ELASTIC_APM_LOG_TO_BACKGROUND_SINK( logLevel_warning, fmt, ##__VA_ARGS__ )
#define ELASTIC_APM_SIGNAL_SAFE_LOG_DEBUG( fmt, ... ) ELASTIC_APM_LOG_TO_BACKGROUND_SINK( logLevel_debug, fmt, ##__VA_ARGS__ )

#endif // #ifdef PHP_WIN32
