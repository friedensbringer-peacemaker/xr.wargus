// Diagnose-Logging für den Quest-Port.
//
// Jede Zeile geht nach logcat (Tag "xr-stratagus" bzw. "Stratagus" für die Engine-Ausgabe)
// und zusätzlich in eine Datei unter files/logs/ (app-intern, per scripts/quest.sh logs abrufbar).
// Pro App-Start eine Datei xr-JJJJMMTT-HHMMSS.log; die letzten 10 bleiben erhalten.
// Zeilenformat: "HH:MM:SS.mmm +Sekunden.seit.Start [Stufe] Bereich: Text".
#pragma once

namespace xrs {

enum class LogLevel { kDebug, kInfo, kWarn, kError };

void logf(LogLevel level, const char* area, const char* fmt, ...) __attribute__((format(printf, 3, 4)));

// Ausgabe der Engine (stdout/stderr), zeilenweise.
void logEngineLine(const char* line);

// Ausführliches Eingabeprotokoll (jeder Klick, jede Taste) an/aus – über das VR-Menü.
bool inputLogging();
void setInputLogging(bool enabled);

const char* logFilePath();

}  // namespace xrs

#define XLOGD(area, ...) ::xrs::logf(::xrs::LogLevel::kDebug, area, __VA_ARGS__)
#define XLOGI(area, ...) ::xrs::logf(::xrs::LogLevel::kInfo, area, __VA_ARGS__)
#define XLOGW(area, ...) ::xrs::logf(::xrs::LogLevel::kWarn, area, __VA_ARGS__)
#define XLOGE(area, ...) ::xrs::logf(::xrs::LogLevel::kError, area, __VA_ARGS__)
