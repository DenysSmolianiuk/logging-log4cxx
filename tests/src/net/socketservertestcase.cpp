/*
 * Copyright 2003,2004 The Apache Software Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(WIN32) || defined(_WIN32)
	#include <windows.h>
#endif

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <log4cxx/logger.h>
#include <log4cxx/net/socketappender.h>
#include <log4cxx/ndc.h>
#include <log4cxx/mdc.h>
#include <log4cxx/asyncappender.h>

#include "socketservertestcase.h"
#include "../util/compare.h"
#include "../util/transformer.h"
#include "../util/linenumberfilter.h"
#include "../util/controlfilter.h"
#include "../util/absolutedateandtimefilter.h"
#include "../util/threadfilter.h"
#include "../xml/xlevel.h"
#include "../util/filenamefilter.h"
#include <apr_time.h>
#include <log4cxx/file.h>
#include <iostream>
#include <log4cxx/helpers/transcoder.h>
#include <log4cxx/helpers/stringhelper.h>

//Define INT64_C for compilers that don't have it
#if (!defined(INT64_C))
#define INT64_C(value)  value ## LL
#endif


using namespace log4cxx;
using namespace log4cxx::helpers;
using namespace log4cxx::net;

#define _T(str) L ## str

#define TEMP LOG4CXX_FILE("output/temp")
#define FILTERED LOG4CXX_FILE("output/filtered")

// %5p %x [%t] %c %m%n
// DEBUG T1 [thread] org.apache.log4j.net.SocketAppenderTestCase Message 1
#define PAT1 \
	LOG4CXX_STR("^(DEBUG| INFO| WARN|ERROR|FATAL|LETHAL) T1 \\[\\d*]\\ ") \
	LOG4CXX_STR(".* Message \\d{1,2}")

// DEBUG T2 [thread] patternlayouttest.cpp(?) Message 1
#define PAT2 \
	LOG4CXX_STR("^(DEBUG| INFO| WARN|ERROR|FATAL|LETHAL) T2 \\[\\d*]\\ ") \
	LOG4CXX_STR(".*socketservertestcase.cpp\\(\\d{1,4}\\) Message \\d{1,2}")

// DEBUG T3 [thread] patternlayouttest.cpp(?) Message 1
#define PAT3 \
	LOG4CXX_STR("^(DEBUG| INFO| WARN|ERROR|FATAL|LETHAL) T3 \\[\\d*]\\ ") \
	LOG4CXX_STR(".*socketservertestcase.cpp\\(\\d{1,4}\\) Message \\d{1,2}")

// DEBUG some T4 MDC-TEST4 [thread] SocketAppenderTestCase - Message 1
// DEBUG some T4 MDC-TEST4 [thread] SocketAppenderTestCase - Message 1
#define PAT4 \
	LOG4CXX_STR("^(DEBUG| INFO| WARN|ERROR|FATAL|LETHAL) some T4 MDC-TEST4 \\[\\d*]\\") \
	LOG4CXX_STR(" (root|SocketServerTestCase) - Message \\d{1,2}")
#define PAT5 \
	LOG4CXX_STR("^(DEBUG| INFO| WARN|ERROR|FATAL|LETHAL) some5 T5 MDC-TEST5 \\[\\d*]\\") \
	LOG4CXX_STR(" (root|SocketServerTestCase) - Message \\d{1,2}")
#define PAT6 \
	LOG4CXX_STR("^(DEBUG| INFO| WARN|ERROR|FATAL|LETHAL) some6 T6 client-test6 MDC-TEST6") \
	LOG4CXX_STR(" \\[\\d*]\\ (root|SocketServerTestCase) - Message \\d{1,2}")
#define PAT7 \
	LOG4CXX_STR("^(DEBUG| INFO| WARN|ERROR|FATAL|LETHAL) some7 T7 client-test7 MDC-TEST7") \
	LOG4CXX_STR(" \\[\\d*]\\ (root|SocketServerTestCase) - Message \\d{1,2}")

// DEBUG some8 T8 shortSocketServer MDC-TEST7 [thread] SocketServerTestCase - Message 1
#define PAT8 \
	LOG4CXX_STR("^(DEBUG| INFO| WARN|ERROR|FATAL|LETHAL) some8 T8 shortSocketServer") \
	LOG4CXX_STR(" MDC-TEST8 \\[\\d*]\\ (root|SocketServerTestCase) - Message \\d{1,2}")

class ShortSocketServerLauncher
{
public:
	ShortSocketServerLauncher()
	{
		if (!launched)
		{
#ifdef WIN32
			PROCESS_INFORMATION pi;
			STARTUPINFO si;
			ZeroMemory( &si, sizeof(si) );
			si.cb = sizeof(si);
			ZeroMemory( &pi, sizeof(pi) );
			LogString commandLine(LOG4CXX_STR("src\\shortsocketserver 8 input/socketServer"));

			BOOL bResult = ::CreateProcess(NULL, (LPTSTR)commandLine.c_str(), NULL, NULL,
				TRUE, 0, NULL, NULL, &si, &pi);
#else
			if(!::fork())
			{
				::execl("src/shortsocketserver", "shortsocketserver",
					"8", "input/socketServer", 0);
				::perror("execl() failed");
				::exit(1);
			}
			else
			{
				sleep(1);
			}
#endif
			launched = true;
		}
	}

	static bool launched;

};

bool ShortSocketServerLauncher::launched = false;


class SocketServerTestCase : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(SocketServerTestCase);
		CPPUNIT_TEST(test1);
		CPPUNIT_TEST(test2);
		CPPUNIT_TEST(test3);
		CPPUNIT_TEST(test4);
		CPPUNIT_TEST(test5);
		CPPUNIT_TEST(test6);
		CPPUNIT_TEST(test7);
		CPPUNIT_TEST(test8);
	CPPUNIT_TEST_SUITE_END();

	SocketAppenderPtr socketAppender;
	LoggerPtr logger;
	LoggerPtr root;


public:
	void setUp()
	{
		ShortSocketServerLauncher();
  		logger = Logger::getLogger(LOG4CXX_STR("org.apache.log4j.net.SocketServerTestCase"));
		root = Logger::getRootLogger();
	}

	void tearDown()
	{
		socketAppender = 0;
		root->getLoggerRepository()->resetConfiguration();
		logger = 0;
		root = 0;
	}

	/**
	The pattern on the server side: %5p %x [%t] %c %m%n.

	We are testing NDC functionality across the wire.
	*/
	void test1()
	{
		SocketAppenderPtr socketAppender =
			new SocketAppender(LOG4CXX_STR("localhost"), PORT);
		root->addAppender(socketAppender);
		common(LOG4CXX_STR("T1"), LOG4CXX_STR("key1"), LOG4CXX_STR("MDC-TEST1"));
		delay(1);

		ControlFilter cf;
		cf << PAT1;

		ThreadFilter threadFilter;

		std::vector<Filter *> filters;
		filters.push_back(&cf);
		filters.push_back(&threadFilter);

		try
		{
			Transformer::transform(TEMP, FILTERED, filters);
		}
		catch(UnexpectedFormatException& e)
		{
			std::cout << "UnexpectedFormatException :" << e.what() << std::endl;
			throw;
		}

		CPPUNIT_ASSERT(Compare::compare(FILTERED, LOG4CXX_FILE("witness/socketServer.1")));
	}

	void test2()
	{
		SocketAppenderPtr socketAppender =
			new SocketAppender(LOG4CXX_STR("localhost"), PORT);
		root->addAppender(socketAppender);
		common(LOG4CXX_STR("T2"), LOG4CXX_STR("key2"), LOG4CXX_STR("MDC-TEST2"));
		delay(1);

		ControlFilter cf;
		cf << PAT2;

		ThreadFilter threadFilter;
		LineNumberFilter lineNumberFilter;
                LogString thisFile;
                log4cxx::helpers::Transcoder::decode(__FILE__, thisFile);
                FilenameFilter filenameFilter(thisFile, LOG4CXX_STR("socketservertestcase.cpp"));

		std::vector<Filter *> filters;
		filters.push_back(&cf);
		filters.push_back(&threadFilter);
		filters.push_back(&lineNumberFilter);
                filters.push_back(&filenameFilter);

		try
		{
			Transformer::transform(TEMP, FILTERED, filters);
		}
		catch(UnexpectedFormatException& e)
		{
			std::cout << "UnexpectedFormatException :" << e.what() << std::endl;
			throw;
		}

		CPPUNIT_ASSERT(Compare::compare(FILTERED, LOG4CXX_FILE("witness/socketServer.2")));
	}

 	void test3()
	{
		SocketAppenderPtr socketAppender =
			new SocketAppender(LOG4CXX_STR("localhost"), PORT);
		root->addAppender(socketAppender);
		common(LOG4CXX_STR("T3"), LOG4CXX_STR("key3"), LOG4CXX_STR("MDC-TEST3"));
		delay(1);

		ControlFilter cf;
		cf << PAT3;

		ThreadFilter threadFilter;
		LineNumberFilter lineNumberFilter;
                LogString thisFile;
                log4cxx::helpers::Transcoder::decode(__FILE__, thisFile);
                FilenameFilter filenameFilter(thisFile, LOG4CXX_STR("socketservertestcase.cpp"));

		std::vector<Filter *> filters;
		filters.push_back(&cf);
		filters.push_back(&threadFilter);
		filters.push_back(&lineNumberFilter);
                filters.push_back(&filenameFilter);

		try
		{
			Transformer::transform(TEMP, FILTERED, filters);
		}
		catch(UnexpectedFormatException& e)
		{
			std::cout << "UnexpectedFormatException :" << e.what() << std::endl;
			throw;
		}

		CPPUNIT_ASSERT(Compare::compare(FILTERED, LOG4CXX_FILE("witness/socketServer.3")));
	}

 	void test4()
	{
		SocketAppenderPtr socketAppender =
			new SocketAppender(LOG4CXX_STR("localhost"), PORT);
		root->addAppender(socketAppender);
		NDC::push(_T("some"));
		common(LOG4CXX_STR("T4"), LOG4CXX_STR("key4"), LOG4CXX_STR("MDC-TEST4"));
		NDC::pop();
		delay(1);

		ControlFilter cf;
		cf << PAT4;

		ThreadFilter threadFilter;

		std::vector<Filter *> filters;
		filters.push_back(&cf);
		filters.push_back(&threadFilter);

		try
		{
			Transformer::transform(TEMP, FILTERED, filters);
		}
		catch(UnexpectedFormatException& e)
		{
			std::cout << "UnexpectedFormatException :" << e.what() << std::endl;
			throw;
		}

		CPPUNIT_ASSERT(Compare::compare(FILTERED, LOG4CXX_FILE("witness/socketServer.4")));
	}

	void test5()
	{
		SocketAppenderPtr socketAppender =
			new SocketAppender(LOG4CXX_STR("localhost"), PORT);
		AsyncAppenderPtr asyncAppender = new AsyncAppender();

		root->addAppender(socketAppender);
		root->addAppender(asyncAppender);

		NDC::push(_T("some5"));
		common(LOG4CXX_STR("T5"), LOG4CXX_STR("key5"), LOG4CXX_STR("MDC-TEST5"));
		NDC::pop();
		delay(2);

		ControlFilter cf;
		cf << PAT5;

		ThreadFilter threadFilter;

		std::vector<Filter *> filters;
		filters.push_back(&cf);
		filters.push_back(&threadFilter);

		try
		{
			Transformer::transform(TEMP, FILTERED, filters);
		}
		catch(UnexpectedFormatException& e)
		{
			std::cout << "UnexpectedFormatException :" << e.what() << std::endl;
			throw;
		}

		CPPUNIT_ASSERT(Compare::compare(FILTERED, LOG4CXX_FILE("witness/socketServer.5")));
	}

	void test6()
	{
		SocketAppenderPtr socketAppender =
			new SocketAppender(LOG4CXX_STR("localhost"), PORT);
		AsyncAppenderPtr asyncAppender = new AsyncAppender();

		root->addAppender(socketAppender);
		root->addAppender(asyncAppender);

		NDC::push(_T("some6"));
    	MDC::put(_T("hostID"), _T("client-test6"));
		common(LOG4CXX_STR("T6"), LOG4CXX_STR("key6"), LOG4CXX_STR("MDC-TEST6"));
		NDC::pop();
  		MDC::remove(_T("hostID"));
		delay(2);

		ControlFilter cf;
		cf << PAT6;

		ThreadFilter threadFilter;

		std::vector<Filter *> filters;
		filters.push_back(&cf);
		filters.push_back(&threadFilter);

		try
		{
			Transformer::transform(TEMP, FILTERED, filters);
		}
		catch(UnexpectedFormatException& e)
		{
			std::cout << "UnexpectedFormatException :" << e.what() << std::endl;
			throw;
		}

		CPPUNIT_ASSERT(Compare::compare(FILTERED, LOG4CXX_FILE("witness/socketServer.6")));
	}

	void test7()
	{
		SocketAppenderPtr socketAppender =
			new SocketAppender(LOG4CXX_STR("localhost"), PORT);
		AsyncAppenderPtr asyncAppender = new AsyncAppender();

		root->addAppender(socketAppender);
		root->addAppender(asyncAppender);

		NDC::push(_T("some7"));
    	MDC::put(_T("hostID"), _T("client-test7"));
		common(LOG4CXX_STR("T7"), LOG4CXX_STR("key7"), LOG4CXX_STR("MDC-TEST7"));
		NDC::pop();
  		MDC::remove(_T("hostID"));
		delay(2);

		ControlFilter cf;
		cf << PAT7;

		ThreadFilter threadFilter;

		std::vector<Filter *> filters;
		filters.push_back(&cf);
		filters.push_back(&threadFilter);

		try
		{
			Transformer::transform(TEMP, FILTERED, filters);
		}
		catch(UnexpectedFormatException& e)
		{
			std::cout << "UnexpectedFormatException :" << e.what() << std::endl;
			throw;
		}

		CPPUNIT_ASSERT(Compare::compare(FILTERED, LOG4CXX_FILE("witness/socketServer.7")));
	}

	void test8()
	{
		SocketAppenderPtr socketAppender =
			new SocketAppender(LOG4CXX_STR("localhost"), PORT);

		root->addAppender(socketAppender);

		NDC::push(_T("some8"));
 		common(LOG4CXX_STR("T8"), LOG4CXX_STR("key8"), LOG4CXX_STR("MDC-TEST8"));
		NDC::pop();
		delay(2);

		ControlFilter cf;
		cf << PAT8;

		ThreadFilter threadFilter;

		std::vector<Filter *> filters;
		filters.push_back(&cf);
		filters.push_back(&threadFilter);

		try
		{
			Transformer::transform(TEMP, FILTERED, filters);
		}
		catch(UnexpectedFormatException& e)
		{
			std::cout << "UnexpectedFormatException :" << e.what() << std::endl;
			throw;
		}

		CPPUNIT_ASSERT(Compare::compare(FILTERED, LOG4CXX_FILE("witness/socketServer.8")));
	}

	void common(const LogString& dc, const LogString& key, const LogString& val)
	{
		int i = -1;
		NDC::push(dc);
		MDC::put(key, val);
                Pool p;

		i++;
                LogString msg(LOG4CXX_STR("Message "));

		LOG4CXX_LOG(logger, XLevel::TRACE,
                     msg + StringHelper::toString(i, p));
		i++;
		LOG4CXX_DEBUG(logger, msg + StringHelper::toString(i, p));
		i++;
		LOG4CXX_DEBUG(root, msg + StringHelper::toString(i, p));
		i++;
		LOG4CXX_INFO(logger, msg + StringHelper::toString(i, p));
		i++;
		LOG4CXX_WARN(logger, msg + StringHelper::toString(i, p));
		i++;
		LOG4CXX_LOG(logger, XLevel::LETHAL, msg + StringHelper::toString(i, p)); //5

		NDC::pop();
		MDC::remove(key);
	}

	void delay(int secs)
	{
		apr_sleep(APR_USEC_PER_SEC * secs);
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(SocketServerTestCase);
