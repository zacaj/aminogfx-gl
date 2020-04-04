#ifndef _AMINOBASE_JS_H
#define _AMINOBASE_JS_H

#include <node.h>
#include <node_buffer.h>
#include <nan.h>

#include <map>
#include <memory>
#ifndef WIN
#include <pthread.h>
#else
typedef unsigned short ushort;
#include <mutex>
#include <thread>
#endif

#define ASYNC_UPDATE_PROPERTY      0
#define ASYNC_UPDATE_VALUE         1
#define ASYNC_JS_UPDATE_PROPERTY  10
#define ASYNC_JS_UPDATE_CALLBACK  11
#define ASYNC_UPDATE_CUSTOM      100

#define DEBUG_BASE false

#define DEBUG_THREADS false
#define DEBUG_RESOURCES false
#define DEBUG_REFERENCES false

class AminoJSObject;

/**
 * Factory object to create JS instance.
 */
class AminoJSObjectFactory {
public:
    std::string name;
    Nan::FunctionCallback callback;

    AminoJSObjectFactory(std::string name, Nan::FunctionCallback callback);

    virtual AminoJSObject* create();
};

class AminoJSEventObject;

/**
 * Basic JS object wrapper for Amino classes.
 *
 * Note: abstract.
 */
class AminoJSObject : public Nan::ObjectWrap {
protected:
    std::string name;
    AminoJSEventObject *eventHandler = NULL;
    bool destroyed = false;

    //stats
    static uint32_t activeInstances;
    static uint32_t totalInstances;
    static std::vector<AminoJSObject *> jsInstances;

    AminoJSObject(std::string name);
    ~AminoJSObject();

    virtual void preInit(Nan::NAN_METHOD_ARGS_TYPE info);
    virtual void setup();
    virtual void destroy();
    void destroyAminoJSObject();

    //properties
    void updateProperty(std::string name, v8::Local<v8::Value> &value);

    static std::string toString(v8::Local<v8::Value> &value);
    static std::string* toNewString(v8::Local<v8::Value> &value);

    static const int32_t PROPERTY_FLOAT        = 1;
    static const int32_t PROPERTY_FLOAT_ARRAY  = 2;
    static const int32_t PROPERTY_DOUBLE       = 3;
    static const int32_t PROPERTY_USHORT_ARRAY = 4;
    static const int32_t PROPERTY_INT32        = 5;
    static const int32_t PROPERTY_UINT32       = 6;
    static const int32_t PROPERTY_BOOLEAN      = 7;
    static const int32_t PROPERTY_UTF8         = 8;
    static const int32_t PROPERTY_OBJECT       = 9;

    class AsyncPropertyUpdate;

    class AnyProperty {
    public:
        int32_t type;
        AminoJSObject *obj;
        std::string name;
        uint32_t id;
        bool connected = false;

        AnyProperty(int32_t type, AminoJSObject *obj, std::string name, uint32_t id);
        virtual ~AnyProperty();

        virtual std::string toString() = 0;

        //sync handling
        virtual v8::Local<v8::Value> toValue() = 0;

        //async handling
        virtual void* getAsyncData(v8::Local<v8::Value> &value, bool &valid) = 0;
        virtual void setAsyncData(AsyncPropertyUpdate *update, void *data) = 0;
        virtual void freeAsyncData(void *data) = 0;

        //weak reference control (obj)
        void retain();
        void release();
    };

    class FloatProperty : public AnyProperty {
    public:
        float value = 0;

        FloatProperty(AminoJSObject *obj, std::string name, uint32_t id);
        ~FloatProperty();

        void setValue(float newValue);

        std::string toString() override;

        //sync handling
        v8::Local<v8::Value> toValue() override;

        //async handling
        void* getAsyncData(v8::Local<v8::Value> &value, bool &valid) override;
        void setAsyncData(AsyncPropertyUpdate *update, void *data) override;
        void freeAsyncData(void *data) override;
    };

    class FloatArrayProperty : public AnyProperty {
    public:
        std::vector<float> value;

        FloatArrayProperty(AminoJSObject *obj, std::string name, uint32_t id);
        ~FloatArrayProperty();

        void setValue(std::vector<float> newValue);

        std::string toString() override;

        //sync handling
        v8::Local<v8::Value> toValue() override;

        //async handling
        void* getAsyncData(v8::Local<v8::Value> &value, bool &valid) override;
        void setAsyncData(AsyncPropertyUpdate *update, void *data) override;
        void freeAsyncData(void *data) override;
    };

    class DoubleProperty : public AnyProperty {
    public:
        double value = 0;

        DoubleProperty(AminoJSObject *obj, std::string name, uint32_t id);
        ~DoubleProperty();

        void setValue(double newValue);

        std::string toString() override;

        //sync handling
        v8::Local<v8::Value> toValue() override;

        //async handling
        void* getAsyncData(v8::Local<v8::Value> &value, bool &valid) override;
        void setAsyncData(AsyncPropertyUpdate *update, void *data) override;
        void freeAsyncData(void *data) override;
    };

    class UShortArrayProperty : public AnyProperty {
    public:
        std::vector<ushort> value;

        UShortArrayProperty(AminoJSObject *obj, std::string name, uint32_t id);
        ~UShortArrayProperty();

        void setValue(std::vector<ushort> newValue);

        std::string toString() override;

        //sync handling
        v8::Local<v8::Value> toValue() override;

        //async handling
        void* getAsyncData(v8::Local<v8::Value> &value, bool &valid) override;
        void setAsyncData(AsyncPropertyUpdate *update, void *data) override;
        void freeAsyncData(void *data) override;
    };

    class Int32Property : public AnyProperty {
    public:
        int32_t value = 0;

        Int32Property(AminoJSObject *obj, std::string name, uint32_t id);
        ~Int32Property();

        void setValue(int32_t newValue);

        std::string toString() override;

        //sync handling
        v8::Local<v8::Value> toValue() override;

        //async handling
        void* getAsyncData(v8::Local<v8::Value> &value, bool &valid) override;
        void setAsyncData(AsyncPropertyUpdate *update, void *data) override;
        void freeAsyncData(void *data) override;
    };

    class UInt32Property : public AnyProperty {
    public:
        uint32_t value = 0;

        UInt32Property(AminoJSObject *obj, std::string name, uint32_t id);
        ~UInt32Property();

        void setValue(uint32_t newValue);

        std::string toString() override;

        //sync handling
        v8::Local<v8::Value> toValue() override;

        //async handling
        void* getAsyncData(v8::Local<v8::Value> &value, bool &valid) override;
        void setAsyncData(AsyncPropertyUpdate *update, void *data) override;
        void freeAsyncData(void *data) override;
    };

    class BooleanProperty : public AnyProperty {
    public:
        bool value = false;

        BooleanProperty(AminoJSObject *obj, std::string name, uint32_t id);
        ~BooleanProperty();

        void setValue(bool newValue);

        std::string toString() override;

        //sync handling
        v8::Local<v8::Value> toValue() override;

        //async handling
        void* getAsyncData(v8::Local<v8::Value> &value, bool &valid) override;
        void setAsyncData(AsyncPropertyUpdate *update, void *data) override;
        void freeAsyncData(void *data) override;
    };

    class Utf8Property : public AnyProperty {
    public:
        std::string value;

        Utf8Property(AminoJSObject *obj, std::string name, uint32_t id);
        ~Utf8Property();

        void setValue(std::string newValue);
        void setValue(char *newValue);

        std::string toString() override;

        //sync handling
        v8::Local<v8::Value> toValue() override;

        //async handling
        void* getAsyncData(v8::Local<v8::Value> &value, bool &valid) override;
        void setAsyncData(AsyncPropertyUpdate *update, void *data) override;
        void freeAsyncData(void *data) override;
    };

    class ObjectProperty : public AnyProperty {
    public:
        AminoJSObject *value = NULL;

        ObjectProperty(AminoJSObject *obj, std::string name, uint32_t id);
        ~ObjectProperty();

        void destroy();

        void setValue(AminoJSObject *newValue);

        std::string toString() override;

        //sync handling
        v8::Local<v8::Value> toValue() override;

        //async handling
        void* getAsyncData(v8::Local<v8::Value> &value, bool &valid) override;
        void setAsyncData(AsyncPropertyUpdate *update, void *data) override;
        void freeAsyncData(void *data) override;
    };

    void updateProperty(AnyProperty *property);

    FloatProperty* createFloatProperty(std::string name);
    FloatArrayProperty* createFloatArrayProperty(std::string name);
    DoubleProperty* createDoubleProperty(std::string name);
    UShortArrayProperty* createUShortArrayProperty(std::string name);
    Int32Property* createInt32Property(std::string name);
    UInt32Property* createUInt32Property(std::string name);
    BooleanProperty* createBooleanProperty(std::string name);
    Utf8Property* createUtf8Property(std::string name);
    ObjectProperty* createObjectProperty(std::string name);

    //async updates
    void setEventHandler(AminoJSEventObject *handler);
    void clearEventHandler();
    virtual bool isEventHandler();

    class AnyAsyncUpdate {
    public:
        int32_t type;

        AnyAsyncUpdate(int32_t type);
        virtual ~AnyAsyncUpdate();

        virtual void apply() = 0;
    };

    class AsyncPropertyUpdate : public AnyAsyncUpdate {
    public:
        AnyProperty *property;
        void *data;

        //helpers
        AminoJSObject *retainLater = NULL;
        AminoJSObject *releaseLater = NULL;

        AsyncPropertyUpdate(AnyProperty *property, void *data);
        ~AsyncPropertyUpdate();

        void apply();
    };

    class AsyncValueUpdate;
    typedef void (AminoJSObject::*asyncValueCallback)(AsyncValueUpdate *, int state);

    class AsyncValueUpdate : public AnyAsyncUpdate {
    public:
        AminoJSObject *obj;
        void *data = NULL;

        //values
        Nan::Persistent<v8::Value> *valuePersistent = NULL;
        AminoJSObject *valueObj = NULL;
        unsigned int valueUint32 = 0;

        asyncValueCallback callback = NULL;

        //helpers
        AminoJSObject *releaseLater = NULL;

        static const int STATE_CREATE = 0;
        static const int STATE_APPLY  = 1;
        static const int STATE_DELETE = 2;

        AsyncValueUpdate(AminoJSObject *obj, AminoJSObject *value, asyncValueCallback callback);
        AsyncValueUpdate(AminoJSObject *obj, unsigned int value, void *data, asyncValueCallback callback);
        AsyncValueUpdate(AminoJSObject *obj, v8::Local<v8::Value> &value, void *data, asyncValueCallback callback);
        ~AsyncValueUpdate();

        void apply() override;
    };

    bool enqueueValueUpdate(AminoJSObject *value, asyncValueCallback callback);
    bool enqueueValueUpdate(unsigned int value, void *data, asyncValueCallback callback);
    bool enqueueValueUpdate(v8::Local<v8::Value> &value, void *data, asyncValueCallback callback);

    virtual bool enqueueValueUpdate(AsyncValueUpdate *update);

    class JSPropertyUpdate: public AnyAsyncUpdate {
    public:
        JSPropertyUpdate(AnyProperty *property);
        ~JSPropertyUpdate();

        void apply() override;
    private:
        AnyProperty *property;
    };

    class JSCallbackUpdate;
    typedef void (AminoJSObject::*jsUpdateCallback)(JSCallbackUpdate *);

    class JSCallbackUpdate: public AnyAsyncUpdate {
    public:
        AminoJSObject *obj;
        jsUpdateCallback callbackApply;
        jsUpdateCallback callbackDone;
        void *data;

        JSCallbackUpdate(AminoJSObject *obj, jsUpdateCallback callbackApply, jsUpdateCallback callbackDone, void *data);
        ~JSCallbackUpdate();

        void apply() override;
    };

    bool enqueueJSCallbackUpdate(jsUpdateCallback callbackApply, jsUpdateCallback callbackDone, void *data);

    //static methods
    static v8::Local<v8::FunctionTemplate> createTemplate(AminoJSObjectFactory* factory);
    static void createInstance(Nan::NAN_METHOD_ARGS_TYPE info, AminoJSObjectFactory* factory);

private:
    //properties
    uint32_t lastPropertyId = 0;
    std::map<uint32_t, AnyProperty *> propertyMap;

    bool addPropertyWatcher(std::string name, uint32_t id, v8::Local<v8::Value> &jsValue);
    void addProperty(AnyProperty *prop);

    //async updates
    bool enqueuePropertyUpdate(uint32_t id, v8::Local<v8::Value> &value);
    static NAN_METHOD(PropertyUpdated);
    static Nan::Persistent<v8::Function> *propertyUpdatedFunc;

    //JS updates
    virtual bool enqueueJSPropertyUpdate(AnyProperty *prop);

public:
    std::string getName();

    void retain();
    void release();
    int getReferenceCount();

    AnyProperty* getPropertyWithId(uint32_t id);
    AnyProperty* getPropertyWithName(std::string name);

    virtual AminoJSEventObject* getEventHandler();

    virtual bool handleSyncUpdate(AnyProperty *property, void *data);
    virtual void handleAsyncUpdate(AsyncPropertyUpdate *update);
    virtual bool handleAsyncUpdate(AsyncValueUpdate *update);
};

/**
 * Amino JS Object with event handler.
 */
class AminoJSEventObject : public AminoJSObject {
public:
    AminoJSEventObject(std::string name);
    ~AminoJSEventObject();

    AminoJSEventObject* getEventHandler() override;

    bool enqueuePropertyUpdate(AnyProperty *prop, v8::Local<v8::Value> &value);
    bool enqueueValueUpdate(AsyncValueUpdate *update) override;

    bool enqueueJSPropertyUpdate(AnyProperty *prop) override;
    bool enqueueJSUpdate(AnyAsyncUpdate *update);

    bool isMainThread();

protected:
    bool isEventHandler() override;
    void processAsyncQueue();
    void clearAsyncQueue();
    void handleAsyncDeletes();
    void handleJSUpdates();

    virtual void getStats(v8::Local<v8::Object> &obj);

private:
    std::vector<AnyAsyncUpdate *> *asyncUpdates = NULL;
    std::vector<AnyAsyncUpdate *> *asyncDeletes = NULL;
    std::vector<AnyAsyncUpdate *> *jsUpdates = NULL;

    std::thread::id mainThread;
    std::recursive_mutex asyncLock; //Note: can block for a while
};

#endif
