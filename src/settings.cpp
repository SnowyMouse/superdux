#include "settings.hpp"

QSettings get_superdux_settings() {
    return QSettings(QSettings::Format::IniFormat, QSettings::Scope::UserScope, "SnowyMouse", "SuperDUX");
}
