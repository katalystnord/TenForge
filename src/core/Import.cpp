#include "core/Import.h"

#include <QCryptographicHash>
#include <QFile>
#include <QRegularExpression>
#include <QStringDecoder>

#include <cmath>
#include <map>

namespace tf::core {

namespace {

QString unquote(QString f)
{
    f = f.trimmed();
    if (f.size() >= 2 && f.front() == '"' && f.back() == '"')
        f = f.mid(1, f.size() - 2).replace("\"\"", "\"");
    return f.trimmed();
}

QStringList fields(const QString &line, QChar delim)
{
    // Quotes are honoured so a quoted field may contain the delimiter.
    QStringList out;
    QString cur;
    bool quoted = false;
    for (QChar c : line) {
        if (c == '"')
            quoted = !quoted;
        if (c == delim && !quoted) {
            out << unquote(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out << unquote(cur);
    return out;
}

// A number in the file's own notation; empty cells are NaN, and anything
// else that is not a number is no number at all.
std::optional<double> number(const QString &f, QChar decimal)
{
    if (f.isEmpty())
        return std::nan("");
    QString s = f;
    if (decimal == ',') {
        if (s.contains('.'))
            return std::nullopt;  // a point in a decimal-comma file is not ours to guess
        s.replace(',', '.');
    }
    bool ok = false;
    const double v = s.toDouble(&ok);  // QString::toDouble is locale-independent
    if (!ok)
        return std::nullopt;
    return v;
}

bool numericRow(const QStringList &fs, QChar decimal)
{
    if (fs.size() < 2)
        return false;
    bool any = false;
    for (const QString &f : fs) {
        const auto v = number(f, decimal);
        if (!v)
            return false;
        any = any || !std::isnan(*v);
    }
    return any;
}

QString stripBrackets(QString u)
{
    u = u.trimmed();
    if (u.size() >= 2 && ((u.front() == '(' && u.back() == ')') || (u.front() == '[' && u.back() == ']')))
        u = u.mid(1, u.size() - 2).trimmed();
    return u;
}

struct Dialect {
    QChar delim, decimal;
    int first = -1, count = 0, width = 0;  // the data block
};

Dialect bestDialect(const QStringList &lines)
{
    const std::vector<std::pair<QChar, QChar>> candidates{
        {';', '.'}, {';', ','}, {'\t', '.'}, {'\t', ','}, {',', '.'}};
    Dialect best;
    for (const auto &[d, dec] : candidates) {
        // The longest run of consecutive numeric rows of one width.
        int runStart = -1, runLen = 0, runWidth = 0;
        for (int i = 0; i <= lines.size(); ++i) {
            const QStringList fs = i < lines.size() ? fields(lines[i], d) : QStringList();
            const bool num = i < lines.size() && numericRow(fs, dec);
            if (num && runLen > 0 && fs.size() == runWidth) {
                ++runLen;
                continue;
            }
            if (runLen > best.count)
                best = Dialect{d, dec, runStart, runLen, runWidth};
            if (num) {
                runStart = i;
                runLen = 1;
                runWidth = int(fs.size());
            } else {
                runLen = 0;
            }
        }
    }
    return best;
}

const std::map<QString, std::map<QString, double>> &unitTable()
{
    static const std::map<QString, std::map<QString, double>> t{
        {"force", {{"N", 1}, {"kN", 1e3}, {"MN", 1e6}, {"mN", 1e-3},
                   {"lbf", 4.4482216152605}, {"kgf", 9.80665}}},
        {"length", {{"m", 1}, {"mm", 1e-3}, {"cm", 1e-2}, {"um", 1e-6}, {"µm", 1e-6},
                    {"in", 0.0254}}},
        {"area", {{"m2", 1}, {"m^2", 1}, {"m²", 1}, {"mm2", 1e-6}, {"mm^2", 1e-6},
                  {"mm²", 1e-6}, {"cm2", 1e-4}, {"cm²", 1e-4}, {"in2", 0.00064516},
                  {"in²", 0.00064516}}},
        {"strain", {{"1", 1}, {"%", 1e-2}, {"mm/mm", 1}, {"m/m", 1}}},
        {"time", {{"s", 1}, {"sec", 1}, {"ms", 1e-3}, {"min", 60}}},
    };
    return t;
}

int findColumn(const QStringList &names, const QString &name)
{
    for (int i = 0; i < names.size(); ++i)
        if (names[i].compare(name.trimmed(), Qt::CaseInsensitive) == 0)
            return i;
    return -1;
}

QList<int> matching(const QStringList &names, const QRegularExpression &re)
{
    QList<int> out;
    for (int i = 0; i < names.size(); ++i)
        if (re.match(names[i]).hasMatch())
            out << i;
    return out;
}

const PreambleEntry *preamble(const Table &t, const QStringList &keys)
{
    for (const auto &p : t.preamble)
        for (const QString &k : keys)
            if (p.key.compare(k, Qt::CaseSensitive) == 0)
                return &p;
    return nullptr;
}

QString listColumns(const QStringList &names)
{
    return "the file has: " + names.join(", ");
}

} // namespace

std::optional<double> siFactor(const QString &unit, const QString &dimension)
{
    const auto &t = unitTable();
    const auto d = t.find(dimension);
    if (d == t.end())
        return std::nullopt;
    const auto u = d->second.find(stripBrackets(unit));
    if (u == d->second.end())
        return std::nullopt;
    return u->second;
}

std::optional<double> parseQuantity(const QString &text, const QString &dimension)
{
    static const QRegularExpression re(
        R"(^\s*([-+]?(?:\d+[.,]?\d*|[.,]\d+)(?:[eE][-+]?\d+)?)\s*(\S+)\s*$)");
    const auto m = re.match(text);
    if (!m.hasMatch())
        return std::nullopt;
    bool ok = false;
    const double v = QString(m.captured(1)).replace(',', '.').toDouble(&ok);
    const auto f = siFactor(m.captured(2), dimension);
    if (!ok || !f)
        return std::nullopt;
    return v * *f;
}

TableOrError readTable(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {std::nullopt, "cannot open " + path + ": " + file.errorString()};
    const QByteArray bytes = file.readAll();

    auto toUtf8 = QStringDecoder(QStringDecoder::Utf8);
    QString text = toUtf8(bytes);
    if (toUtf8.hasError())
        text = QString::fromLatin1(bytes);  // older machine software writes Latin-1
    QStringList lines = text.split('\n');
    for (QString &l : lines)
        if (l.endsWith('\r'))
            l.chop(1);

    const Dialect d = bestDialect(lines);
    if (d.count < 2)
        return {std::nullopt, path + " has no block of numeric rows; is it a delimited text export?"};

    Table t;
    t.path = path;
    t.sha256 = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
    t.delimiter = d.delim;
    t.decimal = d.decimal;

    // Names and units: the rows of the data's width directly above it.
    int namesRow = -1, unitsRow = -1;
    const int above = d.first - 1;
    if (above >= 0 && fields(lines[above], d.delim).size() == d.width) {
        if (above - 1 >= 0 && fields(lines[above - 1], d.delim).size() == d.width) {
            namesRow = above - 1;
            unitsRow = above;
        } else {
            namesRow = above;
        }
    }
    if (namesRow >= 0) {
        t.names = fields(lines[namesRow], d.delim);
    } else {
        for (int c = 0; c < d.width; ++c)
            t.names << QStringLiteral("column %1").arg(c + 1);
    }
    if (unitsRow >= 0) {
        for (const QString &u : fields(lines[unitsRow], d.delim))
            t.units << stripBrackets(u);
    } else {
        for (int c = 0; c < d.width; ++c)
            t.units << QString();
    }

    const int preambleEnd = namesRow >= 0 ? namesRow : d.first;
    for (int i = 0; i < preambleEnd; ++i) {
        const QStringList fs = fields(lines[i], d.delim);
        if (fs.size() >= 2 && !fs[0].isEmpty())
            t.preamble.push_back({fs[0], fs[1], fs.size() >= 3 ? fs[2] : QString()});
    }

    t.columns.assign(d.width, {});
    for (int i = d.first; i < d.first + d.count; ++i) {
        const QStringList fs = fields(lines[i], d.delim);
        for (int c = 0; c < d.width; ++c)
            t.columns[c].push_back(*number(fs[c], d.decimal));
    }
    return {t, {}};
}

TensionImport toTension(const Table &t, const TensionMapping &m)
{
    TensionImport out;
    auto fail = [&](const QString &why) {
        out.error = why;
        return out;
    };

    // --- Columns -----------------------------------------------------------------
    auto choose = [&](const QString &given, const QRegularExpression &guess, const QString &what,
                      const QString &prefer, int &col) -> QString {
        if (!given.isEmpty()) {
            col = findColumn(t.names, given);
            if (col < 0)
                return "no column named \"" + given + "\" for " + what + "; " + listColumns(t.names);
            return {};
        }
        QList<int> hits = matching(t.names, guess);
        if (hits.size() > 1 && !prefer.isEmpty()) {
            const QList<int> preferred = matching(t.names, QRegularExpression(prefer,
                                                  QRegularExpression::CaseInsensitiveOption));
            QList<int> both;
            for (int h : hits)
                if (preferred.contains(h))
                    both << h;
            if (both.size() == 1)
                hits = both;
        }
        if (hits.size() != 1)
            return QString("cannot tell which column is %1 (%2 candidates); name it. ")
                       .arg(what)
                       .arg(hits.size())
                   + listColumns(t.names);
        col = hits.front();
        out.notes << "\"" + t.names[col] + "\" taken as " + what + " by its name";
        return {};
    };
    const auto ci = QRegularExpression::CaseInsensitiveOption;
    int fc = -1, ec = -1;
    QString err = choose(m.force, QRegularExpression(R"(force|load|kraft)", ci), "force", {}, fc);
    if (!err.isEmpty())
        return fail(err);
    err = choose(m.extension, QRegularExpression(R"(extensometer|extension|elongation|strain|dehnung)", ci),
                 "extension", "extensometer", ec);
    if (!err.isEmpty())
        return fail(err);
    out.forceColumn = t.names[fc];
    out.extensionColumn = t.names[ec];

    // --- Units --------------------------------------------------------------------
    auto unitOf = [&](int col, const QString &given) -> QString {
        if (!given.isEmpty())
            return given;
        if (!t.units.value(col).isEmpty())
            return t.units[col];
        const QString name = t.names[col];
        const int us = name.lastIndexOf('_');
        return us >= 0 ? name.mid(us + 1) : QString();
    };
    const QString fu = unitOf(fc, m.forceUnit);
    const auto ff = siFactor(fu, "force");
    if (!ff)
        return fail("no known force unit for column \"" + out.forceColumn + "\" (found \"" + fu
                    + "\"); give one, e.g. kN");
    const QString eu = unitOf(ec, m.extensionUnit);
    const auto lf = siFactor(eu, "length");
    const auto sf = siFactor(eu, "strain");
    if (!lf && !sf)
        return fail("no known unit for column \"" + out.extensionColumn + "\" (found \"" + eu
                    + "\"); give a length (mm) or a strain (%)");

    // --- Geometry -----------------------------------------------------------------
    if (m.area) {
        out.input.area = *m.area;
    } else if (const PreambleEntry *p = preamble(t, {"So", "S0", "So [mm2]"})) {
        const auto a = parseQuantity(p->value + " " + p->unit, "area");
        if (!a)
            return fail("the file gives the cross-section area as \"" + p->value + " " + p->unit
                        + "\", which is not an area TenForge reads");
        out.input.area = *a;
        out.notes << "cross-section area S0 from the file's \"" + p->key + "\"";
    } else {
        return fail("the cross-section area S0 is not in the file; give it (e.g. 120.6 mm2)");
    }

    if (sf) {
        // The column is already a strain: extension over a unit gauge length.
        out.input.gaugeLength = 1.0;
        out.notes << "\"" + out.extensionColumn + "\" is a strain in " + eu
                     + "; used as strain, no gauge length needed";
    } else if (m.gaugeLength) {
        out.input.gaugeLength = *m.gaugeLength;
    } else if (const PreambleEntry *p = preamble(t, {"Le"})) {
        const auto g = parseQuantity(p->value + " " + p->unit, "length");
        if (!g)
            return fail("the file gives the extensometer gauge length as \"" + p->value + " "
                        + p->unit + "\", which is not a length TenForge reads");
        out.input.gaugeLength = *g;
        out.notes << "extensometer gauge length from the file's \"" + p->key + "\"";
    } else {
        return fail("the extensometer gauge length is not in the file; give it (e.g. 50 mm)");
    }

    const double ef = sf ? *sf : *lf;
    for (double v : t.columns[fc])
        out.input.force.push_back(v * *ff);
    for (double v : t.columns[ec])
        out.input.extension.push_back(v * ef);
    return out;
}

} // namespace tf::core
