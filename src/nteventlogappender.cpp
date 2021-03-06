/***************************************************************************
                nteventlogappender.cpp  -  class NTEventLogAppender
                             -------------------
    begin                : dim avr 20 2003
    copyright            : (C) 2003 by Michael CATANZARITI
    email                : mcatan@free.fr
 ***************************************************************************/

/***************************************************************************
 * Copyright (C) The Apache Software Foundation. All rights reserved.      *
 *                                                                         *
 * This software is published under the terms of the Apache Software       *
 * License version 1.1, a copy of which has been included with this        *
 * distribution in the LICENSE.txt file.                                   *
 ***************************************************************************/

#ifdef WIN32

#include <windows.h>
#undef ERROR
#include <log4cxx/nt/nteventlogappender.h>
#include <log4cxx/spi/loggingevent.h>
#include <log4cxx/helpers/loglog.h>
#include <log4cxx/level.h>
#include <log4cxx/helpers/stringhelper.h>

using namespace log4cxx;
using namespace log4cxx::spi;
using namespace log4cxx::helpers;
using namespace log4cxx::nt;

class CCtUserSIDHelper
{
public:
	static bool FreeSid(SID * pSid)
	{
		return ::HeapFree(GetProcessHeap(), 0, (LPVOID)pSid) != 0;
	}

	static bool CopySid(SID * * ppDstSid, SID * pSrcSid)
	{
		bool bSuccess = false;

		DWORD dwLength = ::GetLengthSid(pSrcSid);
		*ppDstSid = (SID *) ::HeapAlloc(GetProcessHeap(),
		 HEAP_ZERO_MEMORY, dwLength);

		if (::CopySid(dwLength, *ppDstSid, pSrcSid))
		{
			bSuccess = true;
		}
		else
		{
			FreeSid(*ppDstSid);
		}

		return bSuccess;
	}

	static bool GetCurrentUserSID(SID * * ppSid)
	{
		bool bSuccess = false;

		// Pseudohandle so don't need to close it
		HANDLE hProcess = ::GetCurrentProcess();
		HANDLE hToken = NULL;
		if (::OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
		{
			// Get the required size
			DWORD tusize = 0;
			GetTokenInformation(hToken, TokenUser, NULL, 0, &tusize);
			TOKEN_USER* ptu = (TOKEN_USER*)new BYTE[tusize];

			if (GetTokenInformation(hToken, TokenUser, (LPVOID)ptu, tusize, &tusize))
			{
				bSuccess = CopySid(ppSid, (SID *)ptu->User.Sid);
			}
			
			CloseHandle(hToken);
			delete [] ptu;
		}

		return bSuccess;
	}
};

NTEventLogAppender::NTEventLogAppender() : hEventLog(NULL), pCurrentUserSID(NULL)
{
}

NTEventLogAppender::NTEventLogAppender(const tstring& server, const tstring& log, const tstring& source, LayoutPtr layout)
: server(server), log(log), source(source), hEventLog(NULL), pCurrentUserSID(NULL)
{
	this->layout = layout;
	activateOptions();
}

NTEventLogAppender::~NTEventLogAppender()
{
	finalize();
}


void NTEventLogAppender::close()
{
	if (hEventLog != NULL)
	{
		::DeregisterEventSource(hEventLog);
		hEventLog = NULL;
	}

	if (pCurrentUserSID != NULL)
	{
		CCtUserSIDHelper::FreeSid(pCurrentUserSID);
		pCurrentUserSID = NULL;
	}
}

void NTEventLogAppender::setOption(const tstring& option, const tstring& value)
{
	if (StringHelper::equalsIgnoreCase(option, _T("server")))
	{
		server = value;
	}
	else if (StringHelper::equalsIgnoreCase(option, _T("log")))
	{
		log = value;
	}
	else if (StringHelper::equalsIgnoreCase(option, _T("source")))
	{
		source = value;
	}
	else
	{
		AppenderSkeleton::setOption(option, value);
	}
}

void NTEventLogAppender::activateOptions()
{
	if (source.empty())
	{
		LogLog::warn(_T("Source option not set for appender [")+name+_T("]."));
		return;
	}

	if (log.empty())
	{
		log = _T("Application");
	}

	close();

	// current user security identifier
	CCtUserSIDHelper::GetCurrentUserSID(&pCurrentUserSID);

	addRegistryInfo();

	hEventLog = ::RegisterEventSource(server.c_str(), source.c_str());
}

void NTEventLogAppender::append(const LoggingEvent& event)
{
	if (hEventLog == NULL)
	{
		LogLog::warn(_T("NT EventLog not opened."));
		return;
	}

	tostringstream oss;
	layout->format(oss, event);
	tstring sz = oss.str();
	const TCHAR * s = sz.c_str();

	BOOL bSuccess = ::ReportEvent(
		hEventLog,
		getEventType(event),
		getEventCategory(event),
		0x1000,
		pCurrentUserSID,
		1,
		0,
		&s,
		NULL);

	if (!bSuccess)
	{
		DWORD dwError = ::GetLastError();
		LogLog::error(_T("Cannot report event in NT EventLog."));
	}
}

HKEY NTEventLogAppender::regGetKey(const tstring& subkey, DWORD *disposition)
{
	HKEY hkey = 0;
	RegCreateKeyEx(HKEY_LOCAL_MACHINE, subkey.c_str(), 0, NULL, 
		REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, 
		&hkey, disposition);
	return hkey;
}

void NTEventLogAppender::regSetString(HKEY hkey, const tstring& name, const tstring& value)
{
	RegSetValueEx(hkey, name.c_str(), 0, REG_SZ, (LPBYTE)value.c_str(), value.length()*sizeof(wchar_t));
}

void NTEventLogAppender::regSetDword(HKEY hkey, const tstring& name, DWORD value)
{
	RegSetValueEx(hkey, name.c_str(), 0, REG_DWORD, (LPBYTE)&value, sizeof(DWORD));
}

/*
 * Add this source with appropriate configuration keys to the registry.
 */
void NTEventLogAppender::addRegistryInfo()
{
	DWORD disposition;
	HKEY hkey = 0;
	tstring subkey = _T("SYSTEM\\CurrentControlSet\\Services\\EventLog\\")
		+ log + _T("\\") + source;
	
	hkey = regGetKey(subkey, &disposition);
	if (disposition == REG_CREATED_NEW_KEY)
	{
		regSetString(hkey, _T("EventMessageFile"), _T("NTEventLogAppender.dll"));
		regSetString(hkey, _T("CategoryMessageFile"), _T("NTEventLogAppender.dll"));
		regSetDword(hkey, _T("TypesSupported"), (DWORD)7);
		regSetDword(hkey, _T("CategoryCount"), (DWORD)5);
	}
	
	RegCloseKey(hkey);
	return;
}

WORD NTEventLogAppender::getEventType(const LoggingEvent& event)
{
	WORD ret_val;
	
	switch (event.getLevel().toInt())
	{
	case Level::FATAL_INT:
	case Level::ERROR_INT:
		ret_val = EVENTLOG_ERROR_TYPE;
		break;
	case Level::WARN_INT:
		ret_val = EVENTLOG_WARNING_TYPE;
		break;
	case Level::INFO_INT:
	case Level::DEBUG_INT:
	default:
		ret_val = EVENTLOG_INFORMATION_TYPE;
		break;
	}

	return ret_val;
}

WORD NTEventLogAppender::getEventCategory(const LoggingEvent& event)
{
	WORD ret_val;
	
	switch (event.getLevel().toInt())
	{
	case Level::FATAL_INT:
		ret_val = 1;
		break;
	case Level::ERROR_INT:
		ret_val = 2;
		break;
	case Level::WARN_INT:
		ret_val = 3;
		break;
	case Level::INFO_INT:
		ret_val = 4;
		break;
	case Level::DEBUG_INT:
	default:
		ret_val = 5;
		break;
	}

	return ret_val;
}

#endif // WIN32
