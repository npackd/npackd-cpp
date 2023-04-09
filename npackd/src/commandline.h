#ifndef COMMANDLINE_H
#define COMMANDLINE_H

#include <vector>

#include "qstring.h"

/**
 * Parses a command line with options. The option names are case-sensitive.
 */
class CommandLine
{
public:
    /**
     * @brief an option definition
     */
    class Option {
    public:
        /** long name of the option or an empty string if there is none */
        QString name;

        /** short name for the option or 0 if not available */
        char name2;

        /**
         * description for the option value or an empty string if
         * there should be no value
         */
        QString valueDescription;

        /** description */
        QString description;

        /** can this option be repeated? */
        bool multiple;

        /**
         * the list of allowed commands for this option or an empty list if this
         * option is "global" and should be available in all options
         */
        std::vector<QString> allowedCommands;

        /**
         * does this option matches the specified name
         *
         * @param name short or long name for an option
         * @return true if this option matches the specified name
         */
        bool nameMatches(const QString& name);
    };

    /**
     * @brief a parsed option
     */
    class ParsedOption {
    public:
        /** pointer to the found option or 0 for "free arguments" */
        Option* opt;

        /** parsed option value */
        QString value;
    };
private:
    std::vector<Option*> options;
    std::vector<ParsedOption*> parsedOptions;

    Option* findOption(const QString& name);
    QString processOneParam(std::vector<QString> *params);
public:
    /**
     * -
     */
    CommandLine();

    ~CommandLine();

    /**
     * Adds an option.
     *
     * @param name name of the option
     * @param name2 short name or 0 if not available
     * @param description short description of this option for printing help
     * @param valueDescription description of the value for this option.
     *     If "", a value is not possible.
     * @param multiple true if multiple occurences of this option are allowed
     * @param allowedCommands comma separated list of allowed commands or an
     *     an empty string if this option is "global"
     */
    void add(QString name, char name2, QString description, QString valueDescription,
            bool multiple, const QString& allowedCommands="");

    /**
     * Print the option description
     *
     * Example:
     *    -p, --package   package name",
     *    -r, --versions  version range (e.g. [1.1,1.2) )",
     *    -v, --version   version (e.g. 1.2.47)",
     *    -u, --url       repository URL (e.g. https://www.example.com/Rep.xml)",
     *    -s, --status    filters package versions by status",
     */
    std::vector<QString> printOptions() const;

    /**
     * Parses program arguments
     *
     * @return error message or ""
     */
    QString parse();

    /**
     * @param name name of the option
     * @return true if the given option is present at least once in the command
     *     line
     */
    bool isPresent(const QString& name);

    /**
     * @param name name of the option
     * @return value of the option or QString::Null if not present
     */
    QString get(const QString& name) const;

    /**
     * @param name name of the option
     * @return values of the option or an empty list if not present
     */
    std::vector<QString> getAll(const QString& name) const;

    /**
     * @return parsed options
     */
    std::vector<ParsedOption *> getParsedOptions() const;

    /**
     * @return true if any arguments were entered
     */
    bool argumentsAvailable() const;
};

#endif // COMMANDLINE_H
