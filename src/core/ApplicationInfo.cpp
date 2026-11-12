#include "core/ApplicationInfo.h"

namespace tf::core {

QString applicationName()
{
    return QStringLiteral("TenForge");
}

QString applicationSubtitle()
{
    return QStringLiteral("open universal testing software");
}

QString applicationVersion()
{
    return QStringLiteral(TF_VERSION);
}

QString applicationBuild()
{
    return QStringLiteral(TF_BUILD);
}

} // namespace tf::core
