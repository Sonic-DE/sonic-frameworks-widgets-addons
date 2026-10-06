/*
    SPDX-FileCopyrightText: 2026 SonicDE contributors

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KCapacityBar>

#include <QImage>
#include <QPainter>
#include <QProxyStyle>
#include <QRegion>
#include <QScopedPointer>
#include <QStyleFactory>
#include <QStyleOptionProgressBar>
#include <QTest>

class CapacityBarStyle : public QProxyStyle {
public:
    CapacityBarStyle()
        : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
    {
    }

    mutable int labelCalls = 0;
    mutable QStyleOptionProgressBar labelOption;

    QRect subElementRect(SubElement element, const QStyleOption* option, const QWidget* widget) const override
    {
        if (element == SE_ProgressBarContents) {
            return option->rect.adjusted(2, 2, -2, -2);
        }
        return QProxyStyle::subElementRect(element, option, widget);
    }

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget) const override
    {
        if (element == CE_ProgressBar) {
            const auto* progress = qstyleoption_cast<const QStyleOptionProgressBar*>(option);
            painter->fillRect(option->rect, Qt::gray);
            QRect fill = subElementRect(SE_ProgressBarContents, option, widget);
            fill.setWidth(fill.width() * progress->progress / progress->maximum);
            painter->fillRect(fill, Qt::blue);
        } else if (element == CE_ProgressBarGroove) {
            painter->fillRect(option->rect, Qt::gray);
        } else if (element == CE_ProgressBarContents) {
            const auto* progress = qstyleoption_cast<const QStyleOptionProgressBar*>(option);
            QCOMPARE(progress->progress, progress->maximum);
            painter->fillRect(subElementRect(SE_ProgressBarContents, option, widget), Qt::blue);
        } else if (element == CE_ProgressBarLabel) {
            ++labelCalls;
            labelOption = *qstyleoption_cast<const QStyleOptionProgressBar*>(option);
        } else {
            QProxyStyle::drawControl(element, option, painter, widget);
        }
    }
};

class KCapacityBarTest : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testBlocks_data()
    {
        QTest::addColumn<bool>("rightToLeft");
        QTest::addColumn<bool>("fillFullBlocks");
        QTest::addColumn<int>("value");
        QTest::addColumn<bool>("inlineText");

        for (bool rtl : {false, true}) {
            for (bool full : {false, true}) {
                for (int value : {0, 7, 37, 100}) {
                    for (bool inlineText : {false, true}) {
                        const QByteArray name = QByteArray::number(rtl) + '-' + QByteArray::number(full) + '-' + QByteArray::number(value) + '-'
                            + QByteArray::number(inlineText);
                        QTest::newRow(name.constData()) << rtl << full << value << inlineText;
                    }
                }
            }
        }
    }

    void testBlocks()
    {
        QFETCH(bool, rightToLeft);
        QFETCH(bool, fillFullBlocks);
        QFETCH(int, value);
        QFETCH(bool, inlineText);

        CapacityBarStyle style;
        KCapacityBar bar(inlineText ? KCapacityBar::DrawTextInline : KCapacityBar::DrawTextOutline);
        bar.setStyle(&style);
        bar.setLayoutDirection(rightToLeft ? Qt::RightToLeft : Qt::LeftToRight);
        bar.setContinuous(false);
        bar.setFillFullBlocks(fillFullBlocks);
        bar.setValue(value);
        bar.setBarHeight(20);

        const QRect rect(11, 7, 40, 20);
        const QRect contents = rect.adjusted(2, 2, -2, -2);
        const QRect clip(12, 8, 38, 18);
        QImage image(64, 40, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.setClipRect(clip);
        const auto hints = painter.renderHints();
        bar.drawCapacityBar(&painter, rect);
        QCOMPARE(painter.clipRegion(), QRegion(clip));
        QCOMPARE(painter.renderHints(), hints);
        painter.end();

        const int filledWidth = contents.width() * value / 100;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                QColor expected = Qt::white;
                if (rect.contains(x, y) && clip.contains(x, y)) {
                    expected = Qt::gray;
                    if (contents.contains(x, y)) {
                        const int offset = rightToLeft ? contents.right() - x : x - contents.left();
                        const int blockStart = offset / 8 * 8;
                        if (offset % 8 < 6 && blockStart < filledWidth && (fillFullBlocks || offset < filledWidth)) {
                            expected = Qt::blue;
                        }
                    }
                }
                QCOMPARE(image.pixelColor(x, y), expected);
            }
        }
    }

    void testModeChanges()
    {
        QScopedPointer<QStyle> style(QStyleFactory::create(QStringLiteral("Fusion")));
        QVERIFY(style);
        KCapacityBar bar;
        bar.setStyle(style.data());
        bar.setValue(38);
        bar.setBarHeight(20);
        const auto render = [&bar]() {
            QImage image(240, 50, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::white);
            QPainter painter(&image);
            bar.drawCapacityBar(&painter, image.rect());
            painter.end();
            return image;
        };

        const QImage continuous = render();
        bar.setContinuous(false);
        const QImage blocks = render();
        QVERIFY(blocks != continuous);
        bar.setFillFullBlocks(false);
        QVERIFY(render() != blocks);
        bar.setContinuous(true);
        QCOMPARE(render(), continuous);
    }

    void testInlineLabel()
    {
        CapacityBarStyle style;
        KCapacityBar bar(KCapacityBar::DrawTextInline);
        bar.setStyle(&style);
        bar.setContinuous(false);
        bar.setValue(37);
        bar.setText(QStringLiteral("37% used"));

        QImage image(240, 50, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter painter(&image);
        bar.drawCapacityBar(&painter, image.rect(), QStyle::State_Selected);
        painter.end();

        QCOMPARE(style.labelCalls, 1);
        QCOMPARE(style.labelOption.text, bar.text());
        QCOMPARE(style.labelOption.progress, 37);
        QVERIFY(style.labelOption.textVisible);
        QVERIFY(style.labelOption.state.testFlag(QStyle::State_Horizontal));
        QVERIFY(style.labelOption.state.testFlag(QStyle::State_Selected));
    }
};

QTEST_MAIN(KCapacityBarTest)

#include "kcapacitybartest.moc"
