// (C) Copyright Gert-Jan de Vos and Jan Wilmans 2013.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)

// Repository at: https://github.com/djeedjay/DebugViewPP/

#include "stdafx.h"
#include <boost/algorithm/string.hpp>
#include "hstream.h"
#include "DebugView++Lib/DBWinWriter.h"
#include "resource.h"
#include "MainFrame.h"

//#define CONSOLE_DEBUG

CAppModule _Module;

namespace fusion {
namespace debugviewpp {

class MessageLoop
{
public:
	explicit MessageLoop(CAppModule& module) :
		m_module(module)
	{
		module.AddMessageLoop(&m_loop);
	}

	~MessageLoop()
	{
		m_module.RemoveMessageLoop();
	}

	int Run()
	{
		return m_loop.Run();
	}

private:
	CAppModule& m_module;
	CMessageLoop m_loop;
};

int ForwardMessagesFromPipe(HANDLE hPipe)
{
	DBWinWriter dbwin;
	DWORD pid = GetParentProcessId();

	hstream pipe(hPipe);
	std::string line;
	while (std::getline(pipe, line))
	{
		line += "\n";
		dbwin.Write(pid, line);
	}

	return 0;
}

int Run(const wchar_t* /*cmdLine*/, int cmdShow)
{
	HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE hFile = nullptr, hPipe = nullptr;
	switch (GetFileType(hStdIn))
	{
	case FILE_TYPE_DISK: hFile = hStdIn; break;
	case FILE_TYPE_PIPE: hPipe = hStdIn; break;
	}

	if (hPipe && IsDBWinViewerActive())
		return ForwardMessagesFromPipe(hPipe);

	fusion::debugviewpp::CMainFrame wndMain;
	MessageLoop theLoop(_Module);

	auto args = GetCommandLineArguments();
	std::wstring fileName;

	for (size_t i = 1; i < args.size(); ++i)
	{
		if (boost::iequals(args[i], L"/min"))
		{
			cmdShow = SW_MINIMIZE;
		}
		else if (boost::iequals(args[i], L"/log"))
		{
			wndMain.SetLogging();
		}
		else if (args[i][0] != '/')
		{
			if (!fileName.empty())
				throw std::runtime_error("Duplicate filename");
			fileName = args[i];
		}
	}

	if (wndMain.CreateEx() == nullptr)
		ThrowLastError(L"Main window creation failed!");

	wndMain.ShowWindow(cmdShow);
	if (!fileName.empty())
		wndMain.Load(fileName);
	else if (hFile)
		wndMain.Load(hFile);
	else if (hPipe)
		wndMain.CapturePipe(hPipe);

	return theLoop.Run();
}

} // namespace debugviewpp 
} // namespace fusion

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
try
{
	fusion::SetPrivilege(SE_DEBUG_NAME, true);
	fusion::SetPrivilege(SE_CREATE_GLOBAL_NAME, true);

	fusion::ComInitialization com(fusion::ComInitialization::ApartmentThreaded);

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(nullptr, 0, 0, 0L);

	AtlInitCommonControls(ICC_BAR_CLASSES);	// add flags to support other controls

	HRESULT hRes = _Module.Init(nullptr, hInstance);
	ATLASSERT(SUCCEEDED(hRes));
	hRes;

#ifdef CONSOLE_DEBUG
	FILE* standardOut;
	AllocConsole();
	freopen_s(&standardOut, "CONOUT$", "wb", stdout);
	std::cout.clear();
#endif

	int nRet = fusion::debugviewpp::Run(lpstrCmdLine, nCmdShow);

#ifdef CONSOLE_DEBUG
	fclose(standardOut);
#endif

	_Module.Term();
	return nRet;
}
catch (std::exception& e)
{
	MessageBoxA(nullptr, e.what(), "DebugView++ Error", MB_OK | MB_ICONERROR);
	return -1;
}
