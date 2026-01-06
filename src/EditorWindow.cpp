#include "EditorWindow.h"
#include "../resources/resource.h"
#include "FileUtils.h"
#include <commctrl.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <shlwapi.h>
#include <shobjidl.h>
#include <sstream>
#include <windowsx.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")

using json = nlohmann::json;

EditorWindow::EditorWindow()
    : m_hwnd(NULL), m_hTabCtrl(NULL), m_activePageIndex(-1),
      m_currentLang("en") {}

EditorWindow::~EditorWindow() { SaveSettings(); }

bool EditorWindow::Create(PCWSTR lpWindowName, DWORD dwStyle, DWORD dwExStyle,
                          int x, int y, int nWidth, int nHeight,
                          HWND hWndParent, HMENU hMenu) {
  WNDCLASS wc = {0};
  wc.lpfnWndProc = EditorWindow::WindowProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.lpszClassName = L"JYEditorClass";
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

  RegisterClass(&wc);

  m_hwnd = CreateWindowEx(dwExStyle, L"JYEditorClass", lpWindowName, dwStyle, x,
                          y, nWidth, nHeight, hWndParent, hMenu,
                          GetModuleHandle(NULL), this);

  return (m_hwnd ? true : false);
}

LRESULT CALLBACK EditorWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                          LPARAM lParam) {
  EditorWindow *pThis = NULL;

  if (uMsg == WM_NCCREATE) {
    CREATESTRUCT *pCreate = (CREATESTRUCT *)lParam;
    pThis = (EditorWindow *)pCreate->lpCreateParams;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    pThis->m_hwnd = hwnd;
  } else {
    pThis = (EditorWindow *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  }

  if (pThis) {
    return pThis->HandleMessage(uMsg, wParam, lParam);
  } else {
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
}

LRESULT EditorWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_CREATE:
    OnCreate();
    return 0;
  case WM_SIZE:
    OnSize(LOWORD(lParam), HIWORD(lParam));
    return 0;
  case WM_COMMAND:
    OnCommand(LOWORD(wParam), HIWORD(wParam));
    return 0;
  case WM_NOTIFY: {
    NMHDR *pnm = (NMHDR *)lParam;
    if (pnm->idFrom == IDC_TAB_CONTROL && pnm->code == TCN_SELCHANGE) {
      int index = TabCtrl_GetCurSel(m_hTabCtrl);
      SwitchTab(index);
    }
  }
    return 0;
  case WM_DESTROY:
    OnDestroy();
    return 0;
  }
  return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

void EditorWindow::OnCreate() {
  // Check for doc folder
  CreateDirectory(L"doc", NULL);

  // Create Tab Control
  m_hTabCtrl = CreateWindow(
      WC_TABCONTROL, L"", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE, 0, 0, 0, 0,
      m_hwnd, (HMENU)IDC_TAB_CONTROL, GetModuleHandle(NULL), NULL);

  SendMessage(m_hTabCtrl, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),
              0);

  // Create Menus
  HMENU hMenu = CreateMenu();
  HMENU hFileMenu = CreatePopupMenu();
  AppendMenu(hFileMenu, MF_STRING, IDM_FILE_NEW, L"&New");
  AppendMenu(hFileMenu, MF_STRING, IDM_FILE_OPEN, L"&Open");
  AppendMenu(hFileMenu, MF_STRING, IDM_FILE_SAVE, L"&Save");
  AppendMenu(hFileMenu, MF_STRING, IDM_FILE_SAVEAS, L"Save &As...");
  AppendMenu(hFileMenu, MF_STRING, IDM_FILE_CLOSE_TAB, L"&Close Tab");
  AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
  AppendMenu(hFileMenu, MF_STRING, IDM_FILE_EXIT, L"E&xit");
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");

  HMENU hFormatMenu = CreatePopupMenu();
  AppendMenu(hFormatMenu, MF_STRING, IDM_FORMAT_JSON, L"Format &JSON");
  AppendMenu(hFormatMenu, MF_STRING, IDM_FORMAT_YAML, L"Format &YAML");
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFormatMenu, L"F&ormat");

  HMENU hEolMenu = CreatePopupMenu();
  AppendMenu(hEolMenu, MF_STRING, IDM_EOL_CRLF, L"CRLF (Windows)");
  AppendMenu(hEolMenu, MF_STRING, IDM_EOL_LF, L"LF (Unix)");
  AppendMenu(hEolMenu, MF_STRING, IDM_EOL_CR, L"CR (Mac Legacy)");
  AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hEolMenu, L"&Line Endings");

  SetMenu(m_hwnd, hMenu);

  LoadSettings();
  if (m_documents.empty()) {
    CreateNewTab(L"", L"");
  }
}

void EditorWindow::CreateNewTab(const std::wstring &path,
                                const std::wstring &content) {
  Document doc;
  doc.filePath = path;
  doc.fileName = GetFileNameFromPath(path);
  doc.eolMode = 0; // Default CRLF
  doc.isDirty = false;

  // Create Edit Control
  HFONT hFont =
      CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                 DEFAULT_PITCH | FF_MODERN, L"Consolas");

  doc.hEdit =
      CreateWindowEx(0, L"EDIT", content.c_str(),
                     WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE |
                         ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
                     0, 0, 0, 0, m_hwnd, NULL, GetModuleHandle(NULL), NULL);

  SendMessage(doc.hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
  // Limit text mainly by memory
  SendMessage(doc.hEdit, EM_LIMITTEXT, 0, 0);

  m_documents.push_back(doc);
  int newIndex = (int)m_documents.size() - 1;

  TCITEM tie;
  tie.mask = TCIF_TEXT;
  tie.pszText = (LPWSTR)doc.fileName.c_str();
  TabCtrl_InsertItem(m_hTabCtrl, newIndex, &tie);

  SwitchTab(newIndex);
}

void EditorWindow::SwitchTab(int index) {
  if (index < 0 || index >= m_documents.size())
    return;

  if (m_activePageIndex != -1) {
    ShowWindow(m_documents[m_activePageIndex].hEdit, SW_HIDE);
  }

  m_activePageIndex = index;
  TabCtrl_SetCurSel(m_hTabCtrl, index);
  ShowWindow(m_documents[index].hEdit, SW_SHOW);
  SetFocus(m_documents[index].hEdit); // Define focus

  ResizeTabControl();
  UpdateTitle();

  // Update Menu State
  HMENU hMenu = GetMenu(m_hwnd);
  int currentEol = m_documents[index].eolMode;
  CheckMenuItem(hMenu, IDM_EOL_CRLF,
                currentEol == 0 ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenu, IDM_EOL_LF, currentEol == 1 ? MF_CHECKED : MF_UNCHECKED);
  CheckMenuItem(hMenu, IDM_EOL_CR, currentEol == 2 ? MF_CHECKED : MF_UNCHECKED);
}

void EditorWindow::OnSize(int width, int height) {
  if (m_hTabCtrl) {
    MoveWindow(m_hTabCtrl, 0, 0, width, height, TRUE);
    ResizeTabControl();
  }
}

void EditorWindow::ResizeTabControl() {
  if (m_activePageIndex == -1)
    return;

  RECT rc;
  GetClientRect(m_hTabCtrl, &rc);
  TabCtrl_AdjustRect(m_hTabCtrl, FALSE, &rc);

  MoveWindow(m_documents[m_activePageIndex].hEdit, rc.left, rc.top,
             rc.right - rc.left, rc.bottom - rc.top, TRUE);
}

void EditorWindow::OnCommand(int id, int code) {
  switch (id) {
  case IDM_FILE_NEW:
    CreateNewTab();
    break;
  case IDM_FILE_EXIT:
    DestroyWindow(m_hwnd);
    break;
  case IDM_FILE_OPEN:
    OpenFile();
    break;
  case IDM_FILE_SAVE:
    SaveFile();
    break;
  case IDM_FILE_SAVEAS:
    SaveFileAs();
    break;
  case IDM_FILE_CLOSE_TAB:
    CloseCurrentTab();
    break;
  case IDM_FORMAT_JSON:
    FormatJson();
    break;
  case IDM_FORMAT_YAML:
    FormatYaml();
    break;
  case IDM_EOL_CRLF:
    if (m_activePageIndex >= 0)
      m_documents[m_activePageIndex].eolMode = 0;
    // Force redraw menu check
    SwitchTab(m_activePageIndex);
    break;
  case IDM_EOL_LF:
    if (m_activePageIndex >= 0)
      m_documents[m_activePageIndex].eolMode = 1;
    SwitchTab(m_activePageIndex);
    break;
  case IDM_EOL_CR:
    if (m_activePageIndex >= 0)
      m_documents[m_activePageIndex].eolMode = 2;
    SwitchTab(m_activePageIndex);
    break;
  }
}

void EditorWindow::OnDestroy() {
  SaveSettings();
  PostQuitMessage(0);
}

void EditorWindow::CloseCurrentTab() {
  if (m_activePageIndex == -1)
    return;

  // Check dirty (omitted for brevity, assume user wants to close)
  DestroyWindow(m_documents[m_activePageIndex].hEdit);
  TabCtrl_DeleteItem(m_hTabCtrl, m_activePageIndex);
  m_documents.erase(m_documents.begin() + m_activePageIndex);

  if (m_documents.empty()) {
    m_activePageIndex = -1;
    CreateNewTab(); // Always keep one
  } else {
    int newIndex = m_activePageIndex;
    if (newIndex >= m_documents.size())
      newIndex = (int)m_documents.size() - 1;
    m_activePageIndex = -1; // Force switch
    SwitchTab(newIndex);
  }
}

void EditorWindow::OpenFile() {
  IFileOpenDialog *pFileOpen;
  HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
                                IID_IFileOpenDialog,
                                reinterpret_cast<void **>(&pFileOpen));
  if (SUCCEEDED(hr)) {
    hr = pFileOpen->Show(m_hwnd);
    if (SUCCEEDED(hr)) {
      IShellItem *pItem;
      hr = pFileOpen->GetResult(&pItem);
      if (SUCCEEDED(hr)) {
        PWSTR pszFilePath;
        hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
        if (SUCCEEDED(hr)) {
          std::wstring wpath = pszFilePath;
          // Check if already open
          for (size_t i = 0; i < m_documents.size(); i++) {
            if (m_documents[i].filePath == wpath) {
              SwitchTab((int)i);
              CoTaskMemFree(pszFilePath);
              pItem->Release();
              pFileOpen->Release();
              return;
            }
          }

          std::wstring content = FileUtils::ReadFileUtf8(wpath);
          CreateNewTab(wpath, content);

          CoTaskMemFree(pszFilePath);
        }
        pItem->Release();
      }
    }
    pFileOpen->Release();
  }
}

void EditorWindow::SaveFile() {
  if (m_activePageIndex == -1)
    return;
  Document &doc = m_documents[m_activePageIndex];

  if (doc.filePath.empty()) {
    SaveFileAs();
    return;
  }

  int len = GetWindowTextLength(doc.hEdit);
  std::vector<wchar_t> buffer(len + 1);
  GetWindowText(doc.hEdit, buffer.data(), len + 1);

  std::wstring text = buffer.data();
  if (FileUtils::WriteFileUtf8(doc.filePath, text,
                               (FileUtils::EolMode)doc.eolMode)) {
    doc.isDirty = false;
    UpdateTitle();
  }
}

void EditorWindow::SaveFileAs() {
  if (m_activePageIndex == -1)
    return;
  Document &doc = m_documents[m_activePageIndex];

  IFileSaveDialog *pFileSave;
  HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_ALL,
                                IID_IFileSaveDialog,
                                reinterpret_cast<void **>(&pFileSave));
  if (SUCCEEDED(hr)) {
    // Set default folder to 'doc'
    wchar_t currentDir[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, currentDir);
    std::wstring docPath = std::wstring(currentDir) + L"\\doc";

    IShellItem *pFolder;
    SHCreateItemFromParsingName(docPath.c_str(), NULL, IID_PPV_ARGS(&pFolder));
    if (pFolder) {
      pFileSave->SetFolder(pFolder);
      pFolder->Release();
    }

    hr = pFileSave->Show(m_hwnd);
    if (SUCCEEDED(hr)) {
      IShellItem *pItem;
      hr = pFileSave->GetResult(&pItem);
      if (SUCCEEDED(hr)) {
        PWSTR pszFilePath;
        hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
        if (SUCCEEDED(hr)) {
          doc.filePath = pszFilePath;
          doc.fileName = GetFileNameFromPath(doc.filePath);

          // Update Tab Text
          TCITEM tie;
          tie.mask = TCIF_TEXT;
          tie.pszText = (LPWSTR)doc.fileName.c_str();
          TabCtrl_SetItem(m_hTabCtrl, m_activePageIndex, &tie);

          SaveFile(); // Now save
          CoTaskMemFree(pszFilePath);
        }
        pItem->Release();
      }
    }
    pFileSave->Release();
  }
}

void EditorWindow::UpdateTitle() {
  if (m_activePageIndex == -1) {
    SetWindowText(m_hwnd, L"JYEditor");
    return;
  }

  std::wstring title = L"JYEditor - " + m_documents[m_activePageIndex].fileName;
  SetWindowText(m_hwnd, title.c_str());
}

std::wstring EditorWindow::GetFileNameFromPath(const std::wstring &path) {
  if (path.empty())
    return L"Untitled";
  std::filesystem::path p(path);
  return p.filename().wstring();
}

void EditorWindow::SaveSettings() {
  json j;
  j["window"]["width"] = 0; // Placeholder

  // Save open files
  std::vector<std::string> files;
  for (const auto &doc : m_documents) {
    if (!doc.filePath.empty()) {
      // Setup simple utf8 conversion
      int len = WideCharToMultiByte(CP_UTF8, 0, doc.filePath.c_str(), -1, NULL,
                                    0, NULL, NULL);
      if (len > 0) {
        std::vector<char> buf(len);
        WideCharToMultiByte(CP_UTF8, 0, doc.filePath.c_str(), -1, buf.data(),
                            len, NULL, NULL);
        files.push_back(std::string(buf.data()));
      }
    }
  }
  j["files"] = files;
  j["language"] = m_currentLang;

  std::ofstream o("settings.json");
  o << j << std::endl;
}

void EditorWindow::LoadSettings() {
  if (!std::filesystem::exists("settings.json"))
    return;

  try {
    std::ifstream i("settings.json");
    json j;
    i >> j;

    // Load files
    if (j.contains("files")) {
      for (const auto &f : j["files"]) {
        std::string s = f.get<std::string>();

        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
        if (len > 0) {
          std::vector<wchar_t> wbuf(len);
          MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, wbuf.data(), len);
          std::wstring wpath = wbuf.data();
          if (std::filesystem::exists(wpath)) {
            std::wstring content = FileUtils::ReadFileUtf8(wpath);
            CreateNewTab(wpath, content);
          }
        }
      }
    }
  } catch (...) {
  }
}

void EditorWindow::FormatJson() {
  if (m_activePageIndex == -1)
    return;
  HWND hEdit = m_documents[m_activePageIndex].hEdit;

  int len = GetWindowTextLength(hEdit);
  if (len == 0)
    return;

  std::vector<wchar_t> buffer(len + 1);
  GetWindowText(hEdit, buffer.data(), len + 1);

  // Convert to UTF-8 for parsing
  int utf8Len =
      WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, NULL, 0, NULL, NULL);
  std::vector<char> utf8Buf(utf8Len + 1); // +1 for safety
  WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, utf8Buf.data(), utf8Len,
                      NULL, NULL);
  utf8Buf[utf8Len] = 0; // Ensure null terminated

  try {
    auto j = json::parse(utf8Buf.data());
    std::string formatted = j.dump(4);

    // Convert back to Wide
    int wLen = MultiByteToWideChar(CP_UTF8, 0, formatted.c_str(), -1, NULL, 0);
    std::vector<wchar_t> wOut(wLen + 1);
    MultiByteToWideChar(CP_UTF8, 0, formatted.c_str(), -1, wOut.data(), wLen);
    wOut[wLen] = 0;

    SetWindowText(hEdit, wOut.data());
  } catch (json::parse_error &e) {
    std::string err = e.what();
    std::wstring wErr(err.begin(), err.end());
    MessageBox(m_hwnd, wErr.c_str(), L"JSON Parse Error", MB_OK | MB_ICONERROR);
  }
}

#include <yaml-cpp/yaml.h>

void EditorWindow::FormatYaml() {
  if (m_activePageIndex == -1)
    return;
  HWND hEdit = m_documents[m_activePageIndex].hEdit;

  int len = GetWindowTextLength(hEdit);
  if (len == 0)
    return;

  std::vector<wchar_t> buffer(len + 1);
  GetWindowText(hEdit, buffer.data(), len + 1);

  // Convert to UTF-8
  int utf8Len =
      WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, NULL, 0, NULL, NULL);
  std::vector<char> utf8Buf(utf8Len + 1);
  WideCharToMultiByte(CP_UTF8, 0, buffer.data(), -1, utf8Buf.data(), utf8Len,
                      NULL, NULL);
  utf8Buf[utf8Len] = 0;

  try {
    YAML::Node node = YAML::Load(utf8Buf.data());
    YAML::Emitter out;
    out.SetIndent(2);
    out << node;

    std::string formatted = out.c_str();

    // Convert back to Wide
    int wLen = MultiByteToWideChar(CP_UTF8, 0, formatted.c_str(), -1, NULL, 0);
    std::vector<wchar_t> wOut(wLen + 1);
    MultiByteToWideChar(CP_UTF8, 0, formatted.c_str(), -1, wOut.data(), wLen);
    wOut[wLen] = 0;

    SetWindowText(hEdit, wOut.data());
  } catch (YAML::Exception &e) {
    std::string err = e.what();
    std::wstring wErr(err.begin(), err.end());
    MessageBox(m_hwnd, wErr.c_str(), L"YAML Parse Error", MB_OK | MB_ICONERROR);
  }
}
