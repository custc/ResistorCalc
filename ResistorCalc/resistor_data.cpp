#include "resistor_data.h"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

// ============================================================================
// Generate full resistor value list from base values
// Range: 1 ohm ~ 10M ohm (7 decades: 10^0 ~ 10^6)
// ============================================================================
std::vector<double> GenerateResistorValues(const double baseValues[], int count) {
    std::vector<double> values;
    values.reserve(count * 7);
    for (int exp = 0; exp <= 6; ++exp) {
        double multiplier = std::pow(10.0, exp);
        for (int i = 0; i < count; ++i) {
            double r = baseValues[i] * multiplier;
            // Round to avoid floating point artifacts
            if (r >= 100.0) {
                r = std::round(r);
            } else if (r >= 10.0) {
                r = std::round(r * 10.0) / 10.0;
            } else {
                r = std::round(r * 100.0) / 100.0;
            }
            values.push_back(r);
        }
    }
    return values;
}

// ============================================================================
// Get resistor values for specified series
// ============================================================================
std::vector<double> GetResistorSeries(ResistorSeries series) {
    if (series == ResistorSeries::E96) {
        return GenerateResistorValues(E96_BASE, E96_COUNT);
    } else {
        return GenerateResistorValues(E24_BASE, E24_COUNT);
    }
}

// ============================================================================
// Parse resistance input string with unit suffix support
// e.g. "10k" -> 10000, "1.5M" -> 1500000, "1000" -> 1000, "0.1k" -> 100
// Returns -1.0 if parsing fails
// ============================================================================
double ParseResistanceInput(const std::wstring& input) {
    if (input.empty()) return -1.0;

    // Trim whitespace
    size_t start = 0;
    while (start < input.size() && iswspace(input[start])) ++start;
    size_t end = input.size();
    while (end > start && iswspace(input[end - 1])) --end;
    std::wstring trimmed = input.substr(start, end - start);
    if (trimmed.empty()) return -1.0;

    // Find last character that is part of the number
    size_t numEnd = trimmed.size();
    wchar_t last = trimmed.back();
    double multiplier = 1.0;

    if (last == L'k' || last == L'K') {
        multiplier = 1e3;
        numEnd = trimmed.size() - 1;
    } else if (last == L'm' || last == L'M') {
        multiplier = 1e6;
        numEnd = trimmed.size() - 1;
    } else if (!iswdigit(last) && last != L'.') {
        return -1.0; // Invalid suffix
    }

    if (numEnd == 0) return -1.0;

    std::wstring numPart = trimmed.substr(0, numEnd);

    // Validate numPart contains only digits or dot
    bool hasDot = false;
    for (size_t i = 0; i < numPart.size(); ++i) {
        wchar_t c = numPart[i];
        if (c == L'.') {
            if (hasDot) return -1.0;
            hasDot = true;
        } else if (!iswdigit(c)) {
            return -1.0;
        }
    }

    try {
        double val = std::stod(numPart);
        return val * multiplier;
    } catch (...) {
        return -1.0;
    }
}

// ============================================================================
// Calculate the best R1/R2 combination
// Supports fixed R1 or fixed R2 mode
// fixedR1 > 0  : R1 is fixed, only R2 is searched
// fixedR2 > 0  : R2 is fixed, only R1 is searched
// Both <= 0    : Original dual-search
// ============================================================================
ResistorResult CalculateBestResistors(
    double voTarget,
    double vfb,
    ResistorSeries series,
    double r1Min,
    double r1Max,
    double r2Min,
    double r2Max,
    double fixedR1,
    double fixedR2,
    std::vector<ResistorResult>& topCandidates,
    int topN)
{
    std::vector<double> resistors = GetResistorSeries(series);

    ResistorResult best{};
    best.errorAbs = 1e300;
    topCandidates.clear();

    bool r1Fixed = (fixedR1 > 0);
    bool r2Fixed = (fixedR2 > 0);

    if (r1Fixed && r2Fixed) {
        // Both fixed - just calculate Vo
        double voActual = vfb * (1.0 + fixedR1 / fixedR2);
        double errorAbs = std::abs(voActual - voTarget);
        double errorPct = (errorAbs / voTarget) * 100.0;
        best = ResistorResult{ fixedR1, fixedR2, voActual, errorAbs, errorPct };
        topCandidates.push_back(best);
        return best;
    }

    if (r1Fixed && !r2Fixed) {
        // R1 fixed, search R2 from series
        for (double r2 : resistors) {
            if (r2 < r2Min || r2 > r2Max) continue;
            double voActual = vfb * (1.0 + fixedR1 / r2);
            double errorAbs = std::abs(voActual - voTarget);
            double errorPct = (errorAbs / voTarget) * 100.0;
            ResistorResult res{ fixedR1, r2, voActual, errorAbs, errorPct };
            if (errorAbs < best.errorAbs) best = res;
            topCandidates.push_back(res);
        }
    } else if (r2Fixed && !r1Fixed) {
        // R2 fixed, search R1 from series
        for (double r1 : resistors) {
            if (r1 < r1Min || r1 > r1Max) continue;
            double voActual = vfb * (1.0 + r1 / fixedR2);
            double errorAbs = std::abs(voActual - voTarget);
            double errorPct = (errorAbs / voTarget) * 100.0;
            ResistorResult res{ r1, fixedR2, voActual, errorAbs, errorPct };
            if (errorAbs < best.errorAbs) best = res;
            topCandidates.push_back(res);
        }
    } else {
        // Both auto - original dual search
        std::vector<double> r1List;
        std::vector<double> r2List;
        for (double r : resistors) {
            if (r >= r1Min && r <= r1Max) r1List.push_back(r);
            if (r >= r2Min && r <= r2Max) r2List.push_back(r);
        }
        for (double r1 : r1List) {
            for (double r2 : r2List) {
                double voActual = vfb * (1.0 + r1 / r2);
                double errorAbs = std::abs(voActual - voTarget);
                double errorPct = (errorAbs / voTarget) * 100.0;
                ResistorResult res{ r1, r2, voActual, errorAbs, errorPct };
                if (errorAbs < best.errorAbs) best = res;
                topCandidates.push_back(res);
            }
        }
    }

    // Sort by error ascending
    std::sort(topCandidates.begin(), topCandidates.end(),
        [](const ResistorResult& a, const ResistorResult& b) {
            return a.errorAbs < b.errorAbs;
        });

    // Keep only top N
    if ((int)topCandidates.size() > topN) {
        topCandidates.resize(topN);
    }

    return best;
}

// ============================================================================
// Format resistance value for display
// ============================================================================
std::wstring FormatResistance(double ohm) {
    std::wostringstream oss;
    oss << std::fixed;
    if (ohm >= 1e6) {
        oss << std::setprecision(2) << (ohm / 1e6) << L" M\u03A9";
    } else if (ohm >= 1e3) {
        oss << std::setprecision(2) << (ohm / 1e3) << L" k\u03A9";
    } else {
        oss << std::setprecision(2) << ohm << L" \u03A9";
    }
    std::wstring s = oss.str();
    // Remove trailing .00
    size_t pos = s.find(L".00");
    if (pos != std::wstring::npos) {
        s.erase(pos, 3);
    }
    return s;
}

// ============================================================================
// Format voltage for display
// ============================================================================
std::wstring FormatVoltage(double v) {
    std::wostringstream oss;
    oss << std::fixed << std::setprecision(6) << v << L" V";
    return oss.str();
}

// ============================================================================
// Format percentage for display
// ============================================================================
std::wstring FormatPercent(double pct) {
    std::wostringstream oss;
    oss << std::fixed << std::setprecision(4) << pct << L" %";
    return oss.str();
}
