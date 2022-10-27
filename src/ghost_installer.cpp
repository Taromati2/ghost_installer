#include <windows.h>
#include <shlobj_core.h>

#include "my-gists/ukagaka/SSP_Runner.hpp"
#include "my-gists/windows/WaitForXObjectWithMessageLoop.hpp"
#include "my-gists/windows/get_temp_path.hpp"
#include "my-gists/windows/download_file.hpp"
#include "my-gists/windows/IsNetworkHasCost.hpp"

#include "resource/resource.h"
#include "ghost_installer.hpp"

namespace download_speed_up_thread {
	void download_speed_up_ssp() {
		try {
			download_file_to_temp_dir(L"https://github.com/Taromati2/package-factory/releases/latest/download/ssp.exe", L"SSP.exe");
		}
		catch(const std::exception& e) {
			MessageBoxA(NULL, e.what(), "Error", MB_OK);
			exit(1);
		}
	}
	void download_speed_up_nar() {
		try {
			download_file_to_temp_dir(L"https://github.com/Taromati2/package-factory/releases/latest/download/Taromati2.nar", L"Taromati2.nar");
		}
		catch(const std::exception& e) {
			MessageBoxA(NULL, e.what(), "Error", MB_OK);
			exit(1);
		}
	}
}		// namespace download_speed_up_thread


namespace ssp_install {
	std::wstring program_dir;
	bool		 ok_to_install = false;
}		// namespace ssp_install
bool get_edit_Dia_text_as_path(std::wstring& path, HWND hDlg, WORD IDC) {
	// Get number of characters.
	size_t pathlen = SendDlgItemMessage(hDlg,
										IDC,
										EM_LINELENGTH,
										(WPARAM)0,
										(LPARAM)0);
	if(pathlen == 0) {
		MessageBox(hDlg,
				   L"请输入路径",
				   L"Error",
				   MB_OK);
		return false;
	}

	// Put the number of characters into first word of buffer.
	path.resize(pathlen);
	path[0] = (wchar_t)pathlen;

	// Get the characters.
	SendDlgItemMessage(hDlg,
					   IDC,
					   EM_GETLINE,
					   (WPARAM)0,		// line 0
					   (LPARAM)path.data());
	return true;
}
LRESULT CALLBACK InstallPathSelDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lp) {
	using namespace ssp_install;
	switch(message) {
	case WM_INITDIALOG:
		SetDlgItemText(hDlg, IDC_PATHEDIT, program_dir.c_str());
		SetFocus(GetDlgItem(hDlg, IDC_PATHEDIT));
		return FALSE;
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK: {
			if(get_edit_Dia_text_as_path(program_dir, hDlg, IDC_PATHEDIT)) {
				EndDialog(hDlg, TRUE);
				ok_to_install = true;
				return TRUE;
			}
			else
				return FALSE;
		}
		case IDCANCEL:
			EndDialog(hDlg, TRUE);
			exit(0);
		case IDSELECT:
			//get path by browsing
			//set browsing Root as IDC_PATHEDIT's text
			{
				BROWSEINFO bi;
				ZeroMemory(&bi, sizeof(bi));
				bi.hwndOwner	  = hDlg;
				bi.pszDisplayName = new wchar_t[MAX_PATH];
				bi.lpszTitle	  = L"选择SSP安装路径";
				bi.ulFlags		  = BIF_RETURNONLYFSDIRS;
				LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
				if(pidl != NULL) {
					SHGetPathFromIDList(pidl, bi.pszDisplayName);
					program_dir = bi.pszDisplayName;
					if(!program_dir.ends_with(L"SSP") || !program_dir.ends_with(L"SSP\\") || !program_dir.ends_with(L"ssp") || !program_dir.ends_with(L"ssp\\"))
						program_dir += L"\\SSP\\";
					auto pos = program_dir.find(L"\\\\");
					if(pos != std::wstring::npos)
						program_dir.replace(pos, 2, L"\\");
					SetDlgItemText(hDlg, IDC_PATHEDIT, program_dir.c_str());
				}
				delete[] bi.pszDisplayName;
				return TRUE;
			}
		default:
			return FALSE;
		}
	default:
		return FALSE;
	}
}

// Winmain
int APIENTRY WinMain(
	_In_ HINSTANCE	   hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR		   lpCmdLine,
	_In_ int		   nShowCmd) {
	//*
	if(IsNetworkHasCost()) {
		auto result = MessageBox(NULL,
								 L"您的网络可能是计费网络，是否继续安装？",
								 L"注意",
								 MB_YESNO);
		if(result == IDNO)
			return 0;
	}
	//*/

	SSP_Runner SSP;
	HANDLE	   nar_download_thread = NULL;
	auto	   downloading_ui	   = CreateDialogW(hInstance, (LPCTSTR)IDD_DOWNLOADING, NULL, NULL);
	if(SSP.IsInstalled()) {
		nar_download_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)download_speed_up_thread::download_speed_up_nar, NULL, 0, NULL);
	install_ghost:
		if(!nar_download_thread) {
			MessageBox(NULL,
					   L"创建下载进程失败",
					   L"Error",
					   MB_OK);
			return 1;
		}
		ShowWindow(downloading_ui, SW_SHOW);
		DWORD wait_result = WaitForSingleObjectWithMessageLoop(nar_download_thread, INFINITE);
		ShowWindow(downloading_ui, SW_HIDE);
		if(wait_result == WAIT_FAILED) {
			MessageBox(NULL, L"下载nar失败", L"Error", MB_OK);
			exit(1);
		}
		auto nar_file = std::wstring(get_temp_path()) + L"Taromati2.nar";
		#ifndef _DEBUG
			SSP.install_nar(nar_file, L"/o", L"callghost,deletesource");
		#else
			SSP.install_nar(nar_file, L"/o", L"callghost");
		#endif
		//We can't wait for ssp to terminate before deleting the nar file, because when ghost ends is up to the user
		//So, no clearing of temporary files
	}
	else {
		std::wstring tmp_path			  = get_temp_path();
		auto		 ssp_file			  = tmp_path + L"SSP.exe";
		bool		 ssp_download_started = _waccess(ssp_file.c_str(), 0) == 0;
		auto ssp_download_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)download_speed_up_thread::download_speed_up_ssp, NULL, 0, NULL);
		if(!ssp_download_thread) {
			MessageBox(NULL,
					   L"创建下载进程失败",
					   L"Error",
					   MB_OK);
			return 1;
		}
		if(!ssp_download_started) {
			auto response = MessageBox(NULL,
										L"SSP未安装，此程序是运行ghost的基础平台\n点击确认以安装SSP并继续\n若已安装SSP，请关掉SSP并在接下来的安装路径选择中选择现有的SSP路径以更新SSP至最新版本",
										L"SSP未安装",
										MB_YESNO);
			if(response != IDYES)
				return 0;
		}
		{
			//wait for ssp to download
			DWORD wait_result = WaitForSingleObjectWithMessageLoop(ssp_download_thread, 0);
			if(wait_result == WAIT_OBJECT_0) {
				//start nar download
				nar_download_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)download_speed_up_thread::download_speed_up_nar, NULL, 0, NULL);
			}
			EXE_Runner SSP_EXE(ssp_file);
			//check if ssp install path saved
			auto ssp_install_path_file = tmp_path + L"SSP_install_path.txt";
			std::wstring& ssp_install_path = ssp_install::program_dir;
			if(_waccess(ssp_install_path_file.c_str(), 0) == 0) {
				//ssp install path saved
				//use winAPI to read the file
				HANDLE hFile = CreateFile(ssp_install_path_file.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if(hFile == INVALID_HANDLE_VALUE) {
				read_saved_path_filed:
					MessageBox(NULL, L"读取已保存的SSP安装路径失败", L"Error", MB_OK);
					goto show_InstallPathSelDlg;
				}
				DWORD dwFileSize = GetFileSize(hFile, NULL);
				if(dwFileSize == INVALID_FILE_SIZE) {
					CloseHandle(hFile);
					goto read_saved_path_filed;
				}
				DWORD dwBytesRead;
				ssp_install_path.resize(dwFileSize);
				if(!ReadFile(hFile, &ssp_install_path[0], dwFileSize, &dwBytesRead, NULL)) {
					CloseHandle(hFile);
					goto read_saved_path_filed;
				}
				CloseHandle(hFile);
			}
			else {
			show_InstallPathSelDlg:
				//ssp install path not saved
				//show dialog to let user choose install path
				ssp_install::program_dir = DefaultSSPinstallPath();
				auto install_path_scl_ui = CreateDialogW(hInstance, (LPCTSTR)IDD_INSTALLATION_PATH_SELECTION, NULL, (DLGPROC)InstallPathSelDlgProc);
				ShowWindow(install_path_scl_ui, SW_SHOW);
				{
					MSG msg;
					while(GetMessage(&msg, NULL, 0, 0)) {
						TranslateMessage(&msg);
						DispatchMessage(&msg);
						if(ssp_install::ok_to_install)
							break;
					}
				}
				DestroyWindow(install_path_scl_ui);
				//save ssp install path
				auto hFile = CreateFile(ssp_install_path_file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
				if(hFile == INVALID_HANDLE_VALUE) {
				save_path_failed:
					MessageBox(NULL, L"保存SSP安装路径失败", L"Error", MB_OK);
					goto create_ssp_install_dir;
				}
				DWORD dwBytesWritten;
				if(!WriteFile(hFile, ssp_install::program_dir.c_str(), ssp_install::program_dir.size() * sizeof(wchar_t), &dwBytesWritten, NULL)) {
					CloseHandle(hFile);
					goto save_path_failed;
				}
				CloseHandle(hFile);
			}
		create_ssp_install_dir:
			//create installation directory
			switch(SHCreateDirectoryExW(NULL, ssp_install::program_dir.c_str(), NULL)) {
			case ERROR_ALREADY_EXISTS:
			case ERROR_SUCCESS:
			case ERROR_FILE_EXISTS: {
				//wait for ssp to download
				ShowWindow(downloading_ui, SW_SHOW);
				DWORD wait_result = WaitForSingleObjectWithMessageLoop(ssp_download_thread, INFINITE);
				ShowWindow(downloading_ui, SW_HIDE);
				if(wait_result != WAIT_OBJECT_0) {
					MessageBox(NULL, L"SSP下载失败", L"SSP下载失败", MB_OK);
					return 1;
				}
				//close the ssp download thread
				CloseHandle(ssp_download_thread);
				//start nar download
				if(!nar_download_thread)
					nar_download_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)download_speed_up_thread::download_speed_up_nar, NULL, 0, NULL);
				//install SSP
				SSP_EXE.RunAndWait(L"-o\"" + ssp_install::program_dir + L"\"", L"-y");
				break;
			}
			default:
				MessageBoxW(NULL, L"未能创建安装文件夹\n请考虑以管理员运行此程序或检查安装路径", L"Error", MB_OK);
				return 1;
			}
			//set SSP_Runner's path
			auto ssp_path		   = ssp_install::program_dir + L"\\ssp.exe";
			SSP.reset_path(ssp_path);
			if(!SSP.IsInstalled()) {
				MessageBoxW(NULL, L"未能安装SSP", L"Error", MB_OK);
				return 1;
			}
			//Delete temporary files
			DeleteFileW(ssp_file.c_str());
			DeleteFileW(ssp_install_path_file.c_str());
			//save ssp path to ssp_path_tmp.txt
			auto ssp_path_tmp_file = tmp_path + L"\\ssp_path_tmp.txt";
			auto hFile = CreateFileW(ssp_path_tmp_file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if(hFile == INVALID_HANDLE_VALUE) {
				goto install_ghost;
			}
			DWORD dwBytesWritten;
			if(!WriteFile(hFile, ssp_path.c_str(), ssp_path.size() * sizeof(wchar_t), &dwBytesWritten, NULL)) {
				CloseHandle(hFile);
				goto install_ghost;
			}
			CloseHandle(hFile);
		}
		//install ghost
		goto install_ghost;
	}
	return 0;
}

std::wstring DefaultSSPinstallPath() {
	#ifdef _DEBUG
		std::wstring self_path;
		self_path.resize(MAX_PATH);
		GetModuleFileNameW(NULL, self_path.data(), MAX_PATH);
		self_path = self_path.substr(0, self_path.find_last_of(L"\\"));
		self_path += L"\\SSP";
		return self_path;
	#else
		std::wstring program_dir;
		program_dir.resize(MAX_PATH);
		SHGetSpecialFolderPathW(NULL, program_dir.data(), CSIDL_PROGRAM_FILESX86, FALSE);
		program_dir.resize(wcslen(program_dir.data()));
		program_dir += L"\\SSP";
		return program_dir;
	#endif
}
