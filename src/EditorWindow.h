#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <windows.h>

class EditorWindow {
public:
  EditorWindow();
  ~EditorWindow();

  bool Create(PCWSTR lpWindowName, DWORD dwStyle, DWORD dwExStyle = 0,
              int x = CW_USEDEFAULT, int y = CW_USEDEFAULT,
              int nWidth = CW_USEDEFAULT, int nHeight = CW_USEDEFAULT,
              HWND hWndParent = 0, HMENU hMenu = 0);
  HWND Window() const { return m_hwnd; }
  void UpdateLineNumbers(HWND hEdit);

protected:
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                     LPARAM lParam);
  LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

  void OnCreate();
  void OnSize(int width, int height);
  void OnCommand(int id, int code);
  void OnDestroy();

  // File operations
  void NewFile();
  void OpenFile();
  void SaveFile();
  void SaveFileAs();
  void CloseCurrentTab();
  void UpdateTitle();
  void SwitchTab(int index);
  void FormatJson();
  void FormatYaml();

  // Settings & Persistence
  void LoadSettings();
  void SaveSettings();

  void SetLanguage(const std::string &lang);

private:
  struct Document {
    HWND hEdit;
    HWND hLineNum;
    std::wstring filePath; // Empty for new untitled files
    std::wstring fileName; // Display name
    bool isDirty;
    int eolMode; // 0: CRLF, 1: LF, 2: CR

    // Internal Data Structure
    nlohmann::json jsonData;
    enum { FMT_TEXT, FMT_JSON, FMT_YAML } format = FMT_TEXT;
  };

  HWND m_hwnd;
  HWND m_hTabCtrl;
  std::vector<Document> m_documents;
  int m_activePageIndex;

  // Localization
  std::string m_currentLang; // "en", "jp", etc.
  std::wstring GetLocalizedString(const std::string &key);
  void UpdateMenus();

  void CreateNewTab(const std::wstring &path = L"",
                    const std::wstring &content = L"");
  void ResizeTabControl();
  std::wstring GetFileNameFromPath(const std::wstring &path);

  // Tree View & Data Model
  void UpdateTreeFromText();
  void UpdateTextFromModel(bool toYaml = false);
  void SyncModelToTree(); // Uses internal jsonData
  HWND m_hTreeView;
};
