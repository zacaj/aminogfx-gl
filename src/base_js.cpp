#include "base_js.h"

#include <sstream>

#define DEBUG_ASYNC false
#define DEBUG_JS_INSTANCES false

//
//  AminoJSObjectFactory
//

/**
 * Create factory for AminoJSObject creation.
 */
AminoJSObjectFactory::AminoJSObjectFactory(std::string name, Nan::FunctionCallback callback): name(name), callback(callback) {
    //empty
}

/**
 * Create new object instance.
 */
AminoJSObject* AminoJSObjectFactory::create() {
    assert(false);

    return NULL;
}

//
//  AminoJSObject
//

/**
 * Constructor.
 */
AminoJSObject::AminoJSObject(std::string name): name(name) {
    if (DEBUG_BASE) {
        printf("%s constructor\n", name.c_str());
    }

    activeInstances++;
    totalInstances++;

    if (DEBUG_JS_INSTANCES) {
        jsInstances.push_back(this);
    }
}

/**
 * Destructor.
 *
 * Note: has to be called on main thread.
 */
AminoJSObject::~AminoJSObject() {
    if (DEBUG_BASE) {
        printf("%s destructor\n", name.c_str());
    }

    if (!destroyed) {
        destroyAminoJSObject();
    }

    //free properties
    for (std::map<uint32_t, AnyProperty *>::iterator iter = propertyMap.begin(); iter != propertyMap.end(); iter++) {
        delete iter->second;
    }

    //debug
    //printf("deleted %i properties\n", (int)propertyMap.size());

    propertyMap.clear();

    //instance count
    activeInstances--;

    if (DEBUG_JS_INSTANCES) {
        std::vector<AminoJSObject *>::iterator pos = std::find(jsInstances.begin(), jsInstances.end(), this);

        assert(pos != jsInstances.end());

        jsInstances.erase(pos);
    }
}

/**
 * Get JS class name.
 *
 * Note: abstract
 */
std::string AminoJSObject::getName() {
    return name;
}

/**
 * Initialize the native object with parameters passed to the constructor. Called before JS init().
 */
void AminoJSObject::preInit(Nan::NAN_METHOD_ARGS_TYPE info) {
    //empty
}

/**
 * Initialize the native object.
 *
 * Called after the JS init() method.
 */
void AminoJSObject::setup() {
    //empty
}

/**
 * Free all resources.
 *
 * Note: has to be called on main thread.
 */
void AminoJSObject::destroy() {
    if (destroyed) {
        return;
    }

    destroyed = true;

    destroyAminoJSObject();

    //end of destroy chain
}

/**
 * Free all resources used by this instance.
 */
void AminoJSObject::destroyAminoJSObject() {
    //event handler
    clearEventHandler();
}

/**
 * Retain JS reference.
 */
void AminoJSObject::retain() {
    Ref();

    if (DEBUG_REFERENCES) {
        printf("--- %s references: %i (+1)\n", name.c_str(), refs_);
    }
}

/**
 * Release JS reference.
 */
void AminoJSObject::release() {
    Unref();

    if (DEBUG_REFERENCES) {
        printf("--- %s references: %i (-1)\n", name.c_str(), refs_);
    }
}

/**
 * Get reference counter.
 */
int AminoJSObject::getReferenceCount() {
    return refs_;
}

/**
 * Create JS function template from factory.
 */
v8::Local<v8::FunctionTemplate> AminoJSObject::createTemplate(AminoJSObjectFactory* factory) {
    if (DEBUG_BASE) {
        printf("%s template created\n", factory->name.c_str());
    }

    //initialize template (bound to New method)
    v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(factory->callback);

    tpl->SetClassName(Nan::New(factory->name).ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1); //object reference only stored

    return tpl;
}

/**
 * Create JS and native instance from factory.
 */
void AminoJSObject::createInstance(Nan::NAN_METHOD_ARGS_TYPE info, AminoJSObjectFactory* factory) {
    if (DEBUG_BASE) {
        printf("-> new %s()\n", factory->name.c_str());
    }

    //check constructor call
    if (!info.IsConstructCall()) {
        //called as plain function (e.g. in extended class)
        Nan::ThrowTypeError("please use new AminoObj() instead of function call");
        return;
    }

    //new AminoObj()

    //create new instance
    AminoJSObject *obj = factory->create();

    assert(obj);

    //bind to C++ instance
    obj->Wrap(info.This());

    //pre-init
    obj->preInit(info);

    //call init (if available)
    Nan::MaybeLocal<v8::Value> initValue = Nan::Get(info.This(), Nan::New<v8::String>("init").ToLocalChecked());

    if (!initValue.IsEmpty()) {
        v8::Local<v8::Value> initLocal = initValue.ToLocalChecked();

        if (initLocal->IsFunction()) {
            v8::Local<v8::Function> initFunc = initLocal.As<v8::Function>();

            //call
            int argc = 0;
            v8::Local<v8::Value> argv[1];

            initFunc->Call(info.This(), argc, argv);
        }
    }

    //native setup
    obj->setup();

    //call initDone (if available)
    Nan::MaybeLocal<v8::Value> initDoneValue = Nan::Get(info.This(), Nan::New<v8::String>("initDone").ToLocalChecked());

    if (!initDoneValue.IsEmpty()) {
        v8::Local<v8::Value> initDoneLocal = initDoneValue.ToLocalChecked();

        if (initDoneLocal->IsFunction()) {
            v8::Local<v8::Function> initDoneFunc = initDoneLocal.As<v8::Function>();

            //call
            int argc = 0;
            v8::Local<v8::Value> argv[1];

            initDoneFunc->Call(info.This(), argc, argv);
        }
    }

    info.GetReturnValue().Set(info.This());
}

/**
 * Watch JS property changes.
 *
 * Note: has to be called in JS scope of setup()!
 */
bool AminoJSObject::addPropertyWatcher(std::string name, uint32_t id, v8::Local<v8::Value> &jsValue) {
    if (DEBUG_BASE) {
        printf("addPropertyWatcher(): %s\n", name.c_str());
    }

    //get property object
    Nan::MaybeLocal<v8::Value> prop = Nan::Get(handle(), Nan::New<v8::String>(name).ToLocalChecked());

    if (prop.IsEmpty()) {
        if (DEBUG_BASE) {
            printf("-> property not defined: %s\n", name.c_str());
        }

        return false;
    }

    v8::Local<v8::Value> propLocal = prop.ToLocalChecked();

    if (!propLocal->IsObject()) {
        if (DEBUG_BASE) {
            printf("-> property not an object: %s in %s\n", name.c_str(), this->name.c_str());
        }

        return false;
    }

    v8::Local<v8::Object> obj = propLocal.As<v8::Object>();

    //set nativeListener value
    if (!propertyUpdatedFunc) {
        propertyUpdatedFunc = new Nan::Persistent<v8::Function>();
        propertyUpdatedFunc->Reset(Nan::New<v8::Function>(PropertyUpdated));
    }

    //Note: memory leak due to v8::Function reference which is never freed!!! Workaround: use single instance to leak a single item.
    //Nan::Set(obj, Nan::New<v8::String>("nativeListener").ToLocalChecked(), Nan::New<v8::Function>(PropertyUpdated));

    Nan::Set(obj, Nan::New<v8::String>("nativeListener").ToLocalChecked(), Nan::New<v8::Function>(*propertyUpdatedFunc));

    //set propId value
    Nan::Set(obj, Nan::New<v8::String>("propId").ToLocalChecked(), Nan::New<v8::Integer>(id));

    //default JS value
    Nan::MaybeLocal<v8::Value> valueMaybe = Nan::Get(obj, Nan::New<v8::String>("value").ToLocalChecked());

    if (valueMaybe.IsEmpty()) {
        jsValue = Nan::Undefined();
    } else {
        jsValue = valueMaybe.ToLocalChecked();

        if (DEBUG_BASE) {
            Nan::Utf8String str(jsValue);

            printf("-> default value: %s\n", *str);
        }
    }

    return true;
}

Nan::Persistent<v8::Function>* AminoJSObject::propertyUpdatedFunc = NULL;

/**
 * Create float property (bound to JS property).
 *
 * Note: has to be called in JS scope of setup()!
 */
AminoJSObject::FloatProperty* AminoJSObject::createFloatProperty(std::string name) {
    uint32_t id = ++lastPropertyId;
    FloatProperty *prop = new FloatProperty(this, name, id);

    addProperty(prop);

    return prop;
}

/**
 * Create float array property (bound to JS property).
 *
 * Note: has to be called in JS scope of setup()!
 */
AminoJSObject::FloatArrayProperty* AminoJSObject::createFloatArrayProperty(std::string name) {
    uint32_t id = ++lastPropertyId;
    FloatArrayProperty *prop = new FloatArrayProperty(this, name, id);

    addProperty(prop);

    return prop;
}

/**
 * Create float property (bound to JS property).
 *
 * Note: has to be called in JS scope of setup()!
 */
AminoJSObject::DoubleProperty* AminoJSObject::createDoubleProperty(std::string name) {
    uint32_t id = ++lastPropertyId;
    DoubleProperty *prop = new DoubleProperty(this, name, id);

    addProperty(prop);

    return prop;
}

/**
 * Create ushort array property (bound to JS property).
 *
 * Note: has to be called in JS scope of setup()!
 */
AminoJSObject::UShortArrayProperty* AminoJSObject::createUShortArrayProperty(std::string name) {
    uint32_t id = ++lastPropertyId;
    UShortArrayProperty *prop = new UShortArrayProperty(this, name, id);

    addProperty(prop);

    return prop;
}

/**
 * Create int32 property (bound to JS property).
 *
 * Note: has to be called in JS scope of setup()!
 */
AminoJSObject::Int32Property* AminoJSObject::createInt32Property(std::string name) {
    uint32_t id = ++lastPropertyId;
    Int32Property *prop = new Int32Property(this, name, id);

    addProperty(prop);

    return prop;
}

/**
 * Create unsigned int32 property (bound to JS property).
 *
 * Note: has to be called in JS scope of setup()!
 */
AminoJSObject::UInt32Property* AminoJSObject::createUInt32Property(std::string name) {
    uint32_t id = ++lastPropertyId;
    UInt32Property *prop = new UInt32Property(this, name, id);

    addProperty(prop);

    return prop;
}

/**
 * Create boolean property (bound to JS property).
 *
 * Note: has to be called in JS scope of setup()!
 */
AminoJSObject::BooleanProperty* AminoJSObject::createBooleanProperty(std::string name) {
    uint32_t id = ++lastPropertyId;
    BooleanProperty *prop = new BooleanProperty(this, name, id);

    addProperty(prop);

    return prop;
}

/**
 * Create UTF8 string property (bound to JS property).
 *
 * Note: has to be called in JS scope of setup()!
 */
AminoJSObject::Utf8Property* AminoJSObject::createUtf8Property(std::string name) {
    uint32_t id = ++lastPropertyId;
    Utf8Property *prop = new Utf8Property(this, name, id);

    addProperty(prop);

    return prop;
}

/**
 * Create object property (bound to JS property).
 *
 * Note: has to be called in JS scope of setup()!
 */
AminoJSObject::ObjectProperty* AminoJSObject::createObjectProperty(std::string name) {
    uint32_t id = ++lastPropertyId;
    ObjectProperty *prop = new ObjectProperty(this, name, id);

    addProperty(prop);

    return prop;
}

/**
 * Bind a property to a watcher.
 *
 * Note: has to be called in JS scope of setup()!
 */
void AminoJSObject::addProperty(AnyProperty *prop) {
    assert(prop);

    if (DEBUG_BASE) {
        assert(getPropertyWithName(prop->name) == NULL);
    }

    int id = prop->id;

    propertyMap[id] = prop;

    v8::Local<v8::Value> value;

    if (addPropertyWatcher(prop->name, id, value)) {
        prop->connected = true;

        //set default value
        bool valid = false;
        void *data = prop->getAsyncData(value, valid);

        if (valid) {
            prop->setAsyncData(NULL, data);
            prop->freeAsyncData(data);
        }
    }
}

/**
 * Callback from property watcher to update native value.
 */
NAN_METHOD(AminoJSObject::PropertyUpdated) {
    assert(info.Length() == 3);

    //params: value, propId, object
    uint32_t id = Nan::To<v8::Uint32>(info[1]).ToLocalChecked()->Value();

    //pass to object instance
    v8::Local<v8::Value> value = info[0];
    v8::Local<v8::Object> jsObj = info[2].As<v8::Object>();
    AminoJSObject *obj = Nan::ObjectWrap::Unwrap<AminoJSObject>(jsObj);

    assert(obj);

    obj->enqueuePropertyUpdate(id, value);
}

/**
 * Set the event handler instance.
 *
 * Note: retains the new event handler object instance.
 */
void AminoJSObject::setEventHandler(AminoJSEventObject *handler) {
    if (eventHandler != handler) {
        if (eventHandler) {
            eventHandler->release();
        }

        eventHandler = handler;

        //retain handler instance (in addition to 'amino' JS property)
        if (handler) {
            assert(!destroyed);

            handler->retain();
        }
    }
}

/**
 * Remove event handler.
 */
void AminoJSObject::clearEventHandler() {
    setEventHandler(NULL);
}

/**
 * This is not an event handeler.
 */
bool AminoJSObject::isEventHandler() {
    return false;
}

/**
 * Enqueue a value update.
 *
 * Note: has to be called on main thread.
 */
bool AminoJSObject::enqueueValueUpdate(AminoJSObject *value, asyncValueCallback callback) {
    return enqueueValueUpdate(new AsyncValueUpdate(this, value, callback));
}

/**
 * Enqueue a value update.
 *
 * Note: has to be called on main thread.
 */
bool AminoJSObject::enqueueValueUpdate(unsigned int value, void *data, asyncValueCallback callback) {
    return enqueueValueUpdate(new AsyncValueUpdate(this, value, data, callback));
}

/**
 * Enqueue a value update.
 *
 * Note: has to be called on main thread.
 */
bool AminoJSObject::enqueueValueUpdate(v8::Local<v8::Value> &value, void *data, asyncValueCallback callback) {
    return enqueueValueUpdate(new AsyncValueUpdate(this, value, data, callback));
}

/**
 * Enqueue a value update.
 *
 * Note: called on main thread.
 */
bool AminoJSObject::enqueueValueUpdate(AsyncValueUpdate *update) {
    AminoJSEventObject *eventHandler = getEventHandler();

    if (eventHandler) {
        return eventHandler->enqueueValueUpdate(update);
    }

    printf("missing queue: %s\n", name.c_str());

    delete update;

    return false;
}

/**
 * Handly property update on main thread (optional).
 */
bool AminoJSObject::handleSyncUpdate(AnyProperty *property, void *data) {
    //no default handling
    return false;
}

/**
 * Default implementation sets the value received from JS.
 */
void AminoJSObject::handleAsyncUpdate(AsyncPropertyUpdate *update) {
    //overwrite for extended handling

    if (DEBUG_BASE) {
        AnyProperty *property = update->property;

        printf("-> updating %s in %s\n", property->name.c_str(), property->obj->name.c_str());
    }

    assert(update);

    //default: update value
    update->apply();

    if (DEBUG_BASE) {
        AnyProperty *property = update->property;
        std::string str = property->toString();

        printf("-> updated %s in %s to %s\n", property->name.c_str(), property->obj->name.c_str(), str.c_str());
    }
}

/**
 * Custom handler for implementation specific async update.
 */
bool AminoJSObject::handleAsyncUpdate(AsyncValueUpdate *update) {
    if (DEBUG_BASE) {
        printf("handleAsyncUpdate(AsyncValueUpdate)\n");
    }
    //overwrite

    assert(update);

    if (update->callback) {
        if (DEBUG_BASE) {
            printf(" -> calling callback\n");
        }

        update->apply();

        if (DEBUG_BASE) {
            printf(" -> done\n");
        }

        return true;
    }

    return false;
}

/**
 * Get the event handler.
 */
AminoJSEventObject* AminoJSObject::getEventHandler() {
    return eventHandler;
}

/**
 * Enqueue a property update.
 *
 * Note: called on main thread.
 */
bool AminoJSObject::enqueuePropertyUpdate(uint32_t id, v8::Local<v8::Value> &value) {
    //check queue exists
    AminoJSEventObject *eventHandler = getEventHandler();

    assert(eventHandler);

    //find property
    AnyProperty *prop = getPropertyWithId(id);

    assert(prop);

    //enqueue (in event handler)
    if (DEBUG_BASE) {
        printf("enqueuePropertyUpdate: %s (id=%i)\n", prop->name.c_str(), id);
    }

    return eventHandler->enqueuePropertyUpdate(prop, value);
}

/**
 * Enqueue a JS property update (update JS value).
 *
 * Note: thread-safe.
 */
bool AminoJSObject::enqueueJSPropertyUpdate(AnyProperty *prop) {
    //check queue exists
    AminoJSEventObject *eventHandler = getEventHandler();

    if (eventHandler) {
        return eventHandler->enqueueJSPropertyUpdate(prop);
    }

    if (DEBUG_BASE) {
        printf("Missing event handler in %s\n", name.c_str());
    }

    return false;
}

/**
 * Enqueue JS callback update.
 *
 * Note: thread-safe.
 */
bool AminoJSObject::enqueueJSCallbackUpdate(jsUpdateCallback callbackApply, jsUpdateCallback callbackDone, void *data) {
    //check queue exists
    AminoJSEventObject *eventHandler = getEventHandler();

    if (eventHandler) {
        return eventHandler->enqueueJSUpdate(new JSCallbackUpdate(this, callbackApply, callbackDone, data));
    }

    if (DEBUG_BASE) {
        printf("Missing event handler in %s\n", name.c_str());
    }

    return false;
}

/**
 * Get property with id.
 */
AminoJSObject::AnyProperty* AminoJSObject::getPropertyWithId(uint32_t id) {
    std::map<uint32_t, AnyProperty *>::iterator iter = propertyMap.find(id);

    if (iter == propertyMap.end()) {
        //property not found
        return NULL;
    }

    return iter->second;
}

/**
 * Get property with name.
 */
AminoJSObject::AnyProperty* AminoJSObject::getPropertyWithName(std::string name) {
    for (std::map<uint32_t, AnyProperty *>::iterator iter = propertyMap.begin(); iter != propertyMap.end(); iter++) {
        if (iter->second->name == name) {
            return iter->second;
        }
    }

    return NULL;
}

/**
 * Update JS property value.
 *
 * Note: has to be called on main thread.
 */
void AminoJSObject::updateProperty(std::string name, v8::Local<v8::Value> &value) {
    if (DEBUG_BASE) {
        printf("updateProperty(): %s\n", name.c_str());
    }

    //get property function
    v8::Local<v8::Object> obj = handle();
    Nan::MaybeLocal<v8::Value> prop = Nan::Get(obj, Nan::New<v8::String>(name).ToLocalChecked());

    if (prop.IsEmpty()) {
        if (DEBUG_BASE) {
            printf("-> property not defined: %s\n", name.c_str());
        }

        return;
    }

    v8::Local<v8::Value> propLocal = prop.ToLocalChecked();

    if (!propLocal->IsFunction()) {
        if (DEBUG_BASE) {
            printf("-> property not a function: %s\n", name.c_str());
        }

        return;
    }

    v8::Local<v8::Function> updateFunc = propLocal.As<v8::Function>();

    //call
    int argc = 2;
    v8::Local<v8::Value> argv[] = { value, Nan::True() };

    updateFunc->Call(obj, argc, argv);

    if (DEBUG_BASE) {
        std::string str = toString(value);

        printf("-> updated property: %s to %s\n", name.c_str(), str.c_str());
    }
}

/**
 * Update a property value.
 *
 * Note: call is thread-safe.
 */
void AminoJSObject::updateProperty(AnyProperty *property) {
    //debug
    //printf("updateProperty() %s of %s\n", property->name.c_str(), name.c_str());

    assert(property);

    AminoJSEventObject *eventHandler = getEventHandler();

    assert(eventHandler);

    if (eventHandler->isMainThread()) {
        //create scope
        Nan::HandleScope scope;

        v8::Local<v8::Value> value = property->toValue();

        property->obj->updateProperty(property->name, value);
    } else {
        enqueueJSPropertyUpdate(property);
    }
}

/**
 * Convert a JS value to a string.
 */
std::string AminoJSObject::toString(v8::Local<v8::Value> &value) {
    //convert anything to a string
    Nan::Utf8String str(value);

    //convert it to string
    return std::string(*str);
}

/**
 * Convert a JS value to a string.
 *
 * Note: instance has to be deleted!
 */
std::string* AminoJSObject::toNewString(v8::Local<v8::Value> &value) {
    //convert anything to a string
    Nan::Utf8String str(value);

    //convert it to string
    return new std::string(*str);
}

//static initializers
uint32_t AminoJSObject::activeInstances = 0;
uint32_t AminoJSObject::totalInstances = 0;
std::vector<AminoJSObject *> AminoJSObject::jsInstances;

//
// AminoJSObject::AnyProperty
//

/**
 * AnyProperty constructor.
 */
AminoJSObject::AnyProperty::AnyProperty(int type, AminoJSObject *obj, std::string name, uint32_t id): type(type), obj(obj), name(name), id(id) {
    //empty

    assert(obj);
}

AminoJSObject::AnyProperty::~AnyProperty() {
    //empty
}

/**
 * Retain base object instance.
 *
 * Note: has to be called on v8 thread!
 */
void AminoJSObject::AnyProperty::retain() {
    obj->retain();
}

/**
 * Release base object instance.
 *
 * Note: has to be called on v8 thread!
 */
void AminoJSObject::AnyProperty::release() {
    obj->release();
}

//
// AminoJSObject::FloatProperty
//

/**
 * FloatProperty constructor.
 */
AminoJSObject::FloatProperty::FloatProperty(AminoJSObject *obj, std::string name, uint32_t id): AnyProperty(PROPERTY_FLOAT, obj, name, id) {
    //empty
}

/**
 * FloatProperty destructor.
 */
AminoJSObject::FloatProperty::~FloatProperty() {
    //empty
}

/**
 * Update the float value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::FloatProperty::setValue(float newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::FloatProperty::toString() {
    return std::to_string(value);
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::FloatProperty::toValue() {
    return Nan::New<v8::Number>(value);
}

/**
 * Get async data representation.
 */
void* AminoJSObject::FloatProperty::getAsyncData(v8::Local<v8::Value> &value, bool &valid) {
    if (value->IsNumber()) {
        //double to float
        float f = Nan::To<v8::Number>(value).ToLocalChecked()->Value();
        float *res = new float;

        *res = f;
        valid = true;

        return res;
    } else {
        if (DEBUG_BASE) {
            printf("-> default value not a number!\n");
        }

        valid = false;

        return NULL;
    }
}

/**
 * Apply async data.
 */
void AminoJSObject::FloatProperty::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    if (data) {
        value = *((float *)data);
    }
}

/**
 * Free async data.
 */
void AminoJSObject::FloatProperty::freeAsyncData(void *data) {
    if (data) {
        delete (float *)data;
    }
}

//
// AminoJSObject::FloatArrayProperty
//

/**
 * FloatArrayProperty constructor.
 */
AminoJSObject::FloatArrayProperty::FloatArrayProperty(AminoJSObject *obj, std::string name, uint32_t id): AnyProperty(PROPERTY_FLOAT_ARRAY, obj, name, id) {
    //empty
}

/**
 * FloatProperty destructor.
 */
AminoJSObject::FloatArrayProperty::~FloatArrayProperty() {
    //empty
}

/**
 * Update the float value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::FloatArrayProperty::setValue(std::vector<float> newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::FloatArrayProperty::toString() {
    std::ostringstream ss;
    std::size_t count = value.size();

    ss << "[";

    for (unsigned int i = 0; i < count; i++) {
        if (i > 0) {
            ss << ", ";
        }

        ss << value[i];
    }

    ss << "]";

    return std::string(ss.str());
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::FloatArrayProperty::toValue() {
    std::size_t count = value.size();

    //typed array
    v8::Local<v8::Float32Array> arr = v8::Float32Array::New(v8::ArrayBuffer::New(v8::Isolate::GetCurrent(), count * sizeof(float)), 0, count);

    //normal array
    //v8::Local<v8::Array> arr = Nan::New<v8::Array>();

    for (unsigned int i = 0; i < count; i++) {
        Nan::Set(arr, Nan::New<v8::Uint32>(i), Nan::New<v8::Number>(value[i]));
    }

    return arr;
}

/**
 * Get async data representation.
 */
void* AminoJSObject::FloatArrayProperty::getAsyncData(v8::Local<v8::Value> &value, bool &valid) {
    if (value->IsNull()) {
        //Note: only accepting empty arrays as values
        valid = false;

        return NULL;
    }

    std::vector<float> *vector =  NULL;

    if (value->IsFloat32Array()) {
        //Float32Array
        v8::Handle<v8::Float32Array> arr = v8::Handle<v8::Float32Array>::Cast(value);
        v8::ArrayBuffer::Contents contents = arr->Buffer()->GetContents();
        float *data = (float *)contents.Data();
        std::size_t count = contents.ByteLength() / sizeof(float);

        //debug
        //printf("is Float32Array (size: %i)\n", (int)count);

        //copy to vector
        vector = new std::vector<float>();

        vector->assign(data, data + count);

        valid = true;
    } else if (value->IsArray()) {
        v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast(value);
        std::size_t count = arr->Length();

        vector = new std::vector<float>();

        for (std::size_t i = 0; i < count; i++) {
            vector->push_back((float)(Nan::To<v8::Number>(arr->Get(i)).ToLocalChecked()->Value()));
        }

        valid = true;
    } else {
        valid = false;
    }

    return vector;
}

/**
 * Apply async data.
 */
void AminoJSObject::FloatArrayProperty::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    if (!data) {
        value.clear();
        return;
    }

    value = *((std::vector<float> *)data);
}

/**
 * Free async data.
 */
void AminoJSObject::FloatArrayProperty::freeAsyncData(void *data) {
    if (data) {
        delete (std::vector<float> *)data;
    }
}

//
// AminoJSObject::DoubleProperty
//

/**
 * DoubleProperty constructor.
 */
AminoJSObject::DoubleProperty::DoubleProperty(AminoJSObject *obj, std::string name, uint32_t id): AnyProperty(PROPERTY_DOUBLE, obj, name, id) {
    //empty
}

/**
 * DoubleProperty destructor.
 */
AminoJSObject::DoubleProperty::~DoubleProperty() {
    //empty
}

/**
 * Update the double value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::DoubleProperty::setValue(double newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::DoubleProperty::toString() {
    return std::to_string(value);
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::DoubleProperty::toValue() {
    return Nan::New<v8::Number>(value);
}

/**
 * Get async data representation.
 */
void* AminoJSObject::DoubleProperty::getAsyncData(v8::Local<v8::Value> &value, bool &valid) {
    if (value->IsNumber()) {
        double d = Nan::To<v8::Number>(value).ToLocalChecked()->Value();
        double *res = new double;

        *res = d;
        valid = true;

        return res;
    } else {
        if (DEBUG_BASE) {
            printf("-> default value not a number!\n");
        }

        valid = false;

        return NULL;
    }
}

/**
 * Apply async data.
 */
void AminoJSObject::DoubleProperty::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    if (data) {
        value = *((double *)data);
    }
}

/**
 * Free async data.
 */
void AminoJSObject::DoubleProperty::freeAsyncData(void *data) {
    if (data) {
        delete (double *)data;
    }
}

//
// AminoJSObject::UShortArrayProperty
//

/**
 * UShortArrayProperty constructor.
 */
AminoJSObject::UShortArrayProperty::UShortArrayProperty(AminoJSObject *obj, std::string name, uint32_t id): AnyProperty(PROPERTY_USHORT_ARRAY, obj, name, id) {
    //empty
}

/**
 * FloatProperty destructor.
 */
AminoJSObject::UShortArrayProperty::~UShortArrayProperty() {
    //empty
}

/**
 * Update the float value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::UShortArrayProperty::setValue(std::vector<ushort> newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::UShortArrayProperty::toString() {
    std::ostringstream ss;
    std::size_t count = value.size();

    ss << "[";

    for (unsigned int i = 0; i < count; i++) {
        if (i > 0) {
            ss << ", ";
        }

        ss << value[i];
    }

    ss << "]";

    return std::string(ss.str());
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::UShortArrayProperty::toValue() {
    std::size_t count = value.size();

    //typed array
    v8::Local<v8::Uint16Array> arr = v8::Uint16Array::New(v8::ArrayBuffer::New(v8::Isolate::GetCurrent(), count * sizeof(ushort)), 0, count);

    //normal array
    //v8::Local<v8::Array> arr = Nan::New<v8::Array>();

    for (unsigned int i = 0; i < count; i++) {
        Nan::Set(arr, Nan::New<v8::Uint32>(i), Nan::New<v8::Number>(value[i]));
    }

    return arr;
}

/**
 * Get async data representation.
 */
void* AminoJSObject::UShortArrayProperty::getAsyncData(v8::Local<v8::Value> &value, bool &valid) {
    if (value->IsNull()) {
        //Note: only accepting empty arrays as values
        valid = false;

        return NULL;
    }

    std::vector<ushort> *vector =  NULL;

    if (value->IsUint16Array()) {
        //Uint16Array
        v8::Handle<v8::Uint16Array> arr = v8::Handle<v8::Uint16Array>::Cast(value);
        v8::ArrayBuffer::Contents contents = arr->Buffer()->GetContents();
        ushort *data = (ushort *)contents.Data();
        std::size_t count = contents.ByteLength() / sizeof(ushort);

        //debug
        //printf("is Float32Array (size: %i)\n", (int)count);

        //copy to vector
        vector = new std::vector<ushort>();

        vector->assign(data, data + count);

        valid = true;
    } else if (value->IsArray()) {
        v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast(value);
        std::size_t count = arr->Length();

        vector = new std::vector<ushort>();

        for (std::size_t i = 0; i < count; i++) {
            vector->push_back((ushort)(Nan::To<v8::Uint32>(arr->Get(i)).ToLocalChecked()->Value()));
        }

        valid = true;
    } else {
        valid = false;
    }

    return vector;
}

/**
 * Apply async data.
 */
void AminoJSObject::UShortArrayProperty::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    if (!data) {
        value.clear();
        return;
    }

    value = *((std::vector<ushort> *)data);
}

/**
 * Free async data.
 */
void AminoJSObject::UShortArrayProperty::freeAsyncData(void *data) {
    if (data) {
        delete (std::vector<ushort> *)data;
    }
}

//
// AminoJSObject::Int32Property
//

/**
 * Int32Property constructor.
 */
AminoJSObject::Int32Property::Int32Property(AminoJSObject *obj, std::string name, uint32_t id): AnyProperty(PROPERTY_INT32, obj, name, id) {
    //empty
}

/**
 * Int32Property destructor.
 */
AminoJSObject::Int32Property::~Int32Property() {
    //empty
}

/**
 * Update the unsigned int value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::Int32Property::setValue(int newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::Int32Property::toString() {
    return std::to_string(value);
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::Int32Property::toValue() {
    return Nan::New<v8::Int32>(value);
}

/**
 * Get async data representation.
 */
void* AminoJSObject::Int32Property::getAsyncData(v8::Local<v8::Value> &value, bool &valid) {
    if (value->IsNumber()) {
        //UInt32
        int32_t i = Nan::To<v8::Int32>(value).ToLocalChecked()->Value();
        int32_t *res = new int32_t;

        *res = i;
        valid = true;

        return res;
    } else {
        if (DEBUG_BASE) {
            printf("-> default value not a number!\n");
        }

        valid = false;

        return NULL;
    }
}

/**
 * Apply async data.
 */
void AminoJSObject::Int32Property::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    if (data) {
        value = *((int32_t *)data);
    }
}

/**
 * Free async data.
 */
void AminoJSObject::Int32Property::freeAsyncData(void *data) {
    if (data) {
        delete (int32_t *)data;
    }
}

//
// AminoJSObject::UInt32Property
//

/**
 * UInt32Property constructor.
 */
AminoJSObject::UInt32Property::UInt32Property(AminoJSObject *obj, std::string name, uint32_t id): AnyProperty(PROPERTY_UINT32, obj, name, id) {
    //empty
}

/**
 * UInt32Property destructor.
 */
AminoJSObject::UInt32Property::~UInt32Property() {
    //empty
}

/**
 * Update the unsigned int value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::UInt32Property::setValue(unsigned int newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::UInt32Property::toString() {
    return std::to_string(value);
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::UInt32Property::toValue() {
    return Nan::New<v8::Uint32>(value);
}

/**
 * Get async data representation.
 */
void* AminoJSObject::UInt32Property::getAsyncData(v8::Local<v8::Value> &value, bool &valid) {
    if (value->IsNumber()) {
        //UInt32
        uint32_t ui = Nan::To<v8::Uint32>(value).ToLocalChecked()->Value();
        uint32_t *res = new uint32_t;

        *res = ui;
        valid = true;

        return res;
    } else {
        if (DEBUG_BASE) {
            printf("-> default value not a number!\n");
        }

        valid = false;

        return NULL;
    }
}

/**
 * Apply async data.
 */
void AminoJSObject::UInt32Property::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    if (data) {
        value = *((uint32_t *)data);
    }
}

/**
 * Free async data.
 */
void AminoJSObject::UInt32Property::freeAsyncData(void *data) {
    if (data) {
        delete (uint32_t *)data;
    }
}

//
// AminoJSObject::BooleanProperty
//

/**
 * BooleanProperty constructor.
 */
AminoJSObject::BooleanProperty::BooleanProperty(AminoJSObject *obj, std::string name, uint32_t id): AnyProperty(PROPERTY_BOOLEAN, obj, name, id) {
    //empty
}

/**
 * BooleanProperty destructor.
 */
AminoJSObject::BooleanProperty::~BooleanProperty() {
    //empty
}

/**
 * Update the float value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::BooleanProperty::setValue(bool newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::BooleanProperty::toString() {
    return value ? "true":"false";
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::BooleanProperty::toValue() {
    return Nan::New<v8::Boolean>(value);
}

/**
 * Get async data representation.
 */
void* AminoJSObject::BooleanProperty::getAsyncData(v8::Local<v8::Value> &value, bool &valid) {
    if (value->IsBoolean()) {
        bool b = value->BooleanValue();
        bool *res = new bool;

        *res = b;
        valid = true;

        return res;
    } else {
        if (DEBUG_BASE) {
            printf("-> default value not a boolean!\n");
        }

        valid = false;

        return NULL;
    }
}

/**
 * Apply async data.
 */
void AminoJSObject::BooleanProperty::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    if (data) {
        value = *((bool *)data);
    }
}

/**
 * Free async data.
 */
void AminoJSObject::BooleanProperty::freeAsyncData(void *data) {
    if (data) {
        delete (bool *)data;
    }
}

//
// AminoJSObject::Utf8Property
//

/**
 * Utf8Property constructor.
 */
AminoJSObject::Utf8Property::Utf8Property(AminoJSObject *obj, std::string name, uint32_t id): AnyProperty(PROPERTY_UTF8, obj, name, id) {
    //empty
}

/**
 * Utf8Property destructor.
 */
AminoJSObject::Utf8Property::~Utf8Property() {
    //empty
}

/**
 * Update the string value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::Utf8Property::setValue(std::string newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Update the string value.
 *
 * Note: only updates the JS value if modified!
 */
void AminoJSObject::Utf8Property::setValue(char *newValue) {
    setValue(std::string(newValue));
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::Utf8Property::toString() {
    return value;
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::Utf8Property::toValue() {
    return Nan::New<v8::String>(value).ToLocalChecked();
}

/**
 * Get async data representation.
 */
void* AminoJSObject::Utf8Property::getAsyncData(v8::Local<v8::Value> &value, bool &valid) {
    //convert to string
    std::string *str = AminoJSObject::toNewString(value);

    valid = true;

    return str;
}

/**
 * Apply async data.
 */
void AminoJSObject::Utf8Property::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    if (data) {
        value = *((std::string *)data);
    }
}

/**
 * Free async data.
 */
void AminoJSObject::Utf8Property::freeAsyncData(void *data) {
    if (data) {
        delete (std::string *)data;
    }
}

//
// AminoJSObject::ObjectProperty
//

/**
 * ObjectProperty constructor.
 *
 * Note: retains the reference automatically.
 */
AminoJSObject::ObjectProperty::ObjectProperty(AminoJSObject *obj, std::string name, uint32_t id): AnyProperty(PROPERTY_OBJECT, obj, name, id) {
    //empty
}

/**
 * Utf8Property destructor.
 */
AminoJSObject::ObjectProperty::~ObjectProperty() {
    //release instance (Note: non-virtual function)
    destroy();
}

/**
 * Free retained object value.
 *
 * Note: has to be called on main thread.
 */
void AminoJSObject::ObjectProperty::destroy() {
    if (value) {
        value->release();
        value = NULL;
    }
}

/**
 * Update the object value.
 *
 * Note: only updates the JS value if modified!
 *
 * !!! Attention: object must be retained and previous value must be released before calling this function!!!
 */
void AminoJSObject::ObjectProperty::setValue(AminoJSObject *newValue) {
    if (value != newValue) {
        value = newValue;

        if (connected) {
            obj->updateProperty(this);
        }
    }
}

/**
 * Convert to string value.
 */
std::string AminoJSObject::ObjectProperty::toString() {
    return value ? value->name.c_str():"null";
}

/**
 * Get JS value.
 */
v8::Local<v8::Value> AminoJSObject::ObjectProperty::toValue() {
    if (value) {
        return value->handle();
    } else {
        return Nan::Null();
    }
}

/**
 * Get async data representation.
 */
void* AminoJSObject::ObjectProperty::getAsyncData(v8::Local<v8::Value> &value, bool &valid) {
    valid = true;

    if (value->IsObject()) {
        v8::Local<v8::Object> jsObj = Nan::To<v8::Object>(value).ToLocalChecked();
        AminoJSObject *obj = Nan::ObjectWrap::Unwrap<AminoJSObject>(jsObj);

        assert(obj);

        //retain reference on main thread
        obj->retain();

        return obj;
    } else {
        return NULL;
    }
}

/**
 * Apply async data.
 */
void AminoJSObject::ObjectProperty::setAsyncData(AsyncPropertyUpdate *update, void *data) {
    AminoJSObject *obj = static_cast<AminoJSObject *>(data);

    if (obj != value) {
        //release old instance
        if (value) {
            if (update) {
                update->releaseLater = value;
            } else {
                //on main thread
                value->release();
            }
        }

        //keep new instance
        if (obj) {
            if (update) {
                update->retainLater = obj;
            } else {
                //on main thread
                obj->retain();
            }
        }

        value = obj;
    }
}

/**
 * Free async data.
 */
void AminoJSObject::ObjectProperty::freeAsyncData(void *data) {
    AminoJSObject *obj = static_cast<AminoJSObject *>(data);

    //release reference on main thread
    if (obj) {
        obj->release();
    }
}

//
// AminoJSObject::AnyAsyncUpdate
//

AminoJSObject::AnyAsyncUpdate::AnyAsyncUpdate(int32_t type): type(type) {
    //empty
}

AminoJSObject::AnyAsyncUpdate::~AnyAsyncUpdate() {
    //empty
}

//
// AminoJSObject::AsyncValueUpdate
//

AminoJSObject::AsyncValueUpdate::AsyncValueUpdate(AminoJSObject *obj, AminoJSObject *value, asyncValueCallback callback): AnyAsyncUpdate(ASYNC_UPDATE_VALUE), obj(obj), valueObj(value), callback(callback) {
    assert(obj);

    //retain objects on main thread
    obj->retain();

    if (value) {
        value->retain();
    }

    //init (on main thread)
    if (callback) {
        (obj->*callback)(this, STATE_CREATE);
    }
}

AminoJSObject::AsyncValueUpdate::AsyncValueUpdate(AminoJSObject *obj, unsigned int value, void *data, asyncValueCallback callback): AnyAsyncUpdate(ASYNC_UPDATE_VALUE), obj(obj), data(data), valueUint32(value), callback(callback) {
    assert(obj);

    //retain objects on main thread
    obj->retain();

    //init (on main thread)
    if (callback) {
        (obj->*callback)(this, STATE_CREATE);
    }
}

AminoJSObject::AsyncValueUpdate::AsyncValueUpdate(AminoJSObject *obj, v8::Local<v8::Value> &value, void *data, asyncValueCallback callback): AnyAsyncUpdate(ASYNC_UPDATE_VALUE), obj(obj), data(data), callback(callback) {
    assert(obj);

    //retain objects on main thread
    obj->retain();

    //value
    valuePersistent = new Nan::Persistent<v8::Value>();
    valuePersistent->Reset(value);

    //init (on main thread)
    if (callback) {
        (obj->*callback)(this, STATE_CREATE);
    }
}

AminoJSObject::AsyncValueUpdate::~AsyncValueUpdate() {
    //additional retains
    if (releaseLater) {
        releaseLater->release();
    }

    //call cleanup handler
    if (callback) {
        (obj->*callback)(this, STATE_DELETE);
    }

    //release objects on main thread
    obj->release();

    if (valueObj) {
        valueObj->release();
    }

    //peristent
    if (valuePersistent) {
        valuePersistent->Reset();
        delete valuePersistent;
    }
}

/**
 * Apply async value.
 */
void AminoJSObject::AsyncValueUpdate::apply() {
    //on async thread
    if (callback) {
        (obj->*callback)(this, AsyncValueUpdate::STATE_APPLY);
    }
}

//
// AminoJSObject::JSPropertyUpdate
//

AminoJSObject::JSPropertyUpdate::JSPropertyUpdate(AnyProperty *property): AnyAsyncUpdate(ASYNC_JS_UPDATE_PROPERTY), property(property) {
    //empty
}

AminoJSObject::JSPropertyUpdate::~JSPropertyUpdate() {
    //empty
}

/**
 * Update JS property on main thread.
 */
void AminoJSObject::JSPropertyUpdate::apply() {
    v8::Local<v8::Value> value = property->toValue();

    property->obj->updateProperty(property->name, value);
}

//
// AminoJSObject::JSCallbackUpdate
//

AminoJSObject::JSCallbackUpdate::JSCallbackUpdate(AminoJSObject *obj, jsUpdateCallback callbackApply, jsUpdateCallback callbackDone, void *data): AnyAsyncUpdate(ASYNC_JS_UPDATE_CALLBACK), obj(obj), callbackApply(callbackApply), callbackDone(callbackDone), data(data) {
    //empty
}

AminoJSObject::JSCallbackUpdate::~JSCallbackUpdate() {
    if (callbackDone) {
        (obj->*callbackDone)(this);
    }
}

void AminoJSObject::JSCallbackUpdate::apply() {
    if (callbackApply) {
        (obj->*callbackApply)(this);
    }
}

//
// AminoJSEventObject
//

AminoJSEventObject::AminoJSEventObject(std::string name): AminoJSObject(name) {
    asyncUpdates = new std::vector<AnyAsyncUpdate *>();
    asyncDeletes = new std::vector<AnyAsyncUpdate *>();
    jsUpdates = new std::vector<AnyAsyncUpdate *>();

    //main thread
    mainThread = std::this_thread::get_id();

    // //recursive mutex needed
    // pthread_mutexattr_t attr;

    // int res = pthread_mutexattr_init(&attr);

    // assert(res == 0);

    // res = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    // assert(res == 0);

    // // asyncLock
    // res = pthread_mutex_init(&asyncLock, &attr);
    // assert(res == 0);
}

AminoJSEventObject::~AminoJSEventObject() {
    //Note: all called methods are not virtual

    //JS updates
    handleJSUpdates();
    delete jsUpdates;

    //asyncUpdates
    clearAsyncQueue();
    delete asyncUpdates;

    //asyncDeletes
    handleAsyncDeletes();
    delete asyncDeletes;

    //mutex
    // int res = pthread_mutex_destroy(&asyncLock);

    // assert(res == 0);
}

/**
 * Get the event handler (this object).
 */
AminoJSEventObject* AminoJSEventObject::getEventHandler() {
    return this;
}

/**
 * Clear async updates.
 *
 * Note: items are never applied.
 */
void AminoJSEventObject::clearAsyncQueue() {
    assert(asyncUpdates);

    asyncLock.lock();

    // assert(res == 0);

    std::size_t count = asyncUpdates->size();

    for (std::size_t i = 0; i < count; i++) {
        AnyAsyncUpdate *item = (*asyncUpdates)[i];

        delete item;
    }

    //Note: not applied, can safely clear vector
    asyncUpdates->clear();

    asyncLock.unlock();
    // assert(res == 0);
}

/**
 * Free async updates on main thread.
 *
 * Note: has to run on main thread!
 */
void AminoJSEventObject::handleAsyncDeletes() {
    assert(asyncDeletes);

    if (DEBUG_BASE) {
        assert(isMainThread());
    }

    asyncLock.lock();

    // assert(res == 0);

    std::size_t count = asyncDeletes->size();

    if (count > 0) {
        //create scope
        Nan::HandleScope scope;

        for (std::size_t i = 0; i < count; i++) {
            AnyAsyncUpdate *item = (*asyncDeletes)[i];

            //free instance
            delete item;
        }

        assert(count == asyncDeletes->size());

        asyncDeletes->clear();
    }

    asyncLock.unlock();
    // assert(res == 0);
}

/**
 * Process all JS updates on main thread.
 *
 * Note: has to run on main thread!
 */
void AminoJSEventObject::handleJSUpdates() {
    assert(jsUpdates);

    if (DEBUG_BASE) {
        assert(isMainThread());
    }

    asyncLock.lock();

    // assert(res == 0);

    std::size_t count = jsUpdates->size();

    if (count > 0) {
        //create scope
        Nan::HandleScope scope;

        for (std::size_t i = 0; i < jsUpdates->size(); i++) {
            AnyAsyncUpdate *item = (*jsUpdates)[i];

            item->apply();
            delete item;
        }

        jsUpdates->clear();
    }

    asyncLock.unlock();
    // assert(res == 0);
}

/**
 * Get runtime specific data.
 */
void AminoJSEventObject::getStats(v8::Local<v8::Object> &obj) {
    //internal

    /*
    Nan::Set(obj, Nan::New("jsUpdates").ToLocalChecked(), Nan::New<v8::Uint32>((uint32_t)jsUpdates->size()));
    Nan::Set(obj, Nan::New("asyncUpdates").ToLocalChecked(), Nan::New<v8::Uint32>((uint32_t)asyncUpdates->size()));
    Nan::Set(obj, Nan::New("asyncDeletes").ToLocalChecked(), Nan::New<v8::Uint32>((uint32_t)asyncDeletes->size()));
    */

    //output instance stats
    if (DEBUG_JS_INSTANCES) {
        //collect items
        std::map<std::string, int> counter;
        std::size_t count = jsInstances.size();

        for (std::size_t i = 0; i < count; i++) {
            AminoJSObject *item = jsInstances[i];
            std::string name = item->getName();

            //check existing
            std::map<std::string, int>::iterator it = counter.find(name);

            if (it != counter.end()) {
                it->second++;
            } else {
                counter.insert(std::pair<std::string, int>(name, 1));
            }
        }

        //output
        printf("JS Instances:\n");

        for (std::map<std::string, int>::iterator it = counter.begin(); it != counter.end(); it++) {
            printf(" %s: %i\n", it->first.c_str(), it->second);
        }
    }
}

/**
 * Check if running on main thread.
 */
bool AminoJSEventObject::isMainThread() {
    return std::this_thread::get_id() == mainThread;
}

/**
 * This is an event handler.
 */
bool AminoJSEventObject::isEventHandler() {
    return true;
}

/**
 * Process all queued updates.
 *
 * Note: runs on rendering thread.
 */
void AminoJSEventObject::processAsyncQueue() {
    if (destroyed) {
        return;
    }

    if (DEBUG_BASE) {
        assert(!isMainThread());

        printf("--- processAsyncQueue() --- \n");
    }

    //iterate
    assert(asyncUpdates);

    asyncLock.lock();

    // assert(res == 0);

    for (std::size_t i = 0; i < asyncUpdates->size(); i++) {
        AnyAsyncUpdate *item = (*asyncUpdates)[i];

        assert(item);

        //debug
        //printf("%i of %i (type: %i)\n", (int)i, (int)asyncUpdates->size(), (int)item->type);

        switch (item->type) {
            case ASYNC_UPDATE_PROPERTY:
                //property update
                {
                    AsyncPropertyUpdate *propItem = static_cast<AsyncPropertyUpdate *>(item);

                    //call local handler
                    assert(propItem->property);
                    assert(propItem->property->obj);

                    if (DEBUG_ASYNC) {
                        printf("%i of %i (property: %s of %s)\n", (int)i, (int)asyncUpdates->size(), propItem->property->name.c_str(), propItem->property->obj->getName().c_str());
                    }

                    propItem->property->obj->handleAsyncUpdate(propItem);
                }
                break;

            case ASYNC_UPDATE_VALUE:
                //custom value update
                {
                    AsyncValueUpdate *valueItem = static_cast<AsyncValueUpdate *>(item);

                    //call local handler
                    assert(valueItem->obj);

                    if (DEBUG_ASYNC) {
                        printf("%i of %i (type: value update)\n", (int)i, (int)asyncUpdates->size());
                    }

                    if (!valueItem->obj->handleAsyncUpdate(valueItem)) {
                        printf("unhandled async update by %s\n", valueItem->obj->getName().c_str());
                    }
                }
                break;

            default:
                printf("unknown async type: %i\n", item->type);
                assert(false);
                break;
        }

        //free item
        asyncDeletes->push_back(item);
    }

    //clear
    asyncUpdates->clear();

    asyncLock.unlock();
    // assert(res == 0);

    if (DEBUG_BASE) {
        printf("--- processAsyncQueue() done --- \n");
    }
}

/**
 * Enqueue a value update.
 */
bool AminoJSEventObject::enqueueValueUpdate(AsyncValueUpdate *update) {
    assert(update);

    if (destroyed) {
        //free
        delete update;

        return false;
    }

    //enqueue
    if (DEBUG_BASE) {
        printf("enqueueValueUpdate\n");
    }

    assert(asyncUpdates);

    asyncLock.lock();

    // assert(res == 0);

    asyncUpdates->push_back(update);

    asyncLock.unlock();
    // assert(res == 0);

    return true;
}

/**
 * Enqueue a property update (value change from JS code).
 *
 * Note: called on main thread.
 */
bool AminoJSEventObject::enqueuePropertyUpdate(AnyProperty *prop, v8::Local<v8::Value> &value) {
    if (destroyed) {
        return false;
    }

    assert(prop);

    //create
    bool valid = false;
    void *data = prop->getAsyncData(value, valid);

    if (!valid) {
        return false;
    }

    //call sync handler
    assert(prop->obj);

    if (prop->obj->handleSyncUpdate(prop, data)) {
        if (DEBUG_BASE) {
            printf("-> sync update (value=%s)\n", toString(value).c_str());
        }

        prop->freeAsyncData(data);

        return true;
    }

    //async handling
    asyncLock.lock();

    // assert(res == 0);

    asyncUpdates->push_back(new AsyncPropertyUpdate(prop, data));

    asyncLock.unlock();
    // assert(res == 0);

    return true;
}

/**
 * Add JS property update.
 */
bool AminoJSEventObject::enqueueJSPropertyUpdate(AnyProperty *prop) {
    return enqueueJSUpdate(new JSPropertyUpdate(prop));
}

/**
 * Add JS update to run on main thread.
 */
bool AminoJSEventObject::enqueueJSUpdate(AnyAsyncUpdate *update) {
    assert(update);

    if (destroyed) {
        delete update;

        return false;
    }

    asyncLock.lock();

    // assert(res == 0);

    jsUpdates->push_back(update);

    asyncLock.unlock();
    // assert(res == 0);

    return true;
}

//
// AminoJSEventObject::AsyncPropertyUpdate
//

/**
 * Constructor.
 */
AminoJSEventObject::AsyncPropertyUpdate::AsyncPropertyUpdate(AnyProperty *property, void *data): AnyAsyncUpdate(ASYNC_UPDATE_PROPERTY), property(property), data(data) {
    assert(property);

    //retain instance to target object
    property->retain();
}

/**
 * Destructor.
 *
 * Note: called on main thread.
 */
AminoJSEventObject::AsyncPropertyUpdate::~AsyncPropertyUpdate() {
    //retain/release
    if (retainLater) {
        retainLater->retain();
    }

    if (releaseLater) {
        releaseLater->release();
    }

    //free data
    property->freeAsyncData(data);
    data = NULL;

    //release instance to target object
    property->release();
}

/**
 * Set new value.
 */
void AminoJSEventObject::AsyncPropertyUpdate::apply() {
    property->setAsyncData(this, data);
}
