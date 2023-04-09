#ifndef COMOBJECT_H
#define COMOBJECT_H

#include <type_traits>
#include <windows.h>
#include <objbase.h>

/**
 * Wrapper around a COM interface
 */
template <typename T>
class COMObject
{
public:
    /**
     * the pointer itself or 0. Release() is
     * called on this pointer on destruction.
     */
    T* ptr;

    /**
     * @brief creates a wrapper
     *
     * @param [move] aptr a COM pointer
     */
    COMObject(T* aptr=nullptr);

    virtual ~COMObject();

    /**
     * @return the wrapped pointer or 0
     */
    T* operator ->();
};

template<typename T>
COMObject<T>::COMObject(T *aptr): ptr(aptr)
{
    static_assert(std::is_base_of<IUnknown, T>::value,
            "type parameter of this class must derive from IUnknown");
}

template<typename T>
COMObject<T>::~COMObject()
{
    if (ptr) {
        ptr->Release();
        ptr = nullptr;
    }
}

template<typename T>
T *COMObject<T>::operator ->()
{
    return ptr;
}


#endif // COMOBJECT_H
