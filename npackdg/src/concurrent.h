#ifndef CONCURRENT_H
#define CONCURRENT_H

#include <QRunnable>
#include <QFuture>
#include <QThreadPool>

template <typename T>
class RunFunctionTaskBase : public QFutureInterface<T> , public QRunnable
{
public:
    QFuture<T> start(QThreadPool* tp)
    {
        this->setRunnable(this);
        this->reportStarted();
        QFuture<T> theFuture = this->future();
        tp->start(this, /*m_priority*/ 0);
        return theFuture;
    }

    void run() {}
    virtual void runFunctor() = 0;
};

template <typename T>
class RunFunctionTask : public RunFunctionTaskBase<T>
{
public:
    void run()
    {
        if (this->isCanceled()) {
            this->reportFinished();
            return;
        }
#ifndef QT_NO_EXCEPTIONS
        try {
#endif
            this->runFunctor();
#ifndef QT_NO_EXCEPTIONS
        } catch (QException &e) {
            QFutureInterface<T>::reportException(e);
        } catch (...) {
            QFutureInterface<T>::reportException(QUnhandledException());
        }
#endif

        this->reportResult(result);
        this->reportFinished();
    }
    T result;
};

template <typename T, typename Class, typename Param1, typename Arg1>
class VoidStoredMemberFunctionPointerCall1 : public RunFunctionTask<T>
{
public:
    VoidStoredMemberFunctionPointerCall1(T (Class::*_fn)(Param1) , Class *_object, const Arg1 &_arg1)
    : fn(_fn), object(_object), arg1(_arg1){ }

    void runFunctor()
    {
        (object->*fn)(arg1);
    }
private:
    T (Class::*fn)(Param1);
    Class *object;
    Arg1 arg1;
};

template <typename T, typename Class, typename Param1, typename Arg1>
class StoredMemberFunctionPointerCall1 : public RunFunctionTask<T>
{
public:
    StoredMemberFunctionPointerCall1(T (Class::*_fn)(Param1) , Class *_object, const Arg1 &_arg1)
    : fn(_fn), object(_object), arg1(_arg1){ }

    void runFunctor()
    {
        this->result = (object->*fn)(arg1);
    }
private:
    T (Class::*fn)(Param1);
    Class *object;
    Arg1 arg1;
};

template <typename T, typename Class, typename Param1, typename Arg1>
struct SelectStoredMemberFunctionPointerCall1
{
    typedef typename QtConcurrent::SelectSpecialization<T>::template
        Type<StoredMemberFunctionPointerCall1    <T, Class, Param1, Arg1>,
             VoidStoredMemberFunctionPointerCall1<T, Class, Param1, Arg1> >::type type;
};

template <typename T, typename Class, typename Param1, typename Arg1>
QFuture<T> run(QThreadPool* tp, Class *object, T (Class::*fn)(Param1),
        const Arg1 &arg1)
{
    return (new typename SelectStoredMemberFunctionPointerCall1<T,
            Class, Param1, Arg1>::type(fn, object, arg1))->start(tp);
}

#endif // CONCURRENT_H
