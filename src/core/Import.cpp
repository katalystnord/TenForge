#include "core/Import.h"

#include <QCryptographicHash>
#include <QFile>
#include <QRegularExpression>
#include <QStringDecoder>

#include <algorithm>
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
                  {"in²", 0.00064516}, {"in^2", 0.00064516}, {"mm*mm", 1e-6}}},
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

// The columns whose names match, compared in compatibility form (NFKC), so
// half-width katakana (Shimadzu's "ｽﾄﾛｰｸ") match as full-width. A name
// repeated (Trapezium's "Force" in kN, then in N) is one choice: the first,
// the one findColumn() finds by that name.
QList<int> matching(const QStringList &names, const QRegularExpression &re)
{
    QList<int> out;
    for (int i = 0; i < names.size(); ++i)
        if (re.match(names[i].normalized(QString::NormalizationForm_KC)).hasMatch()
            && findColumn(names, names[i]) == i)
            out << i;
    return out;
}

// Shift-JIS (Japanese Windows, Shimadzu Trapezium), told from Latin-1 by its
// byte structure: every byte above 0x7F is a single-byte katakana or a lead
// byte with a valid trail, and some lead bytes are 0x81 to 0x9F, which in
// Latin-1 are control codes no text uses. A German "Prüfer" (0xFC 'f') is
// a valid pair too, which is why the 0x81 to 0x9F leads are required.
bool looksShiftJis(const QByteArray &bytes)
{
    int lowLeads = 0;
    for (qsizetype i = 0; i < bytes.size();) {
        const auto b = uchar(bytes[i]);
        if (b < 0x80 || (b >= 0xA1 && b <= 0xDF)) {
            ++i;
            continue;
        }
        if (!((b >= 0x81 && b <= 0x9F) || (b >= 0xE0 && b <= 0xFC)) || i + 1 >= bytes.size())
            return false;
        const auto t = uchar(bytes[i + 1]);
        if (!((t >= 0x40 && t <= 0x7E) || (t >= 0x80 && t <= 0xFC)))
            return false;
        if (b <= 0x9F)
            ++lowLeads;
        i += 2;
    }
    return lowLeads > 0;
}

// A header key without its section and colon: Bluehill writes
// "Dimension : Area", Merlin "Area:".
QString bareKey(QString k)
{
    const int sep = k.lastIndexOf(" : ");
    if (sep >= 0)
        k = k.mid(sep + 3);
    k = k.trimmed();
    if (k.endsWith(':'))
        k.chop(1);
    return k.trimmed();
}

const PreambleEntry *preamble(const Table &t, const QStringList &keys)
{
    for (const auto &p : t.preamble)
        for (const QString &k : keys)
            if (p.key.compare(k, Qt::CaseSensitive) == 0)
                return &p;
    for (const auto &p : t.preamble)
        for (const QString &k : keys)
            if (bareKey(p.key).compare(k, Qt::CaseInsensitive) == 0)
                return &p;
    return nullptr;
}

bool bracketed(const QString &f)
{
    return f.size() >= 2 && ((f.front() == '(' && f.back() == ')') || (f.front() == '[' && f.back() == ']'));
}

bool knownUnit(const QString &u)
{
    for (const char *d : {"force", "length", "strain", "time", "area"})
        if (siFactor(u, d))
            return true;
    return false;
}

// The unit a column's name carries, when it is one TenForge knows: after
// the last underscore ("force_kN"), in brackets at the end ("Force (kN)"),
// or as the last word ("Load N", Instron Merlin). A bare number is never
// taken as a unit ("Extension 1" is not a strain).
QString unitInName(const QString &name)
{
    const QString n = name.trimmed();
    QStringList tries;
    const int us = n.lastIndexOf('_');
    if (us >= 0)
        tries << n.mid(us + 1);
    static const QRegularExpression br(R"([\(\[]\s*([^\)\]]+?)\s*[\)\]]$)");
    if (const auto m = br.match(n); m.hasMatch())
        tries << m.captured(1);
    const int sp = n.lastIndexOf(' ');
    if (sp >= 0)
        tries << n.mid(sp + 1);
    for (const QString &u : tries) {
        bool isNumber = false;
        u.toDouble(&isNumber);
        if (!isNumber && knownUnit(u))
            return u;
    }
    return {};
}

const QRegularExpression &forceRe()
{
    // English, German, Italian, Spanish, Portuguese, Japanese.
    static const QRegularExpression re(R"(force|load|kraft|carico|forza|fuerza|carga|for[cç]a|試験力|荷重)",
                                       QRegularExpression::CaseInsensitiveOption);
    return re;
}

// Columns named as force, less the controller's set points ("Ch 1 Kraft
// Sollwert", "Force Command"): a set point is what was asked for, not what
// was measured.
QList<int> forceCandidates(const QStringList &names)
{
    static const QRegularExpression setPoint(R"(soll|set\s*point|command|consigne|commande)",
                                             QRegularExpression::CaseInsensitiveOption);
    QList<int> out;
    for (int c : matching(names, forceRe()))
        if (!setPoint.match(names[c]).hasMatch())
            out << c;
    return out;
}

// The displacement column, in tiers: a column of the first tier that has
// any wins, so an extensometer beats a named deformation, which beats the
// crosshead. Several in one tier are left for the person to choose.
const std::vector<QRegularExpression> &extensionTiers()
{
    static const std::vector<QRegularExpression> tiers{
        QRegularExpression(R"(extensometer)", QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(R"(extension|elongation|strain|dehnung|deflection|durchbiegung|)"
                           R"(verformung|d[eé]formation|allongement|compression|stauchung|)"
                           R"(allungamento|estensione|deformazione|flessione|)"
                           R"(alargamiento|elongaci[oó]n|deformaci[oó]n|flecha|)"
                           R"(alongamento|deforma[cç][aã]o|伸び|ひずみ|歪)",
                           QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(R"(displacement|crosshead|traverse|position|stroke|lvdt|\bweg\b|)"
                           R"(corsa|traversa|spostamento|posizione|)"
                           R"(desplazamiento|posici[oó]n|deslocamento|posi[cç][aã]o|ストローク|変位)",
                           QRegularExpression::CaseInsensitiveOption),
    };
    return tiers;
}

QList<int> extensionCandidates(const QStringList &names)
{
    for (const auto &re : extensionTiers()) {
        const QList<int> hits = matching(names, re);
        if (!hits.isEmpty())
            return hits;
    }
    return {};
}

QString listColumns(const QStringList &names)
{
    return "the file has: " + names.join(", ");
}

void noteResults(Table &t)
{
    if (!t.results.empty())
        t.notes << QString("the file holds %1 result(s) of its own software; they are shown as the "
                           "file's, not used by the analysis")
                       .arg(t.results.size());
}

// MTS 793 (MultiPurpose TestWare) ".dat": a signature line
// "MTS793|MPT|<language>|..|..|<decimal>|<date sep>|<time sep>|..", then
// sections, each under a title line ("Data Acquisition ... Time: 102.0 s
// <date>"). A data section has a names row, a units row and rows of
// numbers; an information section ("Bediener Information") has key-value
// lines up to "<title> Ende"/"End". The data comes in sections of at most
// 1024 rows under the same header, so sections with the same columns are
// one table, joined in order. A section with other columns (a peak-valley
// acquisition, unlabelled) is kept as the file's own values.
TableOrError readMts793(const QString &path, const QByteArray &bytes, const QStringList &lines)
{
    const QStringList sig = lines.front().split('|');
    const QChar decimal = sig.value(5) == "," ? QChar(',') : QChar('.');

    struct Section {
        QString title;
        QStringList names, units;
        std::vector<QStringList> rows;
    };
    std::vector<Section> data;
    Table t;
    t.path = path;
    t.sha256 = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
    t.delimiter = '\t';
    t.decimal = decimal;

    // A title line: a name, then later a field "Time:"/"Zeit:" followed by a number.
    auto isTitle = [&](const QStringList &fs) {
        if (fs.size() < 3 || fs[0].isEmpty())
            return false;
        for (int k = 1; k + 1 < fs.size(); ++k)
            if (fs[k].endsWith(':') && number(fs[k + 1], decimal).has_value() && !fs[k + 1].isEmpty())
                return true;
        return false;
    };
    auto describe = [](const QStringList &fs) {
        QStringList rest;
        for (int k = 1; k < fs.size(); ++k)
            if (!fs[k].isEmpty())
                rest << fs[k];
        return rest.join(' ');
    };

    for (int i = 1; i < lines.size(); ++i) {
        const QStringList fs = fields(lines[i], '\t');
        if (!isTitle(fs))
            continue;
        const QString title = fs[0];
        // Information section: key-value lines up to "<title> End(e)".
        int end = -1;
        for (int j = i + 1; j < lines.size() && !lines[j].trimmed().isEmpty(); ++j)
            if (lines[j].trimmed().startsWith(title + " "))
                end = j;
        if (end >= 0) {
            t.preamble.push_back({title, describe(fs), QString()});
            for (int j = i + 1; j < end; ++j) {
                const QStringList kv = fields(lines[j], '\t');
                if (!kv[0].isEmpty())
                    t.preamble.push_back({kv[0], kv.value(1), kv.value(2)});
            }
            i = end;
            continue;
        }
        if (i + 2 >= lines.size())
            break;
        Section s;
        s.title = title + " (" + describe(fs) + ")";
        s.names = fields(lines[i + 1], '\t');
        s.units = fields(lines[i + 2], '\t');
        int j = i + 3;
        for (; j < lines.size(); ++j) {
            const QStringList row = fields(lines[j], '\t');
            if (row.size() != s.names.size() || !numericRow(row, decimal))
                break;
            s.rows.push_back(row);
        }
        data.push_back(std::move(s));
        i = j - 1;
    }

    // The table: the columns with the most rows over all their sections.
    std::map<QString, size_t> rowsOf;
    auto keyOf = [](const Section &s) { return s.names.join('\t') + '\n' + s.units.join('\t'); };
    for (const auto &s : data)
        rowsOf[keyOf(s)] += s.rows.size();
    QString main;
    size_t most = 0;
    for (const auto &s : data)  // in file order, so a tie goes to the first
        if (rowsOf[keyOf(s)] > most) {
            most = rowsOf[keyOf(s)];
            main = keyOf(s);
        }
    if (most < 2)
        return {std::nullopt, path + " is an MTS 793 export with no block of numeric rows"};

    int joined = 0;
    for (const auto &s : data) {
        if (keyOf(s) == main) {
            if (joined++ == 0) {
                t.names = s.names;
                for (const QString &u : s.units)
                    t.units << stripBrackets(u);
                t.columns.assign(s.names.size(), {});
                t.preamble.push_back({s.title.section(" (", 0, 0), s.title.section(" (", 1).chopped(1),
                                      QString()});
            }
            for (const auto &row : s.rows)
                for (int c = 0; c < row.size(); ++c)
                    t.columns[c].push_back(*number(row[c], decimal));
        } else {
            for (size_t r = 0; r < s.rows.size(); ++r)
                for (int c = 0; c < s.names.size(); ++c)
                    t.results.push_back({s.title, QString::number(r + 1), s.names[c],
                                         stripBrackets(s.units.value(c)), s.rows[r][c]});
        }
    }
    t.notes << QString("an MTS 793 export (language %1)").arg(sig.value(2));
    if (joined > 1)
        t.notes << QString("the data came in %1 blocks with the same columns (MTS 793 writes it so); "
                           "they are joined in order, no row left out")
                       .arg(joined);
    if (!t.results.empty())
        t.notes << QString("the file has %1 value(s) from another acquisition with other columns, "
                           "unlabelled; they are shown as the file's, not used by the analysis")
                       .arg(t.results.size());
    return {t, {}};
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

TableOrError readTable(const QString &path, const QString &fileSpecimen)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {std::nullopt, "cannot open " + path + ": " + file.errorString()};
    const QByteArray bytes = file.readAll();

    auto toUtf8 = QStringDecoder(QStringDecoder::Utf8);
    QString text = toUtf8(bytes);
    QString encodingNote;
    if (toUtf8.hasError()) {
        QStringDecoder sjis("windows-31j");  // Shift-JIS as Windows writes it
        if (looksShiftJis(bytes) && sjis.isValid()) {
            text = sjis(bytes);
            encodingNote = "the file is in Shift-JIS (Japanese) and is read as such";
        }
        if (encodingNote.isEmpty() || sjis.hasError()) {
            text = QString::fromLatin1(bytes);  // older machine software writes Latin-1
            encodingNote.clear();
        }
    }
    QStringList lines = text.split('\n');
    for (QString &l : lines)
        if (l.endsWith('\r'))
            l.chop(1);

    if (!lines.isEmpty() && lines.front().startsWith("MTS793|MPT|"))
        return readMts793(path, bytes, lines);

    const Dialect d = bestDialect(lines);
    if (d.count < 2)
        return {std::nullopt, path + " has no block of numeric rows; is it a delimited text export?"};

    Table t;
    t.path = path;
    t.sha256 = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
    t.delimiter = d.delim;
    t.decimal = d.decimal;
    if (!encodingNote.isEmpty())
        t.notes << encodingNote;

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

    // Specimens side by side (Shimadzu Trapezium): a row of titles just
    // above the names, each title over a group of columns, every group with
    // the same names (empty-named spacer columns after a group aside).
    std::vector<int> starts;
    int groupWidth = 0;
    if (namesRow >= 1) {
        const QStringList titles = fields(lines[namesRow - 1], d.delim);
        for (int c = 0; c < titles.size() && c < d.width; ++c)
            if (!titles[c].isEmpty())
                starts.push_back(c);
        bool same = starts.size() >= 2 && starts.front() == 0;
        QStringList first;
        for (size_t g = 0; same && g < starts.size(); ++g) {
            const int end = g + 1 < starts.size() ? starts[g + 1] : d.width;
            QStringList group = t.names.mid(starts[g], end - starts[g]);
            while (!group.isEmpty() && group.back().isEmpty())
                group.removeLast();
            if (g == 0)
                first = group;
            same = !group.isEmpty() && group == first;
        }
        if (same) {
            groupWidth = int(first.size());
            for (int c : starts)
                t.fileSpecimens << titles[c];
        } else {
            starts.clear();
        }
    }
    int chosen = 0;
    if (!fileSpecimen.isEmpty()) {
        chosen = int(t.fileSpecimens.indexOf(fileSpecimen.trimmed()));
        if (chosen < 0)
            return {std::nullopt,
                    t.fileSpecimens.isEmpty()
                        ? path + " holds one specimen, not one named \"" + fileSpecimen + "\""
                        : path + " has no specimen \"" + fileSpecimen + "\"; it has "
                              + t.fileSpecimens.join(", ")};
    }

    const int preambleEnd = !starts.empty() ? namesRow - 1 : namesRow >= 0 ? namesRow : d.first;
    static const QRegularExpression resultsTitle(R"(^results\s+table\b)",
                                                 QRegularExpression::CaseInsensitiveOption);
    for (int i = 0; i < preambleEnd; ++i) {
        const QStringList fs = fields(lines[i], d.delim);
        // The software's own results (Bluehill: "Results Table 1", then
        // names, units in brackets, one row per specimen), kept apart.
        if (fs.size() == 1 && resultsTitle.match(fs[0]).hasMatch()) {
            const QString title = fs[0];
            int j = i + 1;
            const QStringList names = j < preambleEnd ? fields(lines[j], d.delim) : QStringList();
            if (names.size() < 2) {
                continue;  // a title with no table under it (the curve's own)
            }
            ++j;
            QStringList units;
            if (j < preambleEnd) {
                const QStringList us = fields(lines[j], d.delim);
                bool allUnits = true;
                for (const QString &u : us)
                    allUnits = allUnits && (u.isEmpty() || bracketed(u));
                if (allUnits) {
                    for (const QString &u : us)
                        units << stripBrackets(u);
                    ++j;
                }
            }
            for (; j < preambleEnd; ++j) {
                const QStringList row = fields(lines[j], d.delim);
                if (row.size() < 2 || (row.size() == 1 && row[0].isEmpty()))
                    break;
                for (int c = 1; c < names.size() && c < row.size(); ++c)
                    if (!names[c].isEmpty())
                        t.results.push_back({title, row[0], names[c], units.value(c), row[c]});
            }
            i = j - 1;
            continue;
        }
        // Bluehill Universal: key, (unit), value.
        if (fs.size() == 3 && !fs[0].isEmpty() && (fs[1].isEmpty() || bracketed(fs[1]))) {
            t.preamble.push_back({fs[0], fs[2], stripBrackets(fs[1])});
            continue;
        }
        if (fs.size() >= 2 && !fs[0].isEmpty()) {
            t.preamble.push_back({fs[0], fs[1], fs.size() >= 3 ? fs[2] : QString()});
            continue;
        }
        // "Key: value" in one field (MTS TestSuite: "Test Run: ...",
        // "Essai: ..."). The key is short and plain; the value is the rest.
        static const QRegularExpression colon(R"(^([^:\t]{1,40}?):\s+(\S.*?)\s*$)");
        if (fs.size() == 1) {
            const auto c = colon.match(fs[0]);
            if (c.hasMatch() && !c.captured(1).contains("  ")) {
                t.preamble.push_back({c.captured(1).trimmed(), c.captured(2), QString()});
                continue;
            }
        }
        // "key   value unit", with no delimiter (ZwickRoell testXpert): the
        // key ends at the first run of two or more spaces.
        static const QRegularExpression spaced(R"(^\s*(\S.*?)\s{2,}(\S+)(?:\s+(\S.*?))?\s*$)");
        const auto m = spaced.match(lines[i]);
        if (fs.size() < 2 && m.hasMatch())
            t.preamble.push_back({m.captured(1), m.captured(2), m.captured(3)});
    }

    t.columns.assign(d.width, {});
    for (int i = d.first; i < d.first + d.count; ++i) {
        const QStringList fs = fields(lines[i], d.delim);
        for (int c = 0; c < d.width; ++c)
            t.columns[c].push_back(*number(fs[c], d.decimal));
    }

    if (!starts.empty()) {
        const int c0 = starts[size_t(chosen)];
        t.fileSpecimen = t.fileSpecimens[chosen];
        t.names = t.names.mid(c0, groupWidth);
        t.units = t.units.mid(c0, groupWidth);
        std::vector<std::vector<double>> cols(t.columns.begin() + c0,
                                              t.columns.begin() + c0 + groupWidth);
        // The specimen's rows end where all its cells are empty to the end;
        // the rows below belong to longer specimens beside it.
        size_t rows = cols[0].size();
        while (rows > 0 && std::all_of(cols.begin(), cols.end(),
                                       [&](const auto &col) { return std::isnan(col[rows - 1]); }))
            --rows;
        const size_t below = cols[0].size() - rows;
        for (auto &col : cols)
            col.resize(rows);
        t.columns = std::move(cols);
        t.notes << QString("the file holds %1 specimens side by side (%2); this is \"%3\", its "
                           "%4 rows")
                       .arg(t.fileSpecimens.size())
                       .arg(t.fileSpecimens.join(", "))
                       .arg(t.fileSpecimen)
                       .arg(rows)
                       + (below > 0 ? QString(", the %1 empty rows below them left out").arg(below)
                                    : QString());
    }

    // A last column with no name, empty in every row, is a trailing
    // delimiter (Shimadzu Trapezium ends every line with a comma).
    if (t.names.size() > 2 && t.names.back().isEmpty()
        && std::all_of(t.columns.back().begin(), t.columns.back().end(),
                       [](double v) { return std::isnan(v); })) {
        t.notes << QString("the file's last column has no name and is empty in every row (each "
                           "line ends with a delimiter); it is left out");
        t.names.removeLast();
        t.units.removeLast();
        t.columns.pop_back();
    }

    // Two columns of one name: a name chooses the first, and that is said.
    for (int c = 0; c < t.names.size(); ++c) {
        if (t.names[c].isEmpty() || findColumn(t.names, t.names[c]) != c)
            continue;
        QStringList units;
        for (int k = c; k < t.names.size(); ++k)
            if (t.names[k].compare(t.names[c], Qt::CaseInsensitive) == 0)
                units << (t.units.value(k).isEmpty() ? QString("no unit") : t.units[k]);
        if (units.size() > 1)
            t.notes << QString("%1 columns are named \"%2\" (%3); the name means the first")
                           .arg(units.size())
                           .arg(t.names[c], units.join(", "));
    }

    // A leading column that is empty in every row and named by a number or
    // nothing (Bluehill writes the specimen number there, above no data)
    // carries nothing: left out, and said.
    static const QRegularExpression indexName(R"(^\d*$)");
    if (d.width > 2 && indexName.match(t.names[0]).hasMatch()
        && std::all_of(t.columns[0].begin(), t.columns[0].end(),
                       [](double v) { return std::isnan(v); })) {
        t.notes << (t.names[0].isEmpty()
                        ? QString("the file's first column is empty in every row; it is left out")
                        : "the file's first column (headed \"" + t.names[0]
                              + "\") is empty in every row; it is left out");
        t.names.removeFirst();
        t.units.removeFirst();
        t.columns.erase(t.columns.begin());
    }
    noteResults(t);
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
    auto choose = [&](const QString &given, const QList<int> &candidates, const QString &what,
                      int &col) -> QString {
        if (!given.isEmpty()) {
            col = findColumn(t.names, given);
            if (col < 0)
                return "no column named \"" + given + "\" for " + what + "; " + listColumns(t.names);
            return {};
        }
        const QList<int> &hits = candidates;
        if (hits.size() != 1)
            return QString("cannot tell which column is %1 (%2 candidates); name it. ")
                       .arg(what)
                       .arg(hits.size())
                   + listColumns(t.names);
        col = hits.front();
        out.notes << "\"" + t.names[col] + "\" taken as " + what + " by its name";
        return {};
    };
    int fc = -1, ec = -1;
    QString err = choose(m.force, forceCandidates(t.names), "force", fc);
    if (!err.isEmpty())
        return fail(err);
    err = choose(m.extension, extensionCandidates(t.names), "extension", ec);
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
        if (const QString u = unitInName(name); !u.isEmpty())
            return u;
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
    if (!m.needsGeometry) {
        out.input.area = 0;
        out.input.gaugeLength = sf ? 1.0 : 0.0;
        if (sf)
            out.notes << "\"" + out.extensionColumn + "\" is a strain in " + eu + "; used as strain";
    } else if (m.area) {
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

    if (!m.needsGeometry) {
    } else if (sf) {
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

    out.forceUnit = fu;
    out.extensionUnit = eu;
    out.extensionIsStrain = bool(sf);
    const double ef = sf ? *sf : *lf;
    for (double v : t.columns[fc])
        out.input.force.push_back(v * *ff);
    for (double v : t.columns[ec])
        out.input.extension.push_back(v * ef);
    return out;
}

namespace {

QString guess(const Table &t, const QList<int> &hits, const QString &what, QString *why)
{
    if (hits.size() == 1)
        return t.names[hits.front()];
    if (why)
        *why = hits.isEmpty() ? "no column's name says it is " + what
                              : QString("%1 columns could be %2").arg(hits.size()).arg(what);
    return {};
}

} // namespace

QString guessForceColumn(const Table &t, QString *why)
{
    return guess(t, forceCandidates(t.names), "force", why);
}

QString guessExtensionColumn(const Table &t, QString *why)
{
    return guess(t, extensionCandidates(t.names), "extension", why);
}

QString columnUnit(const Table &t, const QString &column)
{
    const int col = findColumn(t.names, column);
    if (col < 0)
        return {};
    if (!t.units.value(col).isEmpty())
        return t.units[col];
    return unitInName(t.names[col]);
}

std::optional<double> preambleQuantity(const Table &t, const QStringList &keys,
                                       const QString &dimension, QString *key)
{
    const PreambleEntry *p = preamble(t, keys);
    if (!p)
        return std::nullopt;
    if (key)
        *key = p->key;
    return parseQuantity(p->value + " " + p->unit, dimension);
}

std::optional<double> fileResultQuantity(const Table &t, const QStringList &names,
                                         const QString &dimension, const FileResult **found)
{
    for (const FileResult &r : t.results) {
        bool match = false;
        for (const QString &n : names)
            match = match || r.name.compare(n, Qt::CaseInsensitive) == 0;
        if (!match)
            continue;
        if (found)
            *found = &r;
        // Bluehill groups thousands with commas in its results ("2,225.34").
        QString v = r.value;
        static const QRegularExpression grouped(R"(^-?\d{1,3}(,\d{3})+(\.\d*)?$)");
        if (grouped.match(v).hasMatch())
            v.remove(',');
        bool ok = false;
        v.toDouble(&ok);
        if (!ok || r.unit.isEmpty())
            return std::nullopt;
        return parseQuantity(v + " " + r.unit, dimension);
    }
    return std::nullopt;
}

QStringList knownUnits(const QString &dimension)
{
    static const std::map<QString, QStringList> order{
        {"force", {"N", "kN", "MN", "mN", "lbf", "kgf"}},
        {"length", {"mm", "m", "cm", "um", "in"}},
        {"strain", {"%", "1", "mm/mm"}},
        {"area", {"mm2", "m2", "cm2", "in2"}},
    };
    QStringList out;
    const auto it = order.find(dimension);
    if (it != order.end())
        for (const QString &u : it->second)
            if (siFactor(u, dimension))
                out << u;
    return out;
}

} // namespace tf::core
