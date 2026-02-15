#include <windows.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "core/reference_engine.hpp"

__attribute__((objc_root_class))
@interface RefPadObjCBridge
+ (const wchar_t*)windowTitle;
+ (const wchar_t*)aboutCredits;
@end

@implementation RefPadObjCBridge
+ (const wchar_t*)windowTitle {
  return L"RefPad Brain - Objective-C++ + Modern C++";
}

+ (const wchar_t*)aboutCredits {
  return L"RefPad\\r\\n\\r\\nCredits\\r\\n"
         L"- Core migration scaffold: Modern C++23/2c\\r\\n"
         L"- GUI shell: Objective-C++ + Win32\\r\\n"
         L"- Build flow: macOS MinGW cross-compile to Windows 11";
}
@end

namespace {

constexpr wchar_t kWindowClassName[] = L"RefPadMainWindow";
constexpr int kSearchEditId = 1001;
constexpr int kOpeningListId = 1002;
constexpr int kWikiPaneId = 1003;
constexpr int kCloseButtonId = 1004;
constexpr int kParentMenuId = 1005;
constexpr int kSecondaryMenuId = 1006;
constexpr int kMenuCloseId = 2001;
constexpr int kMenuAboutId = 2002;

struct AppState {
  refpad::ReferenceEngine engine;
  std::vector<std::string> opening_keys;

  HWND search_edit = nullptr;
  HWND parent_menu = nullptr;
  HWND secondary_menu = nullptr;
  HWND opening_list = nullptr;
  HWND wiki_pane = nullptr;
  HWND close_button = nullptr;
};

std::wstring utf8_to_wide(std::string_view text) {
  if (text.empty()) {
    return L"";
  }

  const int required =
      MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (required <= 0) {
    return L"";
  }

  std::wstring output(static_cast<size_t>(required), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), output.data(),
                      required);
  return output;
}

std::string wide_to_utf8(const std::wstring& text) {
  if (text.empty()) {
    return "";
  }

  const int required =
      WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
                          nullptr, nullptr);
  if (required <= 0) {
    return "";
  }

  std::string output(static_cast<size_t>(required), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), output.data(),
                      required, nullptr, nullptr);
  return output;
}

std::wstring read_window_text(HWND control) {
  const int len = GetWindowTextLengthW(control);
  if (len <= 0) {
    return L"";
  }

  std::wstring buffer(static_cast<size_t>(len + 1), L'\0');
  GetWindowTextW(control, buffer.data(), len + 1);
  buffer.resize(static_cast<size_t>(len));
  return buffer;
}

std::string selected_combo_text(HWND control) {
  const LRESULT selected_index = SendMessageW(control, CB_GETCURSEL, 0, 0);
  if (selected_index == CB_ERR) {
    return "";
  }

  const LRESULT text_len = SendMessageW(control, CB_GETLBTEXTLEN, selected_index, 0);
  if (text_len == CB_ERR || text_len <= 0) {
    return "";
  }

  std::wstring buffer(static_cast<size_t>(text_len + 1), L'\0');
  SendMessageW(control, CB_GETLBTEXT, selected_index, reinterpret_cast<LPARAM>(buffer.data()));
  buffer.resize(static_cast<size_t>(text_len));
  return wide_to_utf8(buffer);
}

void update_wiki_from_selection(AppState* state) {
  const LRESULT selected_index = SendMessageW(state->opening_list, LB_GETCURSEL, 0, 0);
  if (selected_index == LB_ERR) {
    SetWindowTextW(state->wiki_pane, L"No result selected.");
    return;
  }

  const auto index = static_cast<size_t>(selected_index);
  if (index >= state->opening_keys.size()) {
    SetWindowTextW(state->wiki_pane, L"No result selected.");
    return;
  }

  const auto& key = state->opening_keys[index];
  const auto entry = state->engine.lookup(key);
  if (!entry.has_value()) {
    SetWindowTextW(state->wiki_pane, L"Internal wiki entry not found.");
    return;
  }

  const std::string full_text =
      "[" + entry->key + "]\\r\\n" + entry->title + "\\r\\n\\r\\n" + entry->body;

  SetWindowTextW(state->wiki_pane, utf8_to_wide(full_text).c_str());
}

void rebuild_secondary_menu(AppState* state) {
  const std::string parent = selected_combo_text(state->parent_menu);
  const auto secondary = state->engine.secondary_menus(parent);

  SendMessageW(state->secondary_menu, CB_RESETCONTENT, 0, 0);
  for (const auto& secondary_name : secondary) {
    const std::wstring wide_secondary = utf8_to_wide(secondary_name);
    SendMessageW(state->secondary_menu, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(wide_secondary.c_str()));
  }

  if (!secondary.empty()) {
    SendMessageW(state->secondary_menu, CB_SETCURSEL, 0, 0);
  }
}

void rebuild_parent_menu(AppState* state) {
  const auto parents = state->engine.parent_menus();

  SendMessageW(state->parent_menu, CB_RESETCONTENT, 0, 0);
  for (const auto& parent_name : parents) {
    const std::wstring wide_parent = utf8_to_wide(parent_name);
    SendMessageW(state->parent_menu, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide_parent.c_str()));
  }

  if (!parents.empty()) {
    SendMessageW(state->parent_menu, CB_SETCURSEL, 0, 0);
  }

  rebuild_secondary_menu(state);
}

void rebuild_opening_list(AppState* state) {
  const std::wstring search_text = read_window_text(state->search_edit);
  const std::string parent = selected_combo_text(state->parent_menu);
  const std::string secondary = selected_combo_text(state->secondary_menu);

  state->opening_keys = state->engine.openings(parent, secondary, wide_to_utf8(search_text));

  SendMessageW(state->opening_list, LB_RESETCONTENT, 0, 0);

  for (const auto& key : state->opening_keys) {
    std::string display = key;
    const auto entry = state->engine.lookup(key);
    if (entry.has_value()) {
      display = entry->title;
    }

    const std::wstring wide_display = utf8_to_wide(display);
    SendMessageW(state->opening_list, LB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(wide_display.c_str()));
  }

  if (!state->opening_keys.empty()) {
    SendMessageW(state->opening_list, LB_SETCURSEL, 0, 0);
    update_wiki_from_selection(state);
    return;
  }

  SetWindowTextW(state->wiki_pane, L"No openings in this menu path. Change parent/secondary.");
}

void layout_controls(AppState* state, int width, int height) {
  constexpr int margin = 10;
  constexpr int top_row_height = 28;
  constexpr int combo_height = 28;
  constexpr int section_gap = 8;
  constexpr int close_button_width = 90;
  constexpr int left_column_width = 250;

  const int top_y = margin;
  const int search_width = std::max(100, width - (margin * 3) - close_button_width);

  MoveWindow(state->search_edit, margin, top_y, search_width, top_row_height, TRUE);
  MoveWindow(state->close_button, margin * 2 + search_width, top_y, close_button_width,
             top_row_height, TRUE);

  const int body_y = top_y + top_row_height + margin;
  const int body_height = std::max(50, height - body_y - margin);

  MoveWindow(state->parent_menu, margin, body_y, left_column_width, combo_height, TRUE);
  MoveWindow(state->secondary_menu, margin, body_y + combo_height + section_gap, left_column_width,
             combo_height, TRUE);

  const int opening_list_y = body_y + (combo_height * 2) + (section_gap * 2);
  const int opening_list_height = std::max(60, body_height - (combo_height * 2) - (section_gap * 2));
  MoveWindow(state->opening_list, margin, opening_list_y, left_column_width, opening_list_height,
             TRUE);

  const int wiki_x = margin * 2 + left_column_width;
  const int wiki_width = std::max(120, width - wiki_x - margin);
  MoveWindow(state->wiki_pane, wiki_x, body_y, wiki_width, body_height, TRUE);
}

HMENU make_main_menu() {
  HMENU menu_bar = CreateMenu();

  HMENU file_menu = CreatePopupMenu();
  AppendMenuW(file_menu, MF_STRING, kMenuCloseId, L"Close");
  AppendMenuW(menu_bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file_menu), L"File");

  HMENU help_menu = CreatePopupMenu();
  AppendMenuW(help_menu, MF_STRING, kMenuAboutId, L"About / Credits");
  AppendMenuW(menu_bar, MF_POPUP, reinterpret_cast<UINT_PTR>(help_menu), L"Help");

  return menu_bar;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
  auto* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  switch (message) {
    case WM_CREATE: {
      auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(l_param);
      state = reinterpret_cast<AppState*>(create_struct->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

      HFONT ui_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

      state->search_edit =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kSearchEditId),
                          create_struct->hInstance, nullptr);

      state->parent_menu =
          CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kParentMenuId),
                          create_struct->hInstance, nullptr);

      state->secondary_menu =
          CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kSecondaryMenuId),
                          create_struct->hInstance, nullptr);

      state->opening_list = CreateWindowExW(
          WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kOpeningListId), create_struct->hInstance,
          nullptr);

      state->wiki_pane =
          CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                                                           ES_MULTILINE | ES_AUTOVSCROLL |
                                                           ES_READONLY | ES_WANTRETURN,
                          0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(kWikiPaneId),
                          create_struct->hInstance, nullptr);

      state->close_button =
          CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
                        hwnd, reinterpret_cast<HMENU>(kCloseButtonId), create_struct->hInstance,
                        nullptr);

      SendMessageW(state->search_edit, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
      SendMessageW(state->parent_menu, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
      SendMessageW(state->secondary_menu, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
      SendMessageW(state->opening_list, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
      SendMessageW(state->wiki_pane, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
      SendMessageW(state->close_button, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);

      SetMenu(hwnd, make_main_menu());

      RECT rect;
      GetClientRect(hwnd, &rect);
      layout_controls(state, rect.right - rect.left, rect.bottom - rect.top);
      rebuild_parent_menu(state);
      rebuild_opening_list(state);
      return 0;
    }

    case WM_SIZE:
      if (state != nullptr) {
        layout_controls(state, LOWORD(l_param), HIWORD(l_param));
      }
      return 0;

    case WM_COMMAND: {
      const int command_id = LOWORD(w_param);
      const int command_code = HIWORD(w_param);

      if (command_id == kSearchEditId && command_code == EN_CHANGE && state != nullptr) {
        rebuild_opening_list(state);
        return 0;
      }

      if (command_id == kParentMenuId && command_code == CBN_SELCHANGE && state != nullptr) {
        rebuild_secondary_menu(state);
        rebuild_opening_list(state);
        return 0;
      }

      if (command_id == kSecondaryMenuId && command_code == CBN_SELCHANGE && state != nullptr) {
        rebuild_opening_list(state);
        return 0;
      }

      if (command_id == kOpeningListId && command_code == LBN_SELCHANGE && state != nullptr) {
        update_wiki_from_selection(state);
        return 0;
      }

      if (command_id == kCloseButtonId && command_code == BN_CLICKED) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return 0;
      }

      if (command_id == kMenuCloseId) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return 0;
      }

      if (command_id == kMenuAboutId) {
        MessageBoxW(hwnd, [RefPadObjCBridge aboutCredits], L"About RefPad", MB_OK | MB_ICONINFORMATION);
        return 0;
      }

      return DefWindowProcW(hwnd, message, w_param, l_param);
    }

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;

    case WM_NCDESTROY:
      if (state != nullptr) {
        delete state;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
      }
      return 0;

    default:
      return DefWindowProcW(hwnd, message, w_param, l_param);
  }
}

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_command) {
  WNDCLASSW wc{};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = kWindowClassName;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

  if (RegisterClassW(&wc) == 0) {
    return 1;
  }

  auto* state = new AppState();

  HWND hwnd = CreateWindowExW(0, kWindowClassName, [RefPadObjCBridge windowTitle],
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1080, 700,
                              nullptr, nullptr, instance, state);

  if (hwnd == nullptr) {
    delete state;
    return 1;
  }

  ShowWindow(hwnd, show_command);
  UpdateWindow(hwnd);

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  return static_cast<int>(msg.wParam);
}
