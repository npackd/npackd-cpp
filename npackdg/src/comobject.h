#ifndef COMOBJECT_H
#define COMOBJECT_H

#include <type_traits>
#include <windows.h>

template <typename T>
class COMObject
{
public:
    T* ptr;

    COMObject(T* aptr=nullptr);
    virtual ~COMObject();

    T* operator ->();
};

template<typename T>
COMObject<T>::COMObject(T *aptr): ptr(aptr)
{
    static_assert(std::is_base_of<IDispatch, T>::value, "type parameter of this class must derive from IDispatch");
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
