#include <QApplication>
#include "settings.hpp"

QSettings get_superdux_settings() {
#ifdef _WIN32
    return QSettings(QCoreApplication::applicationDirPath().append("\\SuperDUX.ini"), QSettings::Format::IniFormat);
#else
    return QSettings(QSettings::Format::IniFormat, QSettings::Scope::UserScope, "SnowyMouse", "SuperDUX");
#endif
}
