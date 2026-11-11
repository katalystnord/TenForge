#include "core/ApplicationInfo.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QTextStream>

// tf-analyse: TenForge's analysis without a window, for scripts and tests.
//
// In Phase 0 it has no profiles to run, and says so plainly rather than
// pretending: a file given to it is refused with the release that will
// accept it.

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("tf-analyse"));
    QCoreApplication::setApplicationVersion(tf::core::applicationVersion());

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("%1 - %2, headless analysis.")
            .arg(tf::core::applicationName(), tf::core::applicationSubtitle()));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"),
                                 QStringLiteral("A CSV file or test record to analyse."));
    parser.process(app);

    if (!parser.positionalArguments().isEmpty()) {
        QTextStream(stderr)
            << "tf-analyse: analysis is not available in this build yet; "
               "it arrives in release 0.1. Nothing was read.\n";
        return 2;
    }

    QTextStream(stderr) << parser.helpText();
    return 2;
}
