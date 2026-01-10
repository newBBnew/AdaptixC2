#include <Utils/ConsoleHighlighter.h>

ConsoleHighlighter::ConsoleHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent) {
    HighlightingRule rule;

    // SUCCESS - 亮青色 (匹配呼吸主题强调色)
    successFormat.setForeground(QColor("#00E5FF"));
    successFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression(QStringLiteral("\\b(SUCCESS|OK|DONE|CONNECTED|COMPLETED)\\b"), QRegularExpression::CaseInsensitiveOption);
    rule.format = successFormat;
    highlightingRules.append(rule);

    // ERROR - 亮红色
    errorFormat.setForeground(QColor("#FF5252"));
    errorFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression(QStringLiteral("\\b(ERROR|FAILED|FAILURE|CRITICAL|STOPPED|DISCONNECTED)\\b"), QRegularExpression::CaseInsensitiveOption);
    rule.format = errorFormat;
    highlightingRules.append(rule);

    // INFO/SESSION - 紫色/蓝色
    infoFormat.setForeground(QColor("#B388FF"));
    rule.pattern = QRegularExpression(QStringLiteral("\\b(INFO|SESSION|BEACON|AGENT|LISTENER)\\b"), QRegularExpression::CaseInsensitiveOption);
    rule.format = infoFormat;
    highlightingRules.append(rule);

    // 时间戳/数字 - 柔和灰色
    numberFormat.setForeground(QColor("#78909C"));
    rule.pattern = QRegularExpression(QStringLiteral("\\b\\d+\\b|\\d{2}:\\d{2}:\\d{2}"));
    rule.format = numberFormat;
    highlightingRules.append(rule);
}

void ConsoleHighlighter::highlightBlock(const QString &text) {
    for (const HighlightingRule &rule : std::as_const(highlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}
