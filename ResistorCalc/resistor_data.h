#pragma once

#include <vector>
#include <string>

// ============================================================================
// E24 Series Base Values (5% tolerance, 24 values per decade)
// ============================================================================
const double E24_BASE[] = {
    1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0,
    2.2, 2.4, 2.7, 3.0, 3.3, 3.6, 3.9, 4.3,
    4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1
};
const int E24_COUNT = 24;

// ============================================================================
// E96 Series Base Values (1% tolerance, 96 values per decade)
// ============================================================================
const double E96_BASE[] = {
    1.00, 1.02, 1.05, 1.07, 1.10, 1.13, 1.15, 1.18,
    1.21, 1.24, 1.27, 1.30, 1.33, 1.37, 1.40, 1.43,
    1.47, 1.50, 1.54, 1.58, 1.62, 1.65, 1.69, 1.74,
    1.78, 1.82, 1.87, 1.91, 1.96, 2.00, 2.05, 2.10,
    2.15, 2.21, 2.26, 2.32, 2.37, 2.43, 2.49, 2.55,
    2.61, 2.67, 2.74, 2.80, 2.87, 2.94, 3.01, 3.09,
    3.16, 3.24, 3.32, 3.40, 3.48, 3.57, 3.65, 3.74,
    3.83, 3.92, 4.02, 4.12, 4.22, 4.32, 4.42, 4.53,
    4.64, 4.75, 4.87, 4.99, 5.11, 5.23, 5.36, 5.49,
    5.62, 5.76, 5.90, 6.04, 6.19, 6.34, 6.49, 6.65,
    6.81, 6.98, 7.15, 7.32, 7.50, 7.68, 7.87, 8.06,
    8.25, 8.45, 8.66, 8.87, 9.09, 9.31, 9.53, 9.76
};
const int E96_COUNT = 96;

// ============================================================================
// Resistor series types
// ============================================================================
enum class ResistorSeries {
    E24,
    E96
};

// ============================================================================
// Candidate result structure
// ============================================================================
struct ResistorResult {
    double r1;           // R1 value in ohms
    double r2;           // R2 value in ohms
    double voActual;     // Actual output voltage
    double errorAbs;     // Absolute error (V)
    double errorPct;     // Relative error (%)
};

// ============================================================================
// Function declarations
// ============================================================================

// Generate full resistor value list from base values (1 ohm ~ 10M ohm)
std::vector<double> GenerateResistorValues(const double baseValues[], int count);

// Get resistor values for specified series
std::vector<double> GetResistorSeries(ResistorSeries series);

// Calculate the best R1/R2 combination to match target Vo
// fixedR1 > 0  ->  R1 is fixed, only R2 is searched from series
// fixedR2 > 0  ->  R2 is fixed, only R1 is searched from series
// If both are <=0, original dual-search is performed
ResistorResult CalculateBestResistors(
    double voTarget,
    double vfb,
    ResistorSeries series,
    double r1Min,
    double r1Max,
    double r2Min,
    double r2Max,
    double fixedR1,          // <=0 means auto-search R1
    double fixedR2,          // <=0 means auto-search R2
    std::vector<ResistorResult>& topCandidates,
    int topN = 10
);

// Parse resistance input string supporting k/K/m/M suffixes
// e.g. "10k" -> 10000, "1.5M" -> 1500000, "1000" -> 1000, "0.1k" -> 100
// Returns parsed value, or -1 if invalid
double ParseResistanceInput(const std::wstring& input);
// Returns the best result and fills topCandidates with top N candidates
ResistorResult CalculateBestResistors(
    double voTarget,       // Target output voltage (V)
    double vfb,            // Feedback voltage (V)
    ResistorSeries series, // E24 or E96
    double r1Min,          // R1 minimum (ohm)
    double r1Max,          // R1 maximum (ohm)
    double r2Min,          // R2 minimum (ohm)
    double r2Max,          // R2 maximum (ohm)
    std::vector<ResistorResult>& topCandidates, // Output: top candidates
    int topN = 10          // Number of top candidates to return
);

// Format resistance value for display (e.g., "1.50 kΩ", "100 Ω")
std::wstring FormatResistance(double ohm);

// Format voltage for display
std::wstring FormatVoltage(double v);

// Format percentage for display
std::wstring FormatPercent(double pct);
