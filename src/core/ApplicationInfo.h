#pragma once

#include <QString>

// The application's name and what it is, said once. The window title, the
// command line and every file TenForge writes take them from here, so the
// subtitle a reader first meets is the same everywhere.

namespace tf::core {

QString applicationName();      // "TenForge"
QString applicationSubtitle();  // "open universal testing software"
QString applicationVersion();
QString applicationBuild();      // git describe at configure time; "unknown" outside git

} // namespace tf::core
