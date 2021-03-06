/***************************************************************************
                          msxmlreader.cpp  -  MsXMLReader
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

#include <log4cxx/helpers/msxmlreader.h>
#include <log4cxx/helpers/loglog.h>

using namespace log4cxx;
using namespace log4cxx::xml;
using namespace log4cxx::helpers;
using namespace MSXML;

#define COM_EXEC(stmt) { HRESULT hr = stmt; if (FAILED(hr)) throw _com_error(hr); }

void MsXMLReader::parse(DOMConfigurator * configurator, const tstring& URL)
{
	this->configurator = configurator;

	try
	{
		CreateDOMDocumentInstance();

		VARIANT_BOOL bSuccess = document->load(URL.c_str());

		if (bSuccess)
		{
			tstring parentTagName;
			parseElement(parentTagName, document->documentElement);
		}
		else
		{
			LogLog::error(_T("Could not open [")+URL+_T("]."));
		}
	}
	catch(_com_error&)
	{
		LogLog::error(_T("Could not open [")+URL+_T("]."));
	}
}

/////////////////////////////////////////////////////////////////////////////
// Implementation

void MsXMLReader::CreateDOMDocumentInstance()
{
	HRESULT hRes;
	hRes = document.CreateInstance(L"Msxml2.DOMDocument.3.0");
	if (FAILED(hRes))
	{
		hRes = document.CreateInstance(L"Msxml2.DOMDocument.2.6");
		if (FAILED(hRes))
		{
			hRes = document.CreateInstance(L"Msxml2.DOMDocument");
			if (FAILED(hRes))
			{
				hRes = document.CreateInstance(L"Msxml.DOMDocument");
				if (FAILED(hRes))
				{
					throw _com_error(E_FAIL);
				}
			}
		}
	}
}

void MsXMLReader::parseElement(const tstring& parentTagName, IXMLDOMElementPtr& element)
{
	tstring tagName = (const TCHAR *)element->tagName;
	configurator->BuildElement(parentTagName, tagName);

	// parse attributes
	IXMLDOMNamedNodeMapPtr namedNodeMap = element->attributes;
	long l, length = namedNodeMap->length;
	
	for (l = 0; l < length; l++)
	{
		IXMLDOMAttributePtr attribute = namedNodeMap->item[l];
		if (attribute != 0)
		{
			_bstr_t name = attribute->name;
			_bstr_t nodeValue = attribute->nodeValue;
			configurator->BuildAttribute(tagName, (const TCHAR *)name, (const TCHAR *)nodeValue);
		}
	}

	// parse children elements
	IXMLDOMNodeListPtr nodeList = element->childNodes;
	length = nodeList->length;

	for (l = 0; l < length; l++)
	{
		IXMLDOMElementPtr element = nodeList->item[l];

		if (element != 0)
		{
			parseElement(tagName, element);
		}
	}
}


#endif // WIN32
