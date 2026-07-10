// ============================================================================
// Resistor Divider Calculator - Win32 GUI Application
// Platform: MS Visual C++ (Win32 API)
// Description: Built-in E24/E96 resistor tables, automatic R1/R2 selection
// Formula: Vo = Vfb * (1 + R1/R2)
// Features: Supports k/M suffix in resistance inputs, fixed R1 or R2 mode
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "resistor_data.h"
#include "resource.h"
#include <cmath>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "kernel32.lib")

// ============================================================================
// Window / Control IDs
// ============================================================================
#define IDC_EDIT_VO_TARGET     101
#define IDC_EDIT_VFB           102
#define IDC_COMBO_SERIES       103
#define IDC_EDIT_R1_MIN        104
#define IDC_EDIT_R1_MAX        105
#define IDC_EDIT_R2_MIN        106
#define IDC_EDIT_R2_MAX        107
#define IDC_BTN_CALCULATE      108
#define IDC_LIST_RESULTS       109
#define IDC_STATIC_RESULT      110
#define IDC_LIST_TABLE         111
#define IDC_CHK_FIXED_R1       112
#define IDC_EDIT_FIXED_R1      113
#define IDC_CHK_FIXED_R2       114
#define IDC_EDIT_FIXED_R2      115

// ============================================================================
// Application constants
// ============================================================================
constexpr int WINDOW_WIDTH  = 960;
constexpr int WINDOW_HEIGHT = 840;
constexpr int MARGIN = 15;
constexpr int LABEL_H = 22;
constexpr int EDIT_H = 26;
constexpr int BTN_H = 34;

// ============================================================================
// Global variables
// ============================================================================
HINSTANCE g_hInst = nullptr;
HWND g_hWndMain = nullptr;
HFONT g_hFontNormal = nullptr;
HFONT g_hFontBold = nullptr;
HFONT g_hFontMono = nullptr;

// ============================================================================
// Helper: Create font
// ============================================================================
HFONT CreateAppFont(int height, bool bold, bool mono = false) {
    return CreateFontW(height, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | (mono ? FF_MODERN : FF_SWISS),
        mono ? L"Consolas" : L"Segoe UI");
}

// ============================================================================
// Helper: format resistance as short string with k/M suffix (e.g., "1k", "2.2k", "1M")
// ============================================================================
std::wstring FormatResistanceShort(double ohm) {
    wchar_t buf[64];
    if (ohm >= 1e6) {
        double v = ohm / 1e6;
        swprintf_s(buf, 64, L"%.2fM", v);
    } else if (ohm >= 1e3) {
        double v = ohm / 1e3;
        swprintf_s(buf, 64, L"%.2fk", v);
    } else {
        swprintf_s(buf, 64, L"%.0f", ohm);
    }
    // trim trailing zeros like 1.00k -> 1k, 2.20k -> 2.2k
    std::wstring s(buf);
    // remove trailing zeros and dot
    if (s.find(L'.') != std::wstring::npos) {
        while (!s.empty() && s.back() == L'0') s.pop_back();
        if (!s.empty() && s.back() == L'.') s.pop_back();
    }
    return s;
}

// Populate the fixed-value combo boxes with resistor series values
void PopulateFixedCombos(HWND hWnd, ResistorSeries series) {
    HWND hCb1 = GetDlgItem(hWnd, IDC_EDIT_FIXED_R1);
    HWND hCb2 = GetDlgItem(hWnd, IDC_EDIT_FIXED_R2);
    if (!hCb1 || !hCb2) return;
    SendMessageW(hCb1, CB_RESETCONTENT, 0, 0);
    SendMessageW(hCb2, CB_RESETCONTENT, 0, 0);
    std::vector<double> vals = GetResistorSeries(series);
    // Only include values between 1k and 1M
    for (double v : vals) {
        if (v < 1e3 || v > 1e6) continue;
        std::wstring s = FormatResistanceShort(v);
        SendMessageW(hCb1, CB_ADDSTRING, 0, (LPARAM)s.c_str());
        SendMessageW(hCb2, CB_ADDSTRING, 0, (LPARAM)s.c_str());
    }
    // Default select 60.4k if present
    SendMessageW(hCb1, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)L"60.4k");
    SendMessageW(hCb2, CB_SELECTSTRING, (WPARAM)-1, (LPARAM)L"60.4k");
}

// ============================================================================
// Helper: Get text from edit control as double (plain number)
// ============================================================================
double GetEditDouble(HWND hEdit) {
    wchar_t buf[256] = { 0 };
    GetWindowTextW(hEdit, buf, 256);
    return _wtof(buf);
}

// ============================================================================
// Helper: Get text from edit control as resistance (supports k/M suffix)
// ============================================================================
double GetEditResistance(HWND hEdit) {
    wchar_t buf[256] = { 0 };
    GetWindowTextW(hEdit, buf, 256);
    double val = ParseResistanceInput(buf);
    return (val > 0) ? val : 0;
}

// ============================================================================
// Helper: Set edit control text from double
// ============================================================================
void SetEditDouble(HWND hEdit, double val, int precision = 3) {
    wchar_t buf[256] = { 0 };
    std::wstring fmt = L"%." + std::to_wstring(precision) + L"f";
    swprintf_s(buf, 256, fmt.c_str(), val);
    SetWindowTextW(hEdit, buf);
}

// ============================================================================
// Helper: Format a double with specified precision to wstring
// ============================================================================
std::wstring FormatDouble(double val, int precision = 6) {
    wchar_t buf[256] = { 0 };
    std::wstring fmt = L"%." + std::to_wstring(precision) + L"f";
    swprintf_s(buf, 256, fmt.c_str(), val);
    return std::wstring(buf);
}

// ============================================================================
// Helper: Create labeled edit box
// ============================================================================
struct LabeledEdit {
    HWND hLabel;
    HWND hEdit;
};

LabeledEdit CreateLabeledEdit(HWND hParent, int x, int y, int w, int h,
    const wchar_t* label, int id, const wchar_t* initText = L"",
    bool readOnly = false) {
    LabeledEdit le;
    le.hLabel = CreateWindowW(L"STATIC", label,
        WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP,
        x, y, w, LABEL_H, hParent, nullptr, g_hInst, nullptr);
    SendMessageW(le.hLabel, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

    le.hEdit = CreateWindowW(L"EDIT", initText,
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | (readOnly ? ES_READONLY : 0),
        x, y + LABEL_H, w, EDIT_H, hParent, (HMENU)(UINT_PTR)id, g_hInst, nullptr);
    SendMessageW(le.hEdit, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    return le;
}

// ============================================================================
// Create main window controls
// ============================================================================
void CreateMainControls(HWND hWnd) {
    int y = 10;
    // compute three columns inside the input group with inner padding
    int groupLeft = MARGIN;
    int groupWidth = WINDOW_WIDTH - MARGIN * 2 - 40; // leave extra margin to avoid overflow
    int innerPad = 10;
    int innerLeft = groupLeft + innerPad;
    int innerRight = groupLeft + groupWidth - innerPad;
    int colW = (innerRight - innerLeft + innerPad) / 3;

    // === Title ===
    HWND hTitle = CreateWindowW(L"STATIC",
        L"\u26A1 Resistor Divider Calculator (E24 / E96 Series)",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        0, y, WINDOW_WIDTH, 36, hWnd, nullptr, g_hInst, nullptr);
    SendMessageW(hTitle, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
    y += 22;

    // === Formula Card ===
    HWND hFormulaLabel = CreateWindowW(L"STATIC",
        L"Formula: Vo(actual) = Vfb \xD7 (1 + R1 / R2)",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        MARGIN, y, WINDOW_WIDTH - MARGIN * 4, 22, hWnd, nullptr, g_hInst, nullptr);
    SendMessageW(hFormulaLabel, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
    y += 18;

    HWND hFormulaDesc = CreateWindowW(L"STATIC",
        L"R1/R2 supports k/K (x1000) and m/M (x1,000,000) suffixes. E.g. 10k = 10000\u03A9, 1.5M = 1500000\u03A9.",
        WS_VISIBLE | WS_CHILD | SS_LEFT,
        MARGIN, y, WINDOW_WIDTH - MARGIN * 4, 22, hWnd, nullptr, g_hInst, nullptr);
    SendMessageW(hFormulaDesc, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    y += 18;

    // === Input Parameters ===
    HWND hGroupInput = CreateWindowW(L"BUTTON", L" Input Parameters ",
        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
        MARGIN, y, WINDOW_WIDTH - MARGIN * 4, 220, hWnd, nullptr, g_hInst, nullptr);
    SendMessageW(hGroupInput, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    int cx = innerLeft;
    int cw = colW - 40; // main column control width
    // compute field widths for min/max inputs so fixed controls can align
    int gap = 10;
    int totalAvail = innerRight - innerLeft;
    int fieldW = (totalAvail - gap * 3) / 4;
    // Evenly distribute controls vertically inside the Input Parameters group
    const int groupH = 220;
    int gyStart = y + 22; // inner top padding
    const int rows = 4; // target row, fixed row, min/max row, action row
    int rowSpacing = (groupH - 40) / (rows - 1); // spacing between rows
    int rowY0 = gyStart;
    // Move fixed/min-max/button rows down by 20 (overall) compared to previous layout
    int rowY1 = rowY0 + 60; // fixed controls row (moved down)
    int rowY2 = rowY1 + 28; // min/max row (closer to fixed)
    int rowY3 = rowY2 + 60; // action/button row (moved down)

    // Row 0: Target Vo, Vfb, Series
    CreateLabeledEdit(hWnd, cx, rowY0, cw, EDIT_H,
        L"Target Vo (V):", IDC_EDIT_VO_TARGET, L"3.300");
    CreateLabeledEdit(hWnd, cx + colW, rowY0, cw, EDIT_H,
        L"Feedback Vfb (V):", IDC_EDIT_VFB, L"0.600");

    // Series combo
    HWND hSeriesLabel = CreateWindowW(L"STATIC", L"Resistor Series:",
        WS_VISIBLE | WS_CHILD | SS_LEFTNOWORDWRAP,
        cx + colW * 2, rowY0, cw, LABEL_H, hWnd, nullptr, g_hInst, nullptr);
    SendMessageW(hSeriesLabel, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    HWND hCombo = CreateWindowW(L"COMBOBOX", L"",
        WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,
        cx + colW * 2, rowY0 + LABEL_H, cw, 200, hWnd, (HMENU)(UINT_PTR)IDC_COMBO_SERIES, g_hInst, nullptr);
    SendMessageW(hCombo, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"E96 (1% precision, 96 values)");
    SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)L"E24 (5% precision, 24 values)");
    SendMessageW(hCombo, CB_SETCURSEL, 0, 0);

    // Fixed R1/R2 controls placed between Target row and Min/Max row (aligned to R1/R2 columns)
    int xField0 = cx + (fieldW + gap) * 0;
    int xField2 = cx + (fieldW + gap) * 2;
    int gyFixedTop = rowY1; // use second row

    HWND hChkR1 = CreateWindowW(L"BUTTON", L"Fixed R1:",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        xField0, gyFixedTop, 120, LABEL_H, hWnd, (HMENU)(UINT_PTR)IDC_CHK_FIXED_R1, g_hInst, nullptr);
    SendMessageW(hChkR1, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    HWND hComboFixR1 = CreateWindowW(L"COMBOBOX", L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL,
        xField0 + 125, gyFixedTop + 2, fieldW - 20, 300, hWnd, (HMENU)(UINT_PTR)IDC_EDIT_FIXED_R1, g_hInst, nullptr);
    SendMessageW(hComboFixR1, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    EnableWindow(hComboFixR1, FALSE);

    HWND hChkR2 = CreateWindowW(L"BUTTON", L"Fixed R2:",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        xField2, gyFixedTop, 120, LABEL_H, hWnd, (HMENU)(UINT_PTR)IDC_CHK_FIXED_R2, g_hInst, nullptr);
    SendMessageW(hChkR2, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    HWND hComboFixR2 = CreateWindowW(L"COMBOBOX", L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL,
        xField2 + 125, gyFixedTop + 2, fieldW - 20, 300, hWnd, (HMENU)(UINT_PTR)IDC_EDIT_FIXED_R2, g_hInst, nullptr);
    SendMessageW(hComboFixR2, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    EnableWindow(hComboFixR2, FALSE);

    // (Fixed R1/R2 controls moved closer to Min/Max inputs below)

    // Row 2: R1 Min/Max, R2 Min/Max - place all four on one row, evenly distributed
    CreateLabeledEdit(hWnd, cx + (fieldW + gap) * 0, rowY2, fieldW - 40, EDIT_H,
        L"R1 Min (Ohm):", IDC_EDIT_R1_MIN, L"1000");
    CreateLabeledEdit(hWnd, cx + (fieldW + gap) * 1, rowY2, fieldW - 40, EDIT_H,
        L"R1 Max (Ohm):", IDC_EDIT_R1_MAX, L"1M");
    CreateLabeledEdit(hWnd, cx + (fieldW + gap) * 2, rowY2, fieldW - 40, EDIT_H,
        L"R2 Min (Ohm):", IDC_EDIT_R2_MIN, L"1000");
    CreateLabeledEdit(hWnd, cx + (fieldW + gap) * 3, rowY2, fieldW - 40, EDIT_H,
        L"R2 Max (Ohm):", IDC_EDIT_R2_MAX, L"1M");


    // Calculate Button (placed under the same area)
    int btnY = rowY3;
    HWND hBtn = CreateWindowW(L"BUTTON", L"\U0001F50D Calculate Best Match",
        WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        cx + colW * 2 , btnY, cw, BTN_H, hWnd, (HMENU)(UINT_PTR)IDC_BTN_CALCULATE, g_hInst, nullptr);
    SendMessageW(hBtn, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    y += 230; // moved up 20px (was 250)

    // === Best Result Card ===
    HWND hGroupResult = CreateWindowW(L"BUTTON", L" Best Match Result ",
        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
        MARGIN, y, WINDOW_WIDTH - MARGIN * 4, 170, hWnd, nullptr, g_hInst, nullptr);
    SendMessageW(hGroupResult, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    HWND hResult = CreateWindowW(L"EDIT", L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | ES_MULTILINE | ES_READONLY |
        WS_VSCROLL | ES_AUTOVSCROLL,
        MARGIN + 10, y + 22, groupWidth - 10, 140,
        hWnd, (HMENU)(UINT_PTR)IDC_STATIC_RESULT, g_hInst, nullptr);
    SendMessageW(hResult, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);

    y += 178;

    // === Top Candidates List ===
    HWND hGroupCandidates = CreateWindowW(L"BUTTON", L" Top 10 Candidate Combinations ",
        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
        MARGIN, y, WINDOW_WIDTH - MARGIN * 4, 200, hWnd, nullptr, g_hInst, nullptr);
    SendMessageW(hGroupCandidates, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    HWND hList = CreateWindowW(L"LISTBOX", L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER | LBS_NOTIFY |
        WS_VSCROLL | LBS_EXTENDEDSEL,
        MARGIN + 10, y + 22, groupWidth - 20, 170,
        hWnd, (HMENU)(UINT_PTR)IDC_LIST_RESULTS, g_hInst, nullptr);
    SendMessageW(hList, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);

    y += 208;

    // === Resistor Table ===
    HWND hGroupTable = CreateWindowW(L"BUTTON", L" Resistor Series Table (E96 / E24) ",
        WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
        MARGIN, y, WINDOW_WIDTH - MARGIN * 4, 160, hWnd, nullptr, g_hInst, nullptr);
    SendMessageW(hGroupTable, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

    HWND hTable = CreateWindowW(L"LISTBOX", L"",
        WS_VISIBLE | WS_CHILD | WS_BORDER |
        WS_VSCROLL | LBS_EXTENDEDSEL,
        MARGIN + 10, y + 22, groupWidth - 20, 130,
        hWnd, (HMENU)(UINT_PTR)IDC_LIST_TABLE, g_hInst, nullptr);
    SendMessageW(hTable, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
}

// ============================================================================
// Perform calculation and update UI
// ============================================================================
void DoCalculation(HWND hWnd) {
    double voTarget = GetEditDouble(GetDlgItem(hWnd, IDC_EDIT_VO_TARGET));
    double vfb = GetEditDouble(GetDlgItem(hWnd, IDC_EDIT_VFB));
    int seriesIdx = (int)SendMessageW(GetDlgItem(hWnd, IDC_COMBO_SERIES), CB_GETCURSEL, 0, 0);
    ResistorSeries series = (seriesIdx == 0) ? ResistorSeries::E96 : ResistorSeries::E24;

    // Fixed values (parse with k/M support)
    double fixedR1 = 0, fixedR2 = 0;
    bool useFixedR1 = (IsDlgButtonChecked(hWnd, IDC_CHK_FIXED_R1) == BST_CHECKED);
    bool useFixedR2 = (IsDlgButtonChecked(hWnd, IDC_CHK_FIXED_R2) == BST_CHECKED);

    if (useFixedR1) {
        fixedR1 = GetEditResistance(GetDlgItem(hWnd, IDC_EDIT_FIXED_R1));
        if (fixedR1 <= 0) {
            MessageBoxW(hWnd, L"Invalid Fixed R1 value! Use format like 10k, 1.5M, or 1000", L"Input Error", MB_OK | MB_ICONWARNING);
            return;
        }
    }
    if (useFixedR2) {
        fixedR2 = GetEditResistance(GetDlgItem(hWnd, IDC_EDIT_FIXED_R2));
        if (fixedR2 <= 0) {
            MessageBoxW(hWnd, L"Invalid Fixed R2 value! Use format like 10k, 1.5M, or 1000", L"Input Error", MB_OK | MB_ICONWARNING);
            return;
        }
    }

    // Range values (support k/M suffixes)
    double r1Min = GetEditResistance(GetDlgItem(hWnd, IDC_EDIT_R1_MIN));
    double r1Max = GetEditResistance(GetDlgItem(hWnd, IDC_EDIT_R1_MAX));
    double r2Min = GetEditResistance(GetDlgItem(hWnd, IDC_EDIT_R2_MIN));
    double r2Max = GetEditResistance(GetDlgItem(hWnd, IDC_EDIT_R2_MAX));

    // Validate
    if (voTarget <= 0 || vfb <= 0) {
        MessageBoxW(hWnd, L"Please enter valid positive voltage values!", L"Input Error", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!useFixedR1 && (r1Min <= 0 || r1Max <= 0)) {
        MessageBoxW(hWnd, L"Please enter valid R1 range values!", L"Input Error", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!useFixedR2 && (r2Min <= 0 || r2Max <= 0)) {
        MessageBoxW(hWnd, L"Please enter valid R2 range values!", L"Input Error", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!useFixedR1 && r1Min > r1Max) {
        MessageBoxW(hWnd, L"R1 Min must be <= R1 Max!", L"Input Error", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!useFixedR2 && r2Min > r2Max) {
        MessageBoxW(hWnd, L"R2 Min must be <= R2 Max!", L"Input Error", MB_OK | MB_ICONWARNING);
        return;
    }

    std::vector<ResistorResult> candidates;
    ResistorResult best = CalculateBestResistors(
        voTarget, vfb, series, r1Min, r1Max, r2Min, r2Max,
        fixedR1, fixedR2, candidates, 10);

    // Update best result text
    HWND hResult = GetDlgItem(hWnd, IDC_STATIC_RESULT);
    std::wstring resultText;
    resultText += L"Best Match:\r\n";
    resultText += L"  R1 (pull-up)  = " + FormatResistance(best.r1) + L"\r\n";
    resultText += L"  R2 (pull-down)= " + FormatResistance(best.r2) + L"\r\n";
    resultText += L"  Vo(actual)    = " + FormatVoltage(best.voActual) + L"\r\n";
    resultText += L"  Vo(target)    = " + FormatVoltage(voTarget) + L"\r\n";
    resultText += L"  Abs Error     = " + FormatVoltage(best.errorAbs) + L"\r\n";
    resultText += L"  Rel Error     = " + FormatPercent(best.errorPct) + L"\r\n";
    resultText += L"  Ratio R1/R2   = " + FormatDouble(best.r1 / best.r2, 6) + L"\r\n";
    resultText += L"  Target Ratio  = " + FormatDouble(voTarget / vfb - 1.0, 6);

    if (useFixedR1 && useFixedR2) {
        resultText += L"\r\n\r\n[Both R1 and R2 are fixed values - no search performed.]";
    } else if (useFixedR1) {
        resultText += L"\r\n\r\n[R1 is fixed. R2 is auto-selected from " + std::wstring(series == ResistorSeries::E96 ? L"E96" : L"E24") + L" series.]";
    } else if (useFixedR2) {
        resultText += L"\r\n\r\n[R2 is fixed. R1 is auto-selected from " + std::wstring(series == ResistorSeries::E96 ? L"E96" : L"E24") + L" series.]";
    } else {
        resultText += L"\r\n\r\n[Both R1 and R2 are auto-selected from " + std::wstring(series == ResistorSeries::E96 ? L"E96" : L"E24") + L" series.]";
    }

    SetWindowTextW(hResult, resultText.c_str());

    // Update candidates list
    HWND hList = GetDlgItem(hWnd, IDC_LIST_RESULTS);
    SendMessageW(hList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < candidates.size(); ++i) {
        const auto& c = candidates[i];
        wchar_t buf[512];
        swprintf_s(buf, L"#%2zu  R1=%10s  R2=%10s  Vo=%10s  err=%s",
            i + 1,
            FormatResistance(c.r1).c_str(),
            FormatResistance(c.r2).c_str(),
            FormatVoltage(c.voActual).c_str(),
            FormatPercent(c.errorPct).c_str());
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)buf);
    }
}

// ============================================================================
// Populate resistor table list
// ============================================================================
void PopulateResistorTable(HWND hWnd, ResistorSeries series) {
    HWND hTable = GetDlgItem(hWnd, IDC_LIST_TABLE);
    SendMessageW(hTable, LB_RESETCONTENT, 0, 0);

    const double* baseValues = (series == ResistorSeries::E96) ? E96_BASE : E24_BASE;
    int count = (series == ResistorSeries::E96) ? E96_COUNT : E24_COUNT;

    // Header
    SendMessageW(hTable, LB_ADDSTRING, 0, (LPARAM)L"Base   x1         x10        x100       x1k        x10k       x100k      x1M");
    SendMessageW(hTable, LB_ADDSTRING, 0, (LPARAM)L"-----------------------------------------------------------------------------------------");

    for (int i = 0; i < count; ++i) {
        double base = baseValues[i];
        wchar_t buf[512];
        wchar_t cols[8][32];
        for (int e = 0; e <= 6; ++e) {
            double r = base * std::pow(10.0, e);
            std::wstring s = FormatResistance(r);
            wcsncpy_s(cols[e], s.c_str(), 31);
        }
        swprintf_s(buf, L"%-5.2f  %-9s  %-9s  %-9s  %-9s  %-9s  %-9s  %-9s",
            base, cols[0], cols[1], cols[2], cols[3], cols[4], cols[5], cols[6]);
        SendMessageW(hTable, LB_ADDSTRING, 0, (LPARAM)buf);
    }
}

// ============================================================================
// Window Procedure
// ============================================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        CreateMainControls(hWnd);
        PopulateResistorTable(hWnd, ResistorSeries::E96);
        PopulateFixedCombos(hWnd, ResistorSeries::E96);
        // Auto-calculate once
        DoCalculation(hWnd);
        return 0;
    }

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmNotify = HIWORD(wParam);

        if (wmId == IDC_BTN_CALCULATE) {
            DoCalculation(hWnd);
            return 0;
        }
        if (wmId == IDC_COMBO_SERIES && wmNotify == CBN_SELCHANGE) {
            int sel = (int)SendMessageW(GetDlgItem(hWnd, IDC_COMBO_SERIES), CB_GETCURSEL, 0, 0);
            ResistorSeries s = (sel == 0) ? ResistorSeries::E96 : ResistorSeries::E24;
            PopulateResistorTable(hWnd, s);
            PopulateFixedCombos(hWnd, s);
            return 0;
        }
        if (wmId == IDC_CHK_FIXED_R1) {
            BOOL checked = IsDlgButtonChecked(hWnd, IDC_CHK_FIXED_R1);
            EnableWindow(GetDlgItem(hWnd, IDC_EDIT_FIXED_R1), checked);
            EnableWindow(GetDlgItem(hWnd, IDC_EDIT_R1_MIN), !checked);
            EnableWindow(GetDlgItem(hWnd, IDC_EDIT_R1_MAX), !checked);

            return 0;
        }
        if (wmId == IDC_CHK_FIXED_R2) {
            BOOL checked = IsDlgButtonChecked(hWnd, IDC_CHK_FIXED_R2);
            EnableWindow(GetDlgItem(hWnd, IDC_EDIT_FIXED_R2), checked);
            EnableWindow(GetDlgItem(hWnd, IDC_EDIT_R2_MIN), !checked);
            EnableWindow(GetDlgItem(hWnd, IDC_EDIT_R2_MAX), !checked);

            return 0;
        }
        return 0;
    }

    case WM_SIZE: {
        // Optional: resize controls on window resize
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
}

// ============================================================================
// WinMain - Application Entry Point
// ============================================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPWSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;

    // Initialize common controls
    INITCOMMONCONTROLSEX iccex = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&iccex);

    // Create larger, clearer application fonts
    g_hFontNormal = CreateAppFont(18, false);
    g_hFontBold = CreateAppFont(20, true);
    g_hFontMono = CreateAppFont(16, false, true);

    // Register window class
    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    // Load embedded icon resource (resistor.ico compiled into resources)
    wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    if (!wcex.hIcon) wcex.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszClassName = L"ResistorCalcClass";
    // small icon
    wcex.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    if (!wcex.hIconSm) wcex.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

    if (!RegisterClassExW(&wcex)) {
        MessageBoxW(nullptr, L"Window Registration Failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Calculate centered position
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - WINDOW_WIDTH) / 2;
    int y = (screenH - WINDOW_HEIGHT) / 2;

    // Create main window
    g_hWndMain = CreateWindowExW(
        WS_EX_CLIENTEDGE | WS_EX_OVERLAPPEDWINDOW,
        L"ResistorCalcClass",
        L"Resistor Divider Calculator - E24 / E96 Series",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
        x, y, WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_hWndMain) {
        MessageBoxW(nullptr, L"Window Creation Failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hWndMain, nCmdShow);
    UpdateWindow(g_hWndMain);

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup
    DeleteObject(g_hFontNormal);
    DeleteObject(g_hFontBold);
    DeleteObject(g_hFontMono);

    return (int)msg.wParam;
}
