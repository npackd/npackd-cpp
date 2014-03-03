#include "xmlutils.h"

#include <QDomNode>
#include <QDomNodeList>

XMLUtils::XMLUtils()
{
}

void XMLUtils::addTextTag(QDomElement& parent, const QString& name,
        const QString& value)
{
    QDomElement e = parent.ownerDocument().createElement(name);
    parent.appendChild(e);
    QDomText t = parent.ownerDocument().createTextNode(value);
    e.appendChild(t);
}


QString XMLUtils::getTagContent(const QDomElement& parent, const QString& name)
{
    QDomNode child = parent.firstChildElement(name);
    if (!child.isNull()) {
        QDomNode txt = child.firstChild();
        if (txt.isText()) {
            return txt.nodeValue().trimmed();
        }
    }
    return QString();
}

