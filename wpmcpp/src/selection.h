#ifndef SELECTION_H
#define SELECTION_H

#include <QList>

/**
 * QWidgets may implement this interface to provide context to actions.
 */
class Selection
{
public:
    explicit Selection();

    /**
     * @param type type of the requested objects. Example: "Package"
     * @return list of selected objects
     */
    virtual QList<void*> getSelected(const QString& type) const = 0;

    /**
     * @return current selected QWidget* or 0
     */
    static Selection* findCurrent();

    /**
     * This signal will be emitted each time the selection changes.
     */
    // void selectionChanged();
};

#endif // SELECTION_H
