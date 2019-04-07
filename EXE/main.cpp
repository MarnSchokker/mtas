﻿#include "stdafx.h"

HANDLE process = 0;
DWORD focus_frame = 0;
HWND window = 0, frames_list = 0, command_input = 0;
WNDPROC edit_proc = 0, drop_proc = 0;
char current_demo[0xFF] = { 0 };

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	CreateMutexA(0, FALSE, "Local\\MTAS.exe");
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		return -1;
	}

	InitCommonControls();

	AllocConsole();
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	if (!LoadLibraryA("DLL.dll")) {
		MessageBoxA(0, "Failed to load DLL.dll", "Error", 0);
		exit(1);
	}

	WNDCLASSEXW wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = DefWindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = GetModuleHandle(0);
	wcex.hIcon = 0;
	wcex.hCursor = LoadCursor(0, IDC_ARROW);
	wcex.hbrBackground = 0;
	wcex.lpszMenuName = 0;
	wcex.lpszClassName = L"win32app";
	RegisterClassExW(&wcex);

	window = CreateDialog(GetModuleHandle(0), MAKEINTRESOURCE(IDD_WINDOW), CreateWindow(L"win32app", L"", 0, 0, 0, 0, 0, 0, 0, GetModuleHandle(0), 0), DlgProc);
	ShowWindow(window, SW_SHOW);

	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Listener, 0, 0, 0);
	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)Update, 0, 0, 0);

	MSG msg = { 0 };
	while (GetMessage(&msg, 0, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

void Listener() {
	DWORD pid = 0;

	for (;;) {
		PROCESSENTRY32 entry = GetProcessInfoByName(L"MirrorsEdge.exe");
		DWORD t = entry.th32ProcessID;
		if (t) {
			if (pid != t) {
				pid = t;
				CloseHandle(process);
				HANDLE p = OpenProcess(PROCESS_ALL_ACCESS, 0, pid);

				char path[MAX_PATH] = { 0 };
				GetFullPathNameA("DLL.dll", MAX_PATH, path, NULL);

				LPVOID arg = (LPVOID)VirtualAllocEx(p, NULL, strlen(path) + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
				WriteProcessMemory(p, arg, path, strlen(path) + 1, 0);
				HANDLE thread = CreateRemoteThread(p, 0, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"), arg, 0, 0);
				WaitForSingleObject(thread, INFINITE);
				CloseHandle(thread);
				VirtualFreeEx(p, arg, 0, MEM_RELEASE);

				DWORD base = (DWORD)GetModuleInfoByName(pid, L"DLL.dll").modBaseAddr;
				DWORD offset = (DWORD)GetModuleHandleA("DLL.dll");

				AddExport(AddControl);
				AddExport(RemoveControl);
				AddExport(GetControl);
				AddExport(Wait);
				AddExport(NewDemo);
				AddExport(LoadDemo);
				AddExport(SaveDemo);
				AddExport(StartDemo);
				AddExport(GotoFrame);
				AddExport(GetDemoCommand);
				AddExport(GetDemoFrame);
				AddExport(GetDemoFrameCount);
				AddExport(GetDemoFrames);
				AddExport(SetTimescale);
				AddExport(GetTimescale);

				process = p;

				wchar_t command[0xFF] = { 0 };
				ReadBuffer(process, (void *)CallRead(dll.GetDemoCommand), (char *)command, sizeof(command));
				SetDlgItemText(window, IDC_COMMAND, command);

				DWORD control = CallRead(dll.GetControl);
				CheckDlgButton(window, IDC_RECORD, control & CONTROL_RECORD);
				CheckDlgButton(window, IDC_PAUSE_CHANGE, control & CONTROL_PAUSE_CHANGE);
				CheckDlgButton(window, IDC_PAUSE_GROUND, control & CONTROL_PAUSE_GROUND);
				CheckDlgButton(window, IDC_PAUSE_AIR, control & CONTROL_PAUSE_AIR);
				CheckDlgButton(window, IDC_PAUSE_WALLRUN, control & CONTROL_PAUSE_WALLRUN);
				CheckDlgButton(window, IDC_PAUSE_WALLCLIMB, control & CONTROL_PAUSE_WALLCLIMB);
			
				control = CallRead(dll.GetTimescale);
				float scale = *(float *)&control;
				HWND hDlg = window;
				if (scale == 0.1f) {
					CheckTimescale(ID_TIMESCALE_10);
				} else if (scale == 0.25f) {
					CheckTimescale(ID_TIMESCALE_25);
				} else if (scale == 0.5f) {
					CheckTimescale(ID_TIMESCALE_50);
				} else if (scale == 0.75f) {
					CheckTimescale(ID_TIMESCALE_75);
				} else if (scale == 1) {
					CheckTimescale(ID_TIMESCALE_1);
				} else if (scale == 2) {
					CheckTimescale(ID_TIMESCALE_2);
				} else if (scale == 4) {
					CheckTimescale(ID_TIMESCALE_4);
				}
			}
		} else {
			pid = 0;
			if (process) {
				CloseHandle(process);
				process = 0;
			}
		}

		Sleep(500);
	}
}

void Update() {
	for (;;) {
		if (process) {
			DWORD len = CallRead(dll.GetDemoFrameCount);
			DWORD clen = ListView_GetItemCount(frames_list);
			if (len != clen) {
				if (len > clen) {
					len -= clen;
					DWORD size = len * sizeof(FRAME);
					Call(dll.Wait, 0);
					FRAME *frames = (FRAME *)calloc(size, 1);
					ReadBuffer(process, (LPVOID)(CallRead(dll.GetDemoFrames) + (clen * sizeof(FRAME))), (char *)frames, size);
					for (DWORD i = 0; i < len; ++i) {
						AddFrame(&frames[i]);
					}
					free(frames);
				} else {
					ListView_DeleteAllItems(frames_list);
					DWORD size = len * sizeof(FRAME);
					FRAME *frames = (FRAME *)calloc(size, 1);
					ReadBuffer(process, (LPVOID)CallRead(dll.GetDemoFrames), (char *)frames, size);
					for (DWORD i = 0; i < len; ++i) {
						AddFrame(&frames[i]);
					}
					free(frames);
				}
			}

			DWORD frame = CallRead(dll.GetDemoFrame);
			DWORD max = ListView_GetItemCount(frames_list) - 1;
			if (frame > max) {
				frame = max;
			}

			DWORD control = CallRead(dll.GetControl);
			if (frame != focus_frame) {
				if ((control & CONTROL_RECORD) && frame > focus_frame && len == clen) {
					DWORD len = frame - focus_frame;
					DWORD size = len * sizeof(FRAME);
					FRAME *frames = (FRAME *)calloc(size, 1);
					ReadBuffer(process, (LPVOID)(CallRead(dll.GetDemoFrames) + (focus_frame * sizeof(FRAME))), (char *)frames, size);
					for (DWORD i = 0; i < len; ++i) {
						UpdateFrame(focus_frame + i, &frames[i]);
					}
					free(frames);
				}

				SetFocusFrame(frame);
			}

			if (control & CONTROL_PAUSE) {
				SetDlgItemText(window, IDC_PAUSE, L"▶");
			}
		}

		Sleep(20);
	}
}

void UpdateFrame(DWORD index, FRAME *frame) {
	ListView_DeleteItem(frames_list, index);
	wchar_t text[0xFF] = { 0 };

	LVITEM item = { 0 };
	item.iItem = index;
	item.lParam = item.iItem;
	item.mask = LVIF_TEXT | LVIF_PARAM;
	item.pszText = text;
	wsprintf(text, L"%d", item.iItem);

	ListView_InsertItem(frames_list, &item);

	item.mask = LVIF_TEXT;
	float x = 0, y = 0;
	for (DWORD i = 0; i < sizeof(frame->mouse_moves) / sizeof(frame->mouse_moves[0]); ++i) {
		auto mm = frame->mouse_moves[i];
		if (!mm.delta) break;

		if (mm.delta > 0) {
			x += mm.change;
		} else {
			y += mm.change;
		}
	}
	swprintf(text, 0xFF, L"%.2f", x);
	item.iSubItem = 1;
	ListView_SetItem(frames_list, &item);
	swprintf(text, 0xFF, L"%.2f", y);
	item.iSubItem = 2;
	ListView_SetItem(frames_list, &item);

	for (DWORD i = 0; i < sizeof(frame->keys) / sizeof(frame->keys[0]); ++i) {
		auto k = frame->keys[i];
		if (!k.keycode) break;

		if (k.keycode > 0) {
			wsprintf(text, L"%s", k.alias);
		} else {
			wsprintf(text, L"%s ", k.alias);
		}

		item.iSubItem = 3 + i;
		ListView_SetItem(frames_list, &item);
	}
}

void AddFrame(FRAME *frame) {
	DWORD c = ListView_GetItemCount(frames_list);
	UpdateFrame(c, frame);
}

void SetFocusFrame(DWORD frame) {
	DWORD old = focus_frame;
	focus_frame = frame;
	ListView_RedrawItems(frames_list, old, focus_frame);
	ListView_EnsureVisible(frames_list, focus_frame, false);
}

void Call(DWORD addr, DWORD arg) {
	HANDLE thread = CreateRemoteThread(process, 0, 0, (LPTHREAD_START_ROUTINE)addr, (LPDWORD)arg, 0, 0);
	WaitForSingleObject(thread, INFINITE);
	CloseHandle(thread);
}

DWORD CallRead(DWORD addr) {
	LPVOID arg = VirtualAllocEx(process, 0, 4, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	Call(addr, (DWORD)arg);
	DWORD r = 0;
	ReadProcessMemory(process, arg, &r, 4, 0);
	VirtualFreeEx(process, arg, 0, MEM_RELEASE);
	return r;
}

void CopyFramesToClipboard(std::vector <FRAME> *frames) {
	DWORD size = frames->size() * sizeof(FRAME);
	HGLOBAL m = GlobalAlloc(GMEM_MOVEABLE, 4 + size);
	DWORD *ptr = (DWORD *)GlobalLock(m);
	*ptr = frames->size();
	memcpy((void *)((DWORD)ptr + 4), &(*frames)[0], size);
	GlobalUnlock(m);

	OpenClipboard(0);
	EmptyClipboard();
	SetClipboardData(CLIPBOARD_TYPE, m);
	CloseClipboard();
}

void UpdateCommand() {
	wchar_t command[0xFF] = { 0 };
	GetWindowText(command_input, command, 0xFF);
	WriteBuffer(process, (LPVOID)CallRead(dll.GetDemoCommand), (char *)command, sizeof(command));
}

LRESULT FramesListDraw(LPARAM lParam) {
	LPNMLVCUSTOMDRAW draw = (LPNMLVCUSTOMDRAW)lParam;
	switch (draw->nmcd.dwDrawStage) {
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
		case CDDS_ITEMPREPAINT:
			return CDRF_NOTIFYSUBITEMDRAW;
		case CDDS_SUBITEM | CDDS_ITEMPREPAINT: {
			if (draw->iSubItem > 2) {
				wchar_t text[0x20] = { 0 };
				ListView_GetItemText(frames_list, draw->nmcd.lItemlParam, draw->iSubItem, text, 0x20);
				if (*text) {
					if (wcschr(text, L' ')) {
						draw->clrTextBk = RGB(255, 100, 100);
						return CDRF_NEWFONT;
					} else {
						draw->clrTextBk = RGB(100, 255, 100);
						return CDRF_NEWFONT;
					}
				}
			}

			if (draw->nmcd.lItemlParam == focus_frame) {
				draw->clrTextBk = RGB(135, 206, 250);
				return CDRF_NEWFONT;
			}

			draw->clrTextBk = RGB(255, 255, 255);
			return CDRF_NEWFONT;
		}
	}

	return CDRF_DODEFAULT;
}

INT_PTR CALLBACK DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static std::vector<ELEMENT> elements;

	if (message == WM_ERASEBKGND) {
		EnableScrollBar(frames_list, SB_VERT, ESB_ENABLE_BOTH);
		ShowScrollBar(frames_list, SB_BOTH, TRUE);
	}
	
	switch (message) {
		case WM_INITDIALOG: {
			command_input = GetDlgItem(hDlg, IDC_COMMAND);
			frames_list = CreateWindow(WC_LISTVIEW, L"", WS_CHILD | LVS_REPORT, 17, 40, 527 + GetSystemMetrics(SM_CXVSCROLL), 490, hDlg, 0, GetModuleHandle(0), 0);
			ShowWindow(frames_list, TRUE);

			ListView_SetExtendedListViewStyle(frames_list, LVS_EX_FULLROWSELECT);

			LVCOLUMN col = { 0 };
			col.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

			col.pszText = L"Frame";
			col.cx = 45;
			ListView_InsertColumn(frames_list, 0, &col);

			col.pszText = L"MX";
			col.cx = 45;
			ListView_InsertColumn(frames_list, 1, &col);

			col.pszText = L"MY";
			col.cx = 45;
			ListView_InsertColumn(frames_list, 2, &col);

			for (int i = 0; i < sizeof(((FRAME *)0)->keys) / sizeof(((FRAME *)0)->keys[0]); ++i) {
				col.pszText = L"";
				col.cx = 40;
				ListView_InsertColumn(frames_list, 3, &col);
			}

			AddElement(command_input, SIZE_WIDTH);
			AddElement(frames_list, SIZE_WIDTH | SIZE_HEIGHT);
			AddElementById(IDC_ADVANCE, ALIGN_RIGHT);
			AddElementById(IDC_PAUSE, ALIGN_RIGHT);
			AddElementById(IDC_START, ALIGN_RIGHT);
			AddElementById(IDC_STATIC_STATES, ALIGN_RIGHT);
			AddElementById(IDC_PAUSE_CHANGE, ALIGN_RIGHT);
			AddElementById(IDC_PAUSE_GROUND, ALIGN_RIGHT);
			AddElementById(IDC_PAUSE_AIR, ALIGN_RIGHT);
			AddElementById(IDC_PAUSE_WALLRUN, ALIGN_RIGHT);
			AddElementById(IDC_PAUSE_WALLCLIMB, ALIGN_RIGHT);
			AddElementById(IDC_STOP, ALIGN_RIGHT | ALIGN_BOTTOM);
			AddElementById(IDC_RECORD, ALIGN_RIGHT | ALIGN_BOTTOM);

			break;
		}
		case WM_SIZE: case WM_SIZING: {
			RECT size = { 0 };
			GetClientRect(hDlg, &size);

			for (auto e : elements) {
				if (e.flags & ALIGN_RIGHT) {
					SetWindowPos(e.handle, 0, size.right + e.rect.left - e.size.right, e.rect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
				}

				if (e.flags & ALIGN_BOTTOM) {
					RECT r = { 0 };
					GetClientRect(e.handle, &r);
					MapWindowPoints(e.handle, GetParent(e.handle), (LPPOINT)&r, 2);
					SetWindowPos(e.handle, 0, r.left, size.bottom + e.rect.top - e.size.bottom, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
				}

				if (e.flags & SIZE_WIDTH) {
					SetWindowPos(e.handle, 0, 0, 0, size.right - (e.size.right - (e.rect.right - e.rect.left)), e.rect.bottom - e.rect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER);
				}

				if (e.flags & SIZE_HEIGHT) {
					RECT r = { 0 };
					GetClientRect(e.handle, &r);
					SetWindowPos(e.handle, 0, 0, 0, r.right - r.left, size.bottom - (e.size.bottom - (e.rect.bottom - e.rect.top)), SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER);
				}
			}

			InvalidateRect(hDlg, 0, 0);

			break;
		}
		case WM_COMMAND:
			if (!process) break;

			switch (LOWORD(wParam)) {
				case ID_FILE_NEW:
					Call(dll.NewDemo, 0);
					SetDlgItemText(hDlg, IDC_PAUSE, L"||");
					SetWindowText(hDlg, L"MTAS");
					SetWindowText(command_input, L"");
					*current_demo = 0;
					break;
				case ID_FILE_OPEN: {
					char name[0xFF] = { 0 };
					OPENFILENAMEA of = { 0 };
					of.lStructSize = sizeof(of);
					of.lpstrFilter = "*.demo";
					of.lpstrFile = name;
					of.nMaxFile = 0xFF;
					of.Flags = OFN_FILEMUSTEXIST;
					GetOpenFileNameA(&of);
					void *arg = VirtualAllocEx(process, 0, sizeof(name), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
					WriteBuffer(process, arg, name, sizeof(name));
					Call(dll.LoadDemo, (DWORD)arg);
					VirtualFreeEx(process, arg, 0, MEM_RELEASE);
					strcpy(current_demo, name);
					PathStripPathA(name);
					wchar_t title[0xFF] = { 0 };
					wsprintf(title, L"MTAS - %S", name);
					SetWindowText(hDlg, title);
					ReadBuffer(process, (void *)CallRead(dll.GetDemoCommand), (char *)title, sizeof(title));
					SetWindowText(command_input, title);
					SetDlgItemText(hDlg, IDC_PAUSE, L"||");
					SetFocusFrame(0);
					break;
				}
				case ID_FILE_SAVE: {
					if (*current_demo) {
						void *arg = VirtualAllocEx(process, 0, 0xFF, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
						WriteBuffer(process, arg, current_demo, 0xFF);
						Call(dll.SaveDemo, (DWORD)arg);
						VirtualFreeEx(process, arg, 0, MEM_RELEASE);
						break;
					}
				}
				case ID_FILE_SAVEAS: {
					char name[0xFF] = { 0 };
					OPENFILENAMEA sf = { 0 };
					sf.lStructSize = sizeof(sf);
					sf.lpstrFilter = "*.demo";
					sf.lpstrFile = name;
					sf.nMaxFile = 0xFF;
					sf.Flags = OFN_FILEMUSTEXIST;
					GetSaveFileNameA(&sf);
					void *arg = VirtualAllocEx(process, 0, 0xFF, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
					WriteBuffer(process, arg, name, 0xFF);
					Call(dll.SaveDemo, (DWORD)arg);
					VirtualFreeEx(process, arg, 0, MEM_RELEASE);
					strcpy(current_demo, name);
					PathStripPathA(name);
					char title[0xFF] = { 0 };
					sprintf(title, "MTAS - %s", name);
					SetWindowTextA(hDlg, title);
					break;
				}
				case IDC_START: {
					SetDlgItemText(hDlg, IDC_PAUSE, L"||");
					UpdateCommand();
					Call(dll.StartDemo, 0);
					SetFocusFrame(0);
					break;
				}
				case IDC_PAUSE: {
					if (ReadShort(process, (LPVOID)CallRead(dll.GetDemoCommand))) {
						HWND button = GetDlgItem(hDlg, IDC_PAUSE);
						wchar_t text[0xFF] = { 0 };
						GetWindowText(button, text, 0xFF);
						if (*text == L'▶') {
							Call(dll.RemoveControl, CONTROL_ADVANCE);
							Call(dll.RemoveControl, CONTROL_PAUSE);
							SetWindowText(button, L"||");
						} else {
							Call(dll.AddControl, CONTROL_PAUSE);
							SetWindowText(button, L"▶");
						}
					}
					break;
				}
				case IDC_ADVANCE: {
					SetDlgItemText(hDlg, IDC_PAUSE, L"▶");
					Call(dll.AddControl, CONTROL_ADVANCE);
					break;
				}
				case IDC_RECORD: {
					Call(IsDlgButtonChecked(hDlg, IDC_RECORD) ? dll.AddControl : dll.RemoveControl, CONTROL_RECORD);
					break;
				}
				case IDC_PAUSE_CHANGE:
					Call(IsDlgButtonChecked(hDlg, IDC_PAUSE_CHANGE) ? dll.AddControl : dll.RemoveControl, CONTROL_PAUSE_CHANGE);
					break;
				case IDC_PAUSE_GROUND:
					Call(IsDlgButtonChecked(hDlg, IDC_PAUSE_GROUND) ? dll.AddControl : dll.RemoveControl, CONTROL_PAUSE_GROUND);
					break;
				case IDC_PAUSE_AIR:
					Call(IsDlgButtonChecked(hDlg, IDC_PAUSE_AIR) ? dll.AddControl : dll.RemoveControl, CONTROL_PAUSE_AIR);
					break;
				case IDC_PAUSE_WALLRUN:
					Call(IsDlgButtonChecked(hDlg, IDC_PAUSE_WALLRUN) ? dll.AddControl : dll.RemoveControl, CONTROL_PAUSE_WALLRUN);
					break;
				case IDC_PAUSE_WALLCLIMB :
					Call(IsDlgButtonChecked(hDlg, IDC_PAUSE_WALLCLIMB) ? dll.AddControl : dll.RemoveControl, CONTROL_PAUSE_WALLCLIMB);
					break;
				case IDC_STOP:
					WriteShort(process, (LPVOID)CallRead(dll.GetDemoCommand), 0);
					Call(dll.RemoveControl, UINT_MAX);
					SetDlgItemText(hDlg, IDC_PAUSE, L"||");
					break;
				case ID_TIMESCALE_10:
					SetTimescale(0.10f);
					break;
				case ID_TIMESCALE_25:
					SetTimescale(0.25f);
					break;
				case ID_TIMESCALE_50:
					SetTimescale(0.50f);
					break;
				case ID_TIMESCALE_75:
					SetTimescale(0.75f);
					break;
				case ID_TIMESCALE_1:
					SetTimescale(1.00f);
					break;
				case ID_TIMESCALE_2:
					SetTimescale(2.00f);
					break;
				case ID_TIMESCALE_4:
					SetTimescale(4.00f);
					break;
				case ID__CUT: {
					std::vector<FRAME> frames;
					DWORD base = CallRead(dll.GetDemoFrames);
					for (int i = -1; (i = SendMessage(frames_list, LVM_GETNEXTITEM, i, LVNI_SELECTED)) != -1;) {
						FRAME f = { 0 };
						LPVOID addr = (LPVOID)(base + (i * sizeof(FRAME)));
						ReadBuffer(process, addr, (char *)&f, sizeof(FRAME));
						frames.push_back(f);
						memset(&f, 0, sizeof(FRAME));
						WriteBuffer(process, addr, (char *)&f, sizeof(FRAME));
						UpdateFrame(i, &f);
					}

					CopyFramesToClipboard(&frames);
					break;
				}
				case ID__COPY: {
					std::vector<FRAME> frames;
					DWORD base = CallRead(dll.GetDemoFrames);
					for (int i = -1; (i = SendMessage(frames_list, LVM_GETNEXTITEM, i, LVNI_SELECTED)) != -1;) {
						FRAME f = { 0 };
						ReadBuffer(process, (LPVOID)(base + (i * sizeof(FRAME))), (char *)&f, sizeof(FRAME));
						frames.push_back(f);
					}

					CopyFramesToClipboard(&frames);
					break;
				}
				case ID__PASTE: {
					OpenClipboard(0);
					HANDLE m = GetClipboardData(CLIPBOARD_TYPE);
					if (m) {
						DWORD *ptr = (DWORD *)GlobalLock(m);
						FRAME *frames = (FRAME *)((DWORD)ptr + 4);

						DWORD base = CallRead(dll.GetDemoFrames);
						for (int i = -1; (i = SendMessage(frames_list, LVM_GETNEXTITEM, i, LVNI_SELECTED)) != -1;) {
							for (DWORD fi = 0, count = ptr[0], start = ptr[1]; fi < count; ++fi, ++i) {
								FRAME *f = &frames[fi];
								WriteBuffer(process, (LPVOID)(base + (i * sizeof(FRAME))), (char *)f, sizeof(FRAME));
								UpdateFrame(i, f);
							}
						}

						GlobalUnlock(m);
					}
					CloseClipboard();
					break;
				}
				case ID__CLEAR: {
					DWORD base = CallRead(dll.GetDemoFrames);
					for (int i = -1; (i = SendMessage(frames_list, LVM_GETNEXTITEM, i, LVNI_SELECTED)) != -1;) {
						FRAME f = { 0 };
						WriteBuffer(process, (LPVOID)(base + (i * sizeof(FRAME))), (char *)&f, sizeof(FRAME));
						UpdateFrame(i, &f);
					}
				}
			}
			break;
		case WM_NOTIFY: {
			LPNMHDR notif = (LPNMHDR)lParam;
			if (notif->hwndFrom == frames_list) {
				switch (notif->code) {
					case NM_DBLCLK: {
						int i = SendMessage(frames_list, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
						if (i > -1) {
							DialogBoxParam(0, MAKEINTRESOURCE(IDD_FRAME), hDlg, FrameDlgProc, (LPARAM)i);
							FRAME f = { 0 };
							ReadBuffer(process, (LPVOID)(CallRead(dll.GetDemoFrames) + (i * sizeof(FRAME))), (char *)&f, sizeof(FRAME));
							UpdateFrame(i, &f);
							ListView_SetItemState(frames_list, i, LVIS_SELECTED, LVIS_SELECTED);
							ListView_EnsureVisible(frames_list, i, false);
						}
						return TRUE;
					}
					case NM_RCLICK: {
						POINT cursor;
						GetCursorPos(&cursor);
						TrackPopupMenu(GetSubMenu(LoadMenu(0, MAKEINTRESOURCE(IDR_FRAME)), 0), TPM_LEFTALIGN | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, hDlg, 0);
						return TRUE;
					}
					case NM_CUSTOMDRAW:
						SetWindowLong(hDlg, DWL_MSGRESULT, FramesListDraw(lParam));
						return TRUE;
				}

			}
			break;
		}
		case WM_CLOSE:
			exit(0);
	}

	return (INT_PTR)FALSE;
}

LRESULT CALLBACK EditProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_KEYUP) {
		if (wParam == VK_ESCAPE) {
			SendMessage(GetParent(hWnd), WM_CLOSE, 0, 0);
			return TRUE;
		} else if (wParam == VK_RETURN) {
			SendMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(1, 0), 0);
			return TRUE;
		}
	}

	return CallWindowProc(edit_proc, hWnd, message, wParam, lParam);
}

LRESULT CALLBACK DropProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	if (message == WM_KEYUP) {
		if (wParam == VK_ESCAPE) {
			SendMessage(GetParent(hWnd), WM_CLOSE, 0, 0);
			return TRUE;
		} else if (wParam == VK_RETURN) {
			SendMessage(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(1, 0), 0);
			return TRUE;
		}
	}

	return CallWindowProc(drop_proc, hWnd, message, wParam, lParam);
}

INT_PTR CALLBACK MouseDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static MOUSE_INPUT *mi = 0;

	switch (message) {
		case WM_INITDIALOG: {
			HWND type = GetDlgItem(hDlg, IDC_MOUSE_TYPE);

			SendMessage(type, CB_ADDSTRING, 0, (LPARAM)L"X");
			SendMessage(type, CB_ADDSTRING, 0, (LPARAM)L"Y");

			mi = (MOUSE_INPUT *)lParam;
			if (mi->type == MOUSE_INPUT_X) {
				SendMessage(type, CB_SETCURSEL, 0, 0);
			} else {
				SendMessage(type, CB_SETCURSEL, 1, 0);
			}

			char text[0xFF] = { 0 };
			sprintf(text, "%f", mi->change);
			SetDlgItemTextA(hDlg, IDC_MOUSE_CHANGE, text);

			edit_proc = (WNDPROC)SetWindowLongPtr(GetDlgItem(hDlg, IDC_MOUSE_CHANGE), GWLP_WNDPROC, (LONG)EditProc);
			drop_proc = (WNDPROC)SetWindowLongPtr(GetDlgItem(hDlg, IDC_MOUSE_TYPE), GWLP_WNDPROC, (LONG)DropProc);

			break;
		}
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_MOUSE_OK: case 1: {
					char text[0xFF] = { 0 };
					GetWindowTextA(GetDlgItem(hDlg, IDC_MOUSE_CHANGE), text, 0xFF);
					float change = 0;
					sscanf(text, "%f", &change);
					mi->change = change;
					GetWindowTextA(GetDlgItem(hDlg, IDC_MOUSE_TYPE), text, 0xFF);
					mi->type = strcmp(text, "X") == 0 ? 0 : 1;
					EndDialog(hDlg, 1);
					break;
				}
			}
			break;
		case WM_CLOSE:
			EndDialog(hDlg, 0);
			break;
	}

	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK KeyboardDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static KEYBOARD_INPUT *ki = 0;
	switch (message) {
		case WM_INITDIALOG: {
			HWND type = GetDlgItem(hDlg, IDC_KEYBOARD_TYPE);

			SendMessage(type, CB_ADDSTRING, 0, (LPARAM)L"Down");
			SendMessage(type, CB_ADDSTRING, 0, (LPARAM)L"Up");

			ki = (KEYBOARD_INPUT *)lParam;
			if (ki->keycode >= 0) {
				SendMessage(type, CB_SETCURSEL, 0, 0);
			} else {
				SendMessage(type, CB_SETCURSEL, 1, 0);
			}

			if (*ki->alias) SetDlgItemText(hDlg, IDC_KEYBOARD_ALIAS, ki->alias);

			drop_proc = (WNDPROC)SetWindowLongPtr(GetDlgItem(hDlg, IDC_KEYBOARD_TYPE), GWLP_WNDPROC, (LONG)DropProc);
		}
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_KEYBOARD_ALIAS: {
					EnableWindow(GetDlgItem(hDlg, IDC_KEYBOARD_ALIAS), FALSE);
					for (;;) {
						for (short i = 0; i < 0xFF; ++i) {
							if (GetAsyncKeyState(i) < 0 && *KEYS[i]) {
								ki->keycode = i;
								wcscpy(ki->alias, KEYS[i]);
								goto leave;
							}
						}
						Sleep(1);
					}
				leave:
					EnableWindow(GetDlgItem(hDlg, IDC_KEYBOARD_ALIAS), TRUE);
					SetDlgItemText(hDlg, IDC_KEYBOARD_ALIAS, ki->alias);

					break;
				}
				case IDC_KEYBOARD_OK: case 1: {
					if (ki->keycode) {
						char text[0xFF] = { 0 };
						GetWindowTextA(GetDlgItem(hDlg, IDC_KEYBOARD_TYPE), text, 0xFF);
						ki->keycode = (strcmp(text, "Down") == 0 ? abs(ki->keycode) : -abs(ki->keycode));
						EndDialog(hDlg, 1);
					} else {
						EndDialog(hDlg, 0);
					}
					break;
				}
			}
			break;
		case WM_CLOSE:
			EndDialog(hDlg, 0);
			break;
	}

	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK FrameDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static DWORD index = 0;
	static FRAME frame = { 0 };
	static HWND mouse_list = 0;
	static HWND keyboard_list = 0;

	switch (message) {
		case WM_INITDIALOG: {
			mouse_list = CreateWindow(WC_LISTVIEW, L"", WS_CHILD | LVS_REPORT | LVS_SINGLESEL, 10, 30, 195, 340, hDlg, 0, GetModuleHandle(0), 0);
			ShowWindow(mouse_list, TRUE);

			ListView_SetExtendedListViewStyle(mouse_list, LVS_EX_FULLROWSELECT);

			LVCOLUMN col = { 0 };
			col.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

			col.pszText = L"Type";
			col.cx = 40;
			ListView_InsertColumn(mouse_list, 0, &col);

			col.pszText = L"Change";
			col.cx = 195 - col.cx;
			ListView_InsertColumn(mouse_list, 1, &col);

			keyboard_list = CreateWindow(WC_LISTVIEW, L"", WS_CHILD | LVS_REPORT | LVS_SINGLESEL, 217, 30, 195, 340, hDlg, 0, GetModuleHandle(0), 0);
			ShowWindow(keyboard_list, TRUE);

			ListView_SetExtendedListViewStyle(keyboard_list, LVS_EX_FULLROWSELECT);

			col.pszText = L"Type";
			col.cx = 40;
			ListView_InsertColumn(keyboard_list, 0, &col);

			col.pszText = L"Bind";
			col.cx = 195 - col.cx;
			ListView_InsertColumn(keyboard_list, 1, &col);

			DWORD i = (DWORD)lParam;
			if (i < CallRead(dll.GetDemoFrameCount)) {
				index = i;
				char text[0xFF] = { 0 };
				sprintf(text, "Frame %d", index);
				SetWindowTextA(hDlg, text);
				ReadBuffer(process, (LPVOID)(CallRead(dll.GetDemoFrames) + (index * sizeof(FRAME))), (char *)&frame, sizeof(FRAME));

				for (DWORD i = 0; i < sizeof(frame.mouse_moves) / sizeof(frame.mouse_moves[0]); ++i) {
					auto mm = frame.mouse_moves[i];
					if (mm.delta == 0) break;

					LVITEM item = { 0 };
					item.iItem = i;
					item.mask = LVIF_TEXT;
					
					if (mm.delta > 0) {
						item.pszText = L"X";
					} else {
						item.pszText = L"Y";
					}

					ListView_InsertItem(mouse_list, &item);

					item.iSubItem = 1;
					wchar_t text[0xFF] = { 0 };
					swprintf(text, 0xFF, L"%f", mm.change);
					item.pszText = text;
					ListView_SetItem(mouse_list, &item);
				}

				for (DWORD i = 0; i < sizeof(frame.keys) / sizeof(frame.keys[0]); ++i) {
					auto k = frame.keys[i];
					if (k.keycode == 0) break;

					LVITEM item = { 0 };
					item.iItem = i;
					item.mask = LVIF_TEXT;

					if (k.keycode > 0) {
						item.pszText = L"Down";
					} else {
						item.pszText = L"Up";
					}

					ListView_InsertItem(keyboard_list, &item);

					item.iSubItem = 1;
					item.pszText = k.alias;
					ListView_SetItem(keyboard_list, &item);
				}
			}

			break;
		}
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_MOUSE_ADD: {
					MOUSE_INPUT m = { 0 };
					if (DialogBoxParam(0, MAKEINTRESOURCE(IDD_MOUSE), hDlg, MouseDlgProc, (LPARAM)&m)) {
						DWORD i = 0;
						for (; i < sizeof(frame.mouse_moves) / sizeof(frame.mouse_moves[0]); ++i) {
							if (frame.mouse_moves[i].delta == 0) break;
						}

						if (i == sizeof(frame.mouse_moves) / sizeof(frame.mouse_moves[0])) {
							char buffer[0xFF] = { 0 };
							sprintf(buffer, "Only %d mouse inputs allowed", sizeof(frame.mouse_moves) / sizeof(frame.mouse_moves[0]));
							MessageBoxA(0, buffer, "Error", 0);
							break;
						}

						auto mm = &frame.mouse_moves[i];
						mm->delta = (m.type == MOUSE_INPUT_X ? 0.02f : -0.02f);
						mm->change = m.change;

						LVITEM item = { 0 };
						item.iItem = i;
						item.mask = LVIF_TEXT;
						item.pszText = (m.type == MOUSE_INPUT_X ? L"X" : L"Y");

						ListView_InsertItem(mouse_list, &item);

						item.iSubItem = 1;
						wchar_t text[0xFF] = { 0 };
						swprintf(text, 0xFF, L"%f", m.change);
						item.pszText = text;
						ListView_SetItem(mouse_list, &item);
					}
					break;
				}
				case IDC_MOUSE_REMOVE: {
					int i = SendMessage(mouse_list, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
					if (i > -1) {
						memset(&frame.mouse_moves[i], 0, sizeof(frame.mouse_moves[0]));
						for (DWORD e = i + 1; e < sizeof(frame.mouse_moves) / sizeof(frame.mouse_moves[0]); ++e) {
							auto m0 = &frame.mouse_moves[e - 1];
							auto m1 = &frame.mouse_moves[e];
							if (m0->delta == 0 && m1->delta == 0) break;
							*m0 = *m1;
						}
						ListView_DeleteItem(mouse_list, i);
					}
					break;
				}
				case IDC_KEYBOARD_ADD: {
					KEYBOARD_INPUT ki = { 0 };
					if (DialogBoxParam(0, MAKEINTRESOURCE(IDD_KEYBOARD), hDlg, KeyboardDlgProc, (LPARAM)&ki)) {
						DWORD i = 0;
						for (; i < sizeof(frame.keys) / sizeof(frame.keys[0]); ++i) {
							if (frame.keys[i].keycode == 0) break;
						}

						if (i == sizeof(frame.keys) / sizeof(frame.keys[0])) {
							char buffer[0xFF] = { 0 };
							sprintf(buffer, "Only %d keyboard inputs allowed", sizeof(frame.keys) / sizeof(frame.keys[0]));
							MessageBoxA(0, buffer, "Error", 0);
							break;
						}

						auto k = &frame.keys[i];
						wcscpy(k->alias, ki.alias);
						k->keycode = ki.keycode;

						LVITEM item = { 0 };
						item.iItem = i;
						item.mask = LVIF_TEXT;
						item.pszText = (ki.keycode > 0 ? L"Down" : L"Up");

						ListView_InsertItem(keyboard_list, &item);

						item.iSubItem = 1;
						item.pszText = ki.alias;
						ListView_SetItem(keyboard_list, &item);
					}
					break;
				}
				case IDC_KEYBOARD_REMOVE: {
					int i = SendMessage(keyboard_list, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
					if (i > -1) {
						memset(&frame.keys[i], 0, sizeof(frame.keys[0]));
						for (DWORD e = i + 1; e < sizeof(frame.keys) / sizeof(frame.keys[0]); ++e) {
							auto m0 = &frame.keys[e - 1];
							auto m1 = &frame.keys[e];
							if (m0->keycode == 0 && m1->keycode == 0) break;
							*m0 = *m1;
						}
						ListView_DeleteItem(keyboard_list, i);
					}
					break;
				}
				case IDC_GOTO:
					Call(dll.GotoFrame, index);
					SetDlgItemText(GetParent(frames_list), IDC_PAUSE, L"▶");
					break;
			}
			break;
		case WM_NOTIFY: {
			LPNMHDR notif = (LPNMHDR)lParam;
			if (notif->hwndFrom == mouse_list && notif->code == NM_DBLCLK) {
				int i = SendMessage(mouse_list, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
				if (i > -1) {
					MOUSE_INPUT m = { 0 };
					m.type = frame.mouse_moves[i].delta > 0 ? MOUSE_INPUT_X : MOUSE_INPUT_Y;
					m.change = frame.mouse_moves[i].change;
					if (DialogBoxParam(0, MAKEINTRESOURCE(IDD_MOUSE), hDlg, MouseDlgProc, (LPARAM)&m)) {
						auto mm = &frame.mouse_moves[i];
						mm->delta = (m.type == MOUSE_INPUT_X ? 0.02f : -0.02f);
						mm->change = m.change;

						LVITEM item = { 0 };
						item.iItem = i;
						item.mask = LVIF_TEXT;
						item.pszText = (m.type == MOUSE_INPUT_X ? L"X" : L"Y");

						ListView_SetItem(mouse_list, &item);

						item.iSubItem = 1;
						wchar_t text[0xFF] = { 0 };
						swprintf(text, 0xFF, L"%f", m.change);
						item.pszText = text;
						ListView_SetItem(mouse_list, &item);
					}
				}
				return TRUE;
			} else if (notif->hwndFrom == keyboard_list && notif->code == NM_DBLCLK) {
				int i = SendMessage(keyboard_list, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
				if (i > -1) {
					KEYBOARD_INPUT ki = { 0 };
					wcscpy(ki.alias, frame.keys[i].alias);
					ki.keycode = frame.keys[i].keycode;
					if (DialogBoxParam(0, MAKEINTRESOURCE(IDD_KEYBOARD), hDlg, KeyboardDlgProc, (LPARAM)&ki)) {
						auto k = &frame.keys[i];
						wcscpy(k->alias, ki.alias);
						k->keycode = ki.keycode;

						LVITEM item = { 0 };
						item.iItem = i;
						item.mask = LVIF_TEXT;
						item.pszText = (ki.keycode > 0 ? L"Down" : L"Up");

						ListView_SetItem(keyboard_list, &item);

						item.iSubItem = 1;
						item.pszText = ki.alias;
						ListView_SetItem(keyboard_list, &item);
					}
				}
				return TRUE;
			}
			break;
		}
		case WM_CLOSE:
			WriteBuffer(process, (LPVOID)(CallRead(dll.GetDemoFrames) + (index * sizeof(FRAME))), (char *)&frame, sizeof(FRAME));
			EndDialog(hDlg, 0);
			break;
	}
	return (INT_PTR)FALSE;
}