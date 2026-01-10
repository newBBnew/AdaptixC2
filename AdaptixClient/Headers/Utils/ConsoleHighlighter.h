#ifndef CONSOLEHIGHLIGHTER_H
#define CONSOLEHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

class ConsoleHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit ConsoleHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;

    QTextCharFormat successFormat;
    QTextCharFormat errorFormat;
    QTextCharFormat infoFormat;
    QTextCharFormat numberFormat;
};

#endif
