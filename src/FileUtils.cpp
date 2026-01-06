#include "FileUtils.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <windows.h>


std::wstring FileUtils::ReadFileUtf8(const std::wstring &path) {
  HANDLE hFile = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE)
    return L"";

  DWORD fileSize = GetFileSize(hFile, NULL);
  if (fileSize == INVALID_FILE_SIZE) {
    CloseHandle(hFile);
    return L"";
  }

  std::vector<char> buffer(fileSize + 1);
  DWORD bytesRead;
  if (!ReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL)) {
    CloseHandle(hFile);
    return L"";
  }
  buffer[bytesRead] = 0;
  CloseHandle(hFile);

  // Convert UTF-8 to Wide Char
  int wlen = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), -1, NULL, 0);
  if (wlen == 0)
    return L"";

  std::vector<wchar_t> wBuffer(wlen);
  MultiByteToWideChar(CP_UTF8, 0, buffer.data(), -1, wBuffer.data(), wlen);

  std::wstring result = wBuffer.data();
  // Normalize line endings to CRLF for Edit control (or keep as is? Edit
  // control needs CRLF usually to display correctly) Actually, standard Edit
  // control expects CRLF. Windows 10+ Edit control might handle LF, but let's
  // normalize to standard internal (CRLF) for editing. However, we want to
  // PRESERVE or SELECT the mode. For now, let's just return raw. But the
  // standard Edit control needs \r\n to break lines visually in some versions.

  // A simple find replace for visual display:
  // This is a naive implementation; for a real editor, we'd handle this better.
  std::wstring displayResult;
  displayResult.reserve(result.size());
  for (size_t i = 0; i < result.size(); ++i) {
    if (result[i] == L'\r') {
      if (i + 1 < result.size() && result[i + 1] == L'\n') {
        displayResult += L"\r\n"; // Keep CRLF
        i++;
      } else {
        displayResult += L"\r\n"; // Treat raw CR as newline
      }
    } else if (result[i] == L'\n') {
      displayResult += L"\r\n"; // Treat raw LF as newline
    } else {
      displayResult += result[i];
    }
  }

  return result;
  // Wait, if I normalize everything to CRLF for display, I might lose the
  // original format info. The user wants to "select the end of file code". So
  // if the file is LF, we probably display it normalized, but remember "LF" was
  // the format. Auto-detection logic should go here.
}

bool FileUtils::WriteFileUtf8(const std::wstring &path,
                              const std::wstring &content, EolMode eol) {
  // Convert logic to desired EOL
  std::wstring output;
  output.reserve(content.size());

  const wchar_t *p = content.c_str();
  while (*p) {
    if (*p == L'\r' && *(p + 1) == L'\n') {
      // Found CRLF
      if (eol == EolMode::CRLF)
        output += L"\r\n";
      else if (eol == EolMode::LF)
        output += L"\n";
      else if (eol == EolMode::CR)
        output += L"\r";
      p += 2;
    } else if (*p == L'\r') { // CR
      if (eol == EolMode::CRLF)
        output += L"\r\n";
      else if (eol == EolMode::LF)
        output += L"\n";
      else if (eol == EolMode::CR)
        output += L"\r";
      p++;
    } else if (*p ==
               L'\n') { // LF (sometimes Edit control might insert just LF?)
      if (eol == EolMode::CRLF)
        output += L"\r\n";
      else if (eol == EolMode::LF)
        output += L"\n";
      else if (eol == EolMode::CR)
        output += L"\r";
      p++;
    } else {
      output += *p++;
    }
  }

  // Convert Wide to UTF-8
  int len =
      WideCharToMultiByte(CP_UTF8, 0, output.c_str(), -1, NULL, 0, NULL, NULL);
  if (len == 0)
    return false;

  std::vector<char> utf8Buffer(len);
  WideCharToMultiByte(CP_UTF8, 0, output.c_str(), -1, utf8Buffer.data(), len,
                      NULL, NULL);
  // Remove null terminator from file write if present (length includes it)
  int writeLen = len - 1;

  HANDLE hFile = CreateFile(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE)
    return false;

  DWORD bytesWritten;
  // Write BOM if needed? User didn't specify. Standard UTF-8 usually no BOM.
  bool res = WriteFile(hFile, utf8Buffer.data(), writeLen, &bytesWritten, NULL);
  CloseHandle(hFile);
  return res;
}
