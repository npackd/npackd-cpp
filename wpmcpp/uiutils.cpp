#include <windows.h>

#include <QMessageBox>
#include <QBuffer>

#include "uiutils.h"


UIUtils::UIUtils()
{
}

QString UIUtils::extractIconURL(const QString& iconFile)
{
    QString res;
    QString icon = iconFile.trimmed();
    if (!icon.isEmpty()) {
        UINT iconIndex = 0;
        int index = icon.lastIndexOf(',');
        if (index > 0) {
            QString number = icon.mid(index + 1);
            bool ok;
            int v = number.toInt(&ok);
            if (ok) {
                iconIndex = (UINT) v;
                icon = icon.left(index);
            }
        }

        HICON ic = ExtractIcon(GetModuleHandle(NULL),
                (LPCWSTR) icon.utf16(), iconIndex);
        if (((UINT_PTR) ic) > 1) {
            QPixmap pm = QPixmap::fromWinHICON(ic);
            if (!pm.isNull() && pm.width() > 0 &&
                    pm.height() > 0) {
                QByteArray bytes;
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                pm.save(&buffer, "PNG");
                res = QString("data:image/png;base64,") +
                        bytes.toBase64();
            }
            DestroyIcon(ic);
        }
    }

    return res;
}

bool UIUtils::confirm(QWidget* parent, QString title, QString text,
        QString detailedText)
{
    /*
    QStringList lines = text.split("\n", QString::SkipEmptyParts);
    int max;
    if (lines.count() > 20) {
        max = 20;
    } else {
        max = lines.count();
    }
    int n = 0;
    int keep = 0;
    for (int i = 0; i < max; i++) {
        n += lines.at(i).length();
        if (n > 20 * 80)
            break;
        keep++;
    }
    if (keep == 0)
        keep = 1;
    QString shortText;
    if (keep < lines.count()) {
        lines = lines.mid(0, keep);
        shortText = lines.join("\n") + "...";
    } else {
        shortText = text;
    }
    */

    QMessageBox mb(parent);
    mb.setWindowTitle(title);
    mb.setText(text);
    mb.setIcon(QMessageBox::Warning);
    mb.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    mb.setDefaultButton(QMessageBox::Ok);
    mb.setDetailedText(detailedText);
    return ((QMessageBox::StandardButton) mb.exec()) == QMessageBox::Ok;
}

