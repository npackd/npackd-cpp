#ifndef PACKAGEMANAGER_H
#define PACKAGEMANAGER_H

/**
 * @brief Package manager
 * @threadsafe
 */
class PackageManager
{
private:
    static PackageManager def;
public:
    /**
     * @return default instance
     */
    static PackageManager* getDefault();

    /**
     * @brief Constructor
     */
    PackageManager();

    virtual ~PackageManager();
};

#endif // PACKAGEMANAGER_H
