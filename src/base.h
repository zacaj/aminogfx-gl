#ifndef _AMINOBASE_H
#define _AMINOBASE_H

#include "gfx.h"
#include "base_js.h"
#include "base_weak.h"
#include "fonts.h"
#include "images.h"

#include <uv.h>
#include "shaders.h"
#include "mathutils.h"
#include <stdio.h>
#include <vector>
#include <stack>
#include <stdlib.h>
#include <string>
#include <map>

#include "freetype-gl.h"
#include "mat4.h"
#include "shader.h"
#include "vertex-buffer.h"
#include "texture-font.h"

#define DEBUG_CRASH false
#define GROUP AMINO_GROUP
#define RECT AMINO_RECT
#undef TEXT
#define TEXT AMINO_TEXT
#define POLY AMINO_POLY
#define MODEL AMINO_MODEL
#include <mutex>
#include <thread>
const int GROUP = 1;
const int RECT  = 2;
const int TEXT  = 3;
const int ANIM  = 4;
const int POLY  = 5;
const int MODEL = 6;

class AminoText;
class AminoGroup;
class AminoAnim;
class AminoRenderer;

/**
 * Amino main class to call from JavaScript.
 *
 * Note: abstract
 */
class AminoGfx : public AminoJSEventObject {
public:
    AminoGfx(std::string name);
    ~AminoGfx();

    static NAN_MODULE_INIT(InitClasses);

    bool addAnimation(AminoAnim *anim);
    void removeAnimation(AminoAnim *anim);

    bool deleteTextureAsync(GLuint textureId);
    bool deleteBufferAsync(GLuint bufferId);
    bool deleteVertexBufferAsync(vertex_buffer_t *buffer);

    //text
    void textUpdateNeeded(AminoText *text);
    amino_atlas_t getAtlasTexture(texture_atlas_t *atlas, bool createIfMissing, bool &newTexture);
    void notifyTextureCreated(int count);
    static void updateAtlasTextures(texture_atlas_t *atlas);

    //video
    virtual AminoVideoPlayer *createVideoPlayer(AminoTexture *texture, AminoVideo *video) = 0;

protected:
    static int instanceCount;
    static std::vector<AminoGfx *> instances;

    bool started = false;
    bool rendering = false;
    Nan::Callback *startCallback = NULL;

    //params
    Nan::Persistent<v8::Object> createParams;

    //renderer
    AminoRenderer *renderer = NULL;
    AminoGroup *root = NULL;
    int viewportW;
    int viewportH;
    bool viewportChanged;
    int32_t swapInterval = 0;
    GLint maxTextureSize = 0;
    int rendererErrors = 0;
    int textureCount = 0;

    //instance
    void addInstance();
    void removeInstance();

    //text
    std::vector<AminoText *> textUpdates;

    void updateTextNodes();
    virtual void atlasTextureHasChanged(texture_atlas_t *atlas);
    void updateAtlasTexture(texture_atlas_t *atlas);
    void updateAtlasTextureHandler(AsyncValueUpdate *update, int state);

    //performance (FPS)
    double fpsStart = 0;
    double fpsCycleStart;
    double fpsCycleEnd;
    double fpsCycleMin;
    double fpsCycleMax;
    double fpsCycleAvg;
    int fpsCount;

    double lastFPS = 0;
    double lastCycleStart = 0;
    double lastCycleMax = 0;
    double lastCycleMin = 0;
    double lastCycleAvg = 0;

    //thread
    uv_thread_t thread;
    bool threadRunning = false;
    uv_async_t asyncHandle;

    //properties
    FloatProperty *propX;
    FloatProperty *propY;

    FloatProperty *propW;
    FloatProperty *propH;

    FloatProperty *propR;
    FloatProperty *propG;
    FloatProperty *propB;

    FloatProperty *propOpacity;

    Utf8Property *propTitle;

    BooleanProperty *propShowFPS;

    //animations
    std::vector<AminoAnim *> animations;
    std::recursive_mutex animLock; //Note: short cycles

    //creation
    static void Init(Nan::ADDON_REGISTER_FUNCTION_ARGS_TYPE target, AminoJSObjectFactory* factory);

    void setup() override;

    //abstract methods
    virtual void initRenderer();
    void setupRenderer();
    void addRuntimeProperty();
    virtual void populateRuntimeProperties(v8::Local<v8::Object> &obj);

    virtual void start();
    void ready();

    void startRenderingThread();
    void stopRenderingThread();
    bool isRenderingThreadRunning();
    static void renderingThread(void *arg);
    static void handleRenderEvents(uv_async_t *handle);
    virtual void handleSystemEvents() = 0;

    virtual void initRendering();
    virtual void render();
    virtual void endRendering();
    void processAnimations();
    virtual bool bindContext() = 0;
    virtual void renderScene();
    virtual void renderingDone() = 0;
    bool isRendering();

    void destroy() override;
    void destroyAminoGfx();

    virtual bool getScreenInfo(int &w, int &h, int &refreshRate, bool &fullscreen) { return false; };
    void updateSize(int w, int h); //call after size event
    void updatePosition(int x, int y); //call after position event

    void fireEvent(v8::Local<v8::Object> &obj);

    bool handleSyncUpdate(AnyProperty *prop, void *data) override;
    virtual void updateWindowSize() = 0;
    virtual void updateWindowPosition() = 0;
    virtual void updateWindowTitle() = 0;

    void setRoot(AminoGroup *group);

    void getStats(v8::Local<v8::Object> &obj) override;

private:
    void preInit(Nan::NAN_METHOD_ARGS_TYPE info) override;

    //JS methods
    static NAN_METHOD(Start);
    static NAN_METHOD(Destroy);

    static NAN_METHOD(SetRoot);
    static NAN_METHOD(ClearAnimations);
    static NAN_METHOD(UpdatePerspective);
    static NAN_METHOD(GetStats);
    static NAN_METHOD(GetTime);

    //animation
    void clearAnimations();

    //texture & buffer
    void deleteTexture(AsyncValueUpdate *update, int state);
    void deleteBuffer(AsyncValueUpdate *update, int state);
    void deleteVertexBuffer(AsyncValueUpdate *update, int state);

    //stats
    void measureRenderingStart();
    void measureRenderingEnd();
};

/**
 * Base class for all rendering nodes.
 *
 * Note: abstract.
 */
class AminoNode : public AminoJSObject {
public:
    int type;

    //location
    FloatProperty *propX;
    FloatProperty *propY;
    FloatProperty *propZ;

    //size (optional)
    FloatProperty *propW = NULL;
    FloatProperty *propH = NULL;

    //origin (optional)
    FloatProperty *propOriginX = NULL;
    FloatProperty *propOriginY = NULL;

    //zoom factor
    FloatProperty *propScaleX;
    FloatProperty *propScaleY;

    //rotation
    FloatProperty *propRotateX;
    FloatProperty *propRotateY;
    FloatProperty *propRotateZ;

    //opacity
    FloatProperty *propOpacity;

    //visibility
    BooleanProperty *propVisible;

    AminoNode(std::string name, int type): AminoJSObject(name), type(type) {
        //empty
    }

    ~AminoNode() {
        //see destroy
    }

    void preInit(Nan::NAN_METHOD_ARGS_TYPE info) override {
        assert(info.Length() >= 1);

        //set amino instance
        v8::Local<v8::Object> jsObj = Nan::To<v8::Object>(info[0]).ToLocalChecked();
        AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(jsObj);

        assert(obj);

        //bind to queue
        this->setEventHandler(obj);
        Nan::Set(handle(), Nan::New("amino").ToLocalChecked(), jsObj);
    }

    void setup() override {
        AminoJSObject::setup();

        //register native properties
        propX = createFloatProperty("x");
        propY = createFloatProperty("y");
        propZ = createFloatProperty("z");

        propScaleX = createFloatProperty("sx");
        propScaleY = createFloatProperty("sy");

        propRotateX = createFloatProperty("rx");
        propRotateY = createFloatProperty("ry");
        propRotateZ = createFloatProperty("rz");

        propOpacity = createFloatProperty("opacity");
        propVisible = createBooleanProperty("visible");
    }

    /**
     * Free all resources.
     */
    void destroy() override {
        if (destroyed) {
            return;
        }

        AminoJSObject::destroy();

        //to be overwritten

        //debug
        //printf("Destroyed node: %i\n", type);
    }

    /**
     * Get AminoGfx instance.
     */
    AminoGfx* getAminoGfx() {
        assert(eventHandler);

        AminoGfx *res = static_cast<AminoGfx *>(eventHandler);

        return res;
    }

    /**
     * Validate renderer instance. Must be called in JS method handler.
     */
    bool checkRenderer(AminoNode *node) {
        return checkRenderer(node->getAminoGfx());
    }

    /**
     * Validate renderer instance. Must be called in JS method handler.
     */
    bool checkRenderer(AminoGfx *amino) {
        assert(eventHandler);

        if (eventHandler != amino) {
            Nan::ThrowTypeError("invalid renderer");
            return false;
        }

        return true;
    }
};

/**
 * Text factory.
 */
class AminoTextFactory : public AminoJSObjectFactory {
public:
    AminoTextFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Text node class.
 */
class AminoText : public AminoNode {
public:
    //text
    Utf8Property *propText;

    //color
    FloatProperty *propR;
    FloatProperty *propG;
    FloatProperty *propB;

    //box
    FloatProperty *propW;
    FloatProperty *propH;

    Utf8Property *propWrap;
    int wrap = WRAP_NONE;

    //font
    ObjectProperty *propFont;
    AminoFontSize *fontSize = NULL;
    vertex_buffer_t *buffer = NULL;

    //alignment
    Utf8Property *propAlign;
    Utf8Property *propVAlign;
    int align = ALIGN_LEFT;
    int vAlign = VALIGN_BASELINE;

    //lines
    Int32Property *propMaxLines;
    int lineNr = 1;
    float lineW = 0;

    //mutex
    static uv_mutex_t freeTypeMutex;
    static bool freeTypeMutexInitialized;

    //constants
    static const int ALIGN_LEFT   = 0x0;
    static const int ALIGN_CENTER = 0x1;
    static const int ALIGN_RIGHT  = 0x2;

    static const int VALIGN_BASELINE = 0x0;
    static const int VALIGN_TOP      = 0x1;
    static const int VALIGN_MIDDLE   = 0x2;
    static const int VALIGN_BOTTOM   = 0x3;

    static const int WRAP_NONE = 0x0;
    static const int WRAP_END  = 0x1;
    static const int WRAP_WORD = 0x2;

    AminoText(): AminoNode(getFactory()->name, TEXT) {
        //mutex
        initFreeTypeMutex();
    }

    ~AminoText() {
        if (!destroyed) {
            destroyAminoText();
        }
    }

    /**
     * Initialize the mutex.
     */
    static void initFreeTypeMutex() {
        if (!freeTypeMutexInitialized) {
            freeTypeMutexInitialized = true;

            int res = uv_mutex_init(&freeTypeMutex);

            assert(res == 0);
        }
    }

    /**
     * Free all resources.
     */
    void destroy() override {
        if (destroyed) {
            return;
        }

        //instance
        destroyAminoText();

        //base
        AminoNode::destroy();
    }

    /**
     * Free buffers.
     */
    void destroyAminoText() {
        if (buffer) {
            if (eventHandler) {
                if (getAminoGfx()->deleteVertexBufferAsync(buffer)) {
                    buffer = NULL;
                }
            }

            if (buffer) {
                //prevent OpenGL calls(on wrong thread)
                buffer->vertices_id = 0;
                buffer->indices_id = 0;

                vertex_buffer_delete(buffer);

                buffer = NULL;
            }
        }

        //release object values
        propFont->destroy();

        fontSize = NULL;
        texture.textureId = INVALID_TEXTURE;
    }

    /**
     * Setup instance.
     */
    void setup() override {
        AminoNode::setup();

        //register native properties
        propText = createUtf8Property("text");

        propR = createFloatProperty("r");
        propG = createFloatProperty("g");
        propB = createFloatProperty("b");

        propW = createFloatProperty("w");
        propH = createFloatProperty("h");

        propOriginX = createFloatProperty("originX");
        propOriginY = createFloatProperty("originY");

        propWrap = createUtf8Property("wrap");
        propAlign = createUtf8Property("align");
        propVAlign = createUtf8Property("vAlign");
        propFont = createObjectProperty("font");

        propMaxLines = createInt32Property("maxLines");
    }

    //creation

    /**
     * Create text factory.
     */
    static AminoTextFactory* getFactory() {
        static AminoTextFactory *textFactory = NULL;

        if (!textFactory) {
            textFactory = new AminoTextFactory(New);
        }

        return textFactory;
    }

    /**
     * Initialize Group template.
     */
    static v8::Local<v8::FunctionTemplate> GetInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

        //prototype methods
        // -> none

        //template function
        return tpl;
    }

    /**
     * Handle async property updates.
     */
    void handleAsyncUpdate(AsyncPropertyUpdate *update) override {
        //debug
        //printf("AminoText::handleAsyncUpdate()\n");

        //default: set value
        AminoJSObject::handleAsyncUpdate(update);

        //check font updates
        AnyProperty *property = update->property;
        bool updated = false;

        assert(property);

        if (property == propW || property == propH || property == propText || property == propMaxLines) {
            updated = true;
        } else if (property == propWrap) {
            //wrap
            std::string str = propWrap->value;
            int oldWrap = wrap;

            if (str == "none") {
                wrap = WRAP_NONE;
            } else if (str == "word") {
                wrap = WRAP_WORD;
            } else if (str == "end") {
                wrap = WRAP_END;
            } else {
                //error
                printf("unknown wrap mode: %s\n", str.c_str());
            }

            if (wrap != oldWrap) {
                updated = true;
            }
        } else if (property == propAlign) {
            //align
            std::string str = propAlign->value;
            int oldAlign = align;

            if (str == "left") {
                align = AminoText::ALIGN_LEFT;
            } else if (str == "center") {
                align = AminoText::ALIGN_CENTER;
            } else if (str == "right") {
                align = AminoText::ALIGN_RIGHT;
            } else {
                //error
                printf("unknown align mode: %s\n", str.c_str());
            }

            if (align != oldAlign) {
                updated = true;
            }
        } else if (property == propVAlign) {
            //vAlign
            std::string str = propVAlign->value;
            int oldVAlign = vAlign;

            if (str == "top") {
                vAlign = AminoText::VALIGN_TOP;
            } else if (str == "middle") {
                vAlign = AminoText::VALIGN_MIDDLE;
            } else if (str == "baseline") {
                vAlign = AminoText::VALIGN_BASELINE;
            } else if (str == "bottom") {
                vAlign = AminoText::VALIGN_BOTTOM;
            } else {
                //error
                printf("unknown vAlign mode: %s\n", str.c_str());
            }

            if (vAlign != oldVAlign) {
                updated = true;
            }
        } else if (property == propFont) {
            //font
            AminoFontSize *fs = static_cast<AminoFontSize *>(propFont->value);

            if (fontSize == fs) {
                //same font
                return;
            }

            //new font
            fontSize = fs;
            texture.textureId = INVALID_TEXTURE; //reset texture

            //debug
            //printf("-> use font: %s\n", fs->font->fontName.c_str());

            updated = true;
        }

        if (updated) {
            getAminoGfx()->textUpdateNeeded(this);
        }
    }

    /**
     * Update the rendered text.
     */
    bool layoutText();

    /**
     * Create or update a font texture.
     */
    void updateTexture();
    static void updateTextureFromAtlas(GLuint textureId, texture_atlas_t *atlas);

    /**
     * Get font texture.
     */
    GLuint getTextureId();

private:
    amino_atlas_t texture = { INVALID_TEXTURE };

    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }

    static void addTextGlyphs(vertex_buffer_t *buffer, texture_font_t *font, const char *text, vec2 *pen, int wrap, int width, int *lineNr, int maxLines, float *lineW);
};

/**
 * Animation factory.
 */
class AminoAnimFactory : public AminoJSObjectFactory {
public:
    AminoAnimFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Animation class.
 */
class AminoAnim : public AminoJSObject {
private:
    AnyProperty *prop;

    bool started = false;
    bool ended = false;

    //properties
    double start;
    double end;
    int32_t count;
    double duration;
    bool autoreverse;
    int32_t direction = FORWARD;
    int32_t timeFunc = TF_CUBIC_IN_OUT;
    Nan::Callback *then = NULL;

    //start pos
    double zeroPos;
    bool hasZeroPos = false;

    //sync time
    double refTime;
    bool hasRefTime = false;

    double startTime = 0;
    double lastTime  = 0;
    double pauseTime = 0;

    static const int32_t FORWARD  = 1;
    static const int32_t BACKWARD = 2;

    static const int32_t FOREVER = -1;

public:
    static const int32_t TF_LINEAR       = 0x0;
    static const int32_t TF_CUBIC_IN     = 0x1;
    static const int32_t TF_CUBIC_OUT    = 0x2;
    static const int32_t TF_CUBIC_IN_OUT = 0x3;

    AminoAnim(): AminoJSObject(getFactory()->name) {
        //empty
    }

    ~AminoAnim() {
        if (!destroyed) {
            destroyAminoAnim();
        }
    }

    /**
     * Handle JS constructor params.
     */
    void preInit(Nan::NAN_METHOD_ARGS_TYPE info) override {
        assert(info.Length() == 3);

        //params
        AminoGfx *obj = Nan::ObjectWrap::Unwrap<AminoGfx>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
        AminoNode *node = Nan::ObjectWrap::Unwrap<AminoNode>(Nan::To<v8::Object>(info[1]).ToLocalChecked());
        uint32_t propId = Nan::To<v8::Uint32>(info[2]).ToLocalChecked()->Value();

        assert(obj);
        assert(node);

        if (!node->checkRenderer(obj)) {
            return;
        }

        //get property
        AnyProperty *prop = node->getPropertyWithId(propId);

        if (!prop || prop->type != PROPERTY_FLOAT) {
            Nan::ThrowTypeError("property cannot be animated");
            return;
        }

        //bind to queue (retains AminoGfx reference)
        this->setEventHandler(obj);
        this->prop = prop;

        //retain property (Note: stop() has to be called to free the instance)
        prop->retain();

        //enqueue
        obj->addAnimation(this);
    }

    /**
     * Free all resources.
     */
    void destroy() override {
        if (destroyed) {
            return;
        }

        //instance
        destroyAminoAnim();

        //base class
        AminoJSObject::destroy();
    }

    /**
     * Free instance data.
     */
    void destroyAminoAnim() {
        if (prop) {
            prop->release();
            prop = NULL;
        }

        if (then) {
            delete then;
            then = NULL;
        }
    }

    //creation

    /**
     * Create anim factory.
     */
    static AminoAnimFactory* getFactory() {
        static AminoAnimFactory *animFactory = NULL;

        if (!animFactory) {
            animFactory = new AminoAnimFactory(New);
        }

        return animFactory;
    }

    /**
     * Initialize Group template.
     */
    static v8::Local<v8::FunctionTemplate> GetInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

        //methods
        Nan::SetPrototypeMethod(tpl, "_start", Start);
        Nan::SetPrototypeMethod(tpl, "stop", Stop);

        //template function
        return tpl;
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }

    /**
     * Start animation.
     */
    static NAN_METHOD(Start) {
        assert(info.Length() == 1);

        AminoAnim *obj = Nan::ObjectWrap::Unwrap<AminoAnim>(info.This());
        v8::Local<v8::Object> data = Nan::To<v8::Object>(info[0]).ToLocalChecked();

        assert(obj);

        obj->handleStart(data);
    }

    /**
     * Start animation.
     */
    void handleStart(v8::Local<v8::Object> &data) {
        if (started) {
            Nan::ThrowTypeError("already started");
            return;
        }

        //parameters
        start       = Nan::To<v8::Number>(Nan::Get(data, Nan::New<v8::String>("from").ToLocalChecked()).ToLocalChecked()).ToLocalChecked()->Value();
        end         = Nan::To<v8::Number>(Nan::Get(data, Nan::New<v8::String>("to").ToLocalChecked()).ToLocalChecked()).ToLocalChecked()->Value();
        duration    = Nan::To<v8::Number>(Nan::Get(data, Nan::New<v8::String>("duration").ToLocalChecked()).ToLocalChecked()).ToLocalChecked()->Value();
        count       = Nan::To<v8::Integer>(Nan::Get(data, Nan::New<v8::String>("count").ToLocalChecked()).ToLocalChecked()).ToLocalChecked()->Value();
        autoreverse = Nan::To<v8::Boolean>(Nan::Get(data, Nan::New<v8::String>("autoreverse").ToLocalChecked()).ToLocalChecked()).ToLocalChecked()->Value();

        //time func
        Nan::Utf8String str(Nan::Get(data, Nan::New<v8::String>("timeFunc").ToLocalChecked()).ToLocalChecked());
        std::string tf = std::string(*str);

        if (tf == "cubicIn") {
            timeFunc = TF_CUBIC_IN;
        } else if (tf == "cubicOut") {
            timeFunc = TF_CUBIC_OUT;
        } else if (tf == "cubicInOut") {
            timeFunc = TF_CUBIC_IN_OUT;
        } else {
            timeFunc = TF_LINEAR;
        }

        //TODO support CSS key frames

        //then
        v8::MaybeLocal<v8::Value> maybeThen = Nan::Get(data, Nan::New<v8::String>("then").ToLocalChecked());

        if (!maybeThen.IsEmpty()) {
            v8::Local<v8::Value> thenLocal = maybeThen.ToLocalChecked();

            if (thenLocal->IsFunction()) {
                then = new Nan::Callback(thenLocal.As<v8::Function>());
            }
        }

        //optional values

        // 1) pos
        v8::MaybeLocal<v8::Value> maybePos = Nan::Get(data, Nan::New<v8::String>("pos").ToLocalChecked());

        if (!maybePos.IsEmpty()) {
            v8::Local<v8::Value> posLocal = maybePos.ToLocalChecked();

            if (posLocal->IsNumber()) {
                hasZeroPos = true;
                zeroPos = Nan::To<v8::Number>(posLocal).ToLocalChecked()->Value();
            }
        }

        // 2) refTime
        v8::MaybeLocal<v8::Value> maybeRefTime = Nan::Get(data, Nan::New<v8::String>("refTime").ToLocalChecked());

        if (!maybeRefTime.IsEmpty()) {
            v8::Local<v8::Value> refTimeLocal = maybeRefTime.ToLocalChecked();

            if (refTimeLocal->IsNumber()) {
                hasRefTime = true;
                refTime = Nan::To<v8::Number>(refTimeLocal).ToLocalChecked()->Value();
            }
        }

        //start
        started = true;
    }

    /**
     * Cubic-in time function.
     */
    static double cubicIn(double t) {
        return pow(t, 3);
    }

    /**
     * Cubic-out time function.
     */
    static double cubicOut(double t) {
        return 1 - cubicIn(1 - t);
    }

    /**
     * Cubic-in-out time function.
     */
    static double cubicInOut(double t) {
        if (t < 0.5) {
            return cubicIn(t * 2.0) / 2.0;
        }

        return 1 - cubicIn((1 - t) * 2) / 2;
    }

    /**
     * Call time function.
     */
    float timeToPosition(double t) {
        double t2 = 0;

        switch (timeFunc) {
            case TF_CUBIC_IN:
                t2 = cubicIn(t);
                break;

            case TF_CUBIC_OUT:
                t2 = cubicOut(t);
                break;

            case TF_CUBIC_IN_OUT:
                t2 = cubicInOut(t);
                break;

            case TF_LINEAR:
            default:
                t2 = t;
                break;
        }

        return start + (end - start) * t2;
    }

    /**
     * Toggle animation direction.
     *
     * Note: works only if autoreverse is enabled
     */
    void toggle() {
        if (autoreverse) {
            if (direction == FORWARD) {
                direction = BACKWARD;
            } else {
                direction = FORWARD;
            }
        }
    }

    /**
     * Apply animation value.
     *
     * @param value current property value.
     */
    void applyValue(double value) {
        if (!prop) {
            return;
        }

        //Note: only float properties supported
        FloatProperty *floatProp = static_cast<FloatProperty *>(prop);

        floatProp->setValue(value);
    }

    //TODO pause
    //TODO resume
    //TODO reset (start from beginning)

    /**
     * Stop and destroy animation.
     */
    static NAN_METHOD(Stop) {
        AminoAnim *obj = Nan::ObjectWrap::Unwrap<AminoAnim>(info.This());

        assert(obj);

        obj->stop();
    }

    /**
     * Stop animation.
     *
     * Note: has to be called on main thread!
     */
    void stop() {
        if (!destroyed) {
            //keep instance until destroyed
            retain();

            //remove animation
            if (eventHandler) {
                (static_cast<AminoGfx *>(eventHandler))->removeAnimation(this);
            }

            //free resources
            destroy();

            //release instance
            release();
        }
    }

    /**
     * End the animation.
     */
    void endAnimation() {
        if (ended) {
            return;
        }

        if (DEBUG_BASE) {
            printf("Anim: endAnimation()\n");
        }

        ended = true;

        //apply end state
        applyValue(end);

        //callback function
        if (then) {
            if (DEBUG_BASE) {
                printf("-> callback used\n");
            }

            //Note: not using async Nan call to keep order with stop
            enqueueJSCallbackUpdate(static_cast<jsUpdateCallback>(&AminoAnim::callThen), NULL, NULL);
        }

        //stop
        enqueueJSCallbackUpdate(static_cast<jsUpdateCallback>(&AminoAnim::callStop), NULL, NULL);
    }

    /**
     * Perform then() call on main thread.
     */
    void callThen(JSCallbackUpdate *update) {
        //create scope
        Nan::HandleScope scope;

        //call
        Nan::Call(*then, handle(), 0, NULL);
    }

    /**
     * Perform stop() call on main thread.
     */
    void callStop(JSCallbackUpdate *update) {
        stop();
    }

    /**
     * Next animation step.
     */
    void update(double currentTime) {
        //check active
    	if (!started || ended) {
            return;
        }

        //check remaining loops
        if (count == 0) {
            endAnimation();
            return;
        }

        //handle first start
        if (startTime == 0) {
            startTime = currentTime;
            lastTime = currentTime;
            pauseTime = 0;

            //sync with reference time
            if (hasRefTime) {
                double diff = currentTime - refTime;

                if (diff < 0) {
                    //in future: wait
                    startTime = 0;
                    lastTime = 0;
                    return;
                }

                //check passed iterations
                int cycles = diff / duration;

                if (cycles > 0) {
                    //check end of animation
                    if (count != FOREVER) {
                        if (cycles >= count) {
                            //end reached
                            endAnimation();
                            return;
                        }

                        //reduce
                        count -= cycles;
                    }

                    diff -= cycles * duration;

                    //check direction
                    if (cycles & 0x1) {
                        toggle();
                    }
                }

                //shift start time
                startTime -= diff;
            }

            //adjust animation position
            if (hasZeroPos && zeroPos > start && zeroPos <= end) {
                double pos = (zeroPos - start) / (end - start);

                //shift start time
                startTime -= pos * duration;
            }
        }

        //validate time (should never happen if time is monotonic)
        if (currentTime < startTime) {
            //smooth animation
            startTime = currentTime - (lastTime - startTime);
            lastTime = currentTime;
        }

        //process
        double timePassed = currentTime - startTime;
        double t = timePassed / duration;

        lastTime = currentTime;

        if (t > 1) {
            //end reached
            bool doToggle = false;

            if (count == FOREVER) {
                doToggle = true;
            }

            if (count > 0) {
                count--;

                if (count > 0) {
                    doToggle = true;
                } else {
                    endAnimation();
                    return;
                }
            }

            if (doToggle) {
                //next cycle

                //calc exact time offset
                double overTime = timePassed - duration;

                if (overTime > duration) {
                    int times = overTime / duration;

                    overTime -= times * duration;

                    if (times & 0x1) {
                        doToggle = false;
                    }
                }

                startTime = currentTime - overTime;
                t = overTime / duration;

                if (doToggle) {
                    toggle();
                }
            } else {
                //end position
                t = 1;
            }
        }

        if (direction == BACKWARD) {
            t = 1 - t;
        }

        //apply time function
        double value = timeToPosition(t);

        applyValue(value);
    }
};

/**
 * Rect factory.
 */
class AminoRectFactory : public AminoJSObjectFactory {
public:
    AminoRectFactory(Nan::FunctionCallback callback, bool hasImage);

    AminoJSObject* create() override;

private:
    bool hasImage;
};

/**
 * Rectangle node class.
 */
class AminoRect : public AminoNode {
public:
    bool hasImage;

    //color (no texture)
    FloatProperty *propR = NULL;
    FloatProperty *propG = NULL;
    FloatProperty *propB = NULL;

    //texture (image only)
    ObjectProperty *propTexture = NULL;

    //texture offset (image only)
    FloatProperty *propLeft = NULL;
    FloatProperty *propRight = NULL;
    FloatProperty *propTop = NULL;
    FloatProperty *propBottom = NULL;

    //repeat
    Utf8Property *propRepeat = NULL;
    bool repeatX = false;
    bool repeatY = false;

    AminoRect(bool hasImage): AminoNode(hasImage ? getImageViewFactory()->name:getRectFactory()->name, RECT) {
        this->hasImage = hasImage;
    }

    ~AminoRect() {
        if (!destroyed) {
            destroyAminoRect();
        }
    }

    /**
     * Free resources.
     */
    void destroy() override {
        if (destroyed) {
            return;
        }

        //instance
        destroyAminoRect();

        //base class
        AminoNode::destroy();
    }

    /**
     * Free instance resources.
     */
    void destroyAminoRect() {
        //release object values
        if (propTexture) {
            propTexture->destroy();
        }
    }

    /**
     * Setup rect properties.
     */
    void setup() override {
        AminoNode::setup();

        //register native properties
        propW = createFloatProperty("w");
        propH = createFloatProperty("h");

        propOriginX = createFloatProperty("originX");
        propOriginY = createFloatProperty("originY");

        if (hasImage) {
            propTexture = createObjectProperty("image");

            propLeft = createFloatProperty("left");
            propRight = createFloatProperty("right");
            propTop = createFloatProperty("top");
            propBottom = createFloatProperty("bottom");

            propRepeat = createUtf8Property("repeat");
        } else {
            propR = createFloatProperty("r");
            propG = createFloatProperty("g");
            propB = createFloatProperty("b");
        }
    }

    //creation

    /**
     * Get rect factory.
     */
    static AminoRectFactory* getRectFactory() {
        static AminoRectFactory *rectFactory = NULL;

        if (!rectFactory) {
            rectFactory = new AminoRectFactory(NewRect, false);
        }

        return rectFactory;
    }

    /**
     * Initialize Rect template.
     */
    static v8::Local<v8::FunctionTemplate> GetRectInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getRectFactory());

        //no methods

        //template function
        return tpl;
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(NewRect) {
        AminoJSObject::createInstance(info, getRectFactory());
    }

    //ImageView

    //creation

    /**
     * Get image view factory.
     */
    static AminoRectFactory* getImageViewFactory() {
        static AminoRectFactory *rectFactory = NULL;

        if (!rectFactory) {
            rectFactory = new AminoRectFactory(NewImageView, true);
        }

        return rectFactory;
    }

    /**
     * Initialize ImageView template.
     */
    static v8::Local<v8::FunctionTemplate> GetImageViewInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getImageViewFactory());

        //no methods

        //template function
        return tpl;
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(NewImageView) {
        AminoJSObject::createInstance(info, getImageViewFactory());
    }

    /**
     * Handle async property updates.
     */
    void handleAsyncUpdate(AsyncPropertyUpdate *update) override {
        //default: set value
        AminoJSObject::handleAsyncUpdate(update);

        //check property updates
        AnyProperty *property = update->property;

        assert(property);

        if (property == propRepeat) {
            std::string str = propRepeat->value;

            if (str == "no-repeat") {
                repeatX = false;
                repeatY = false;
            } else if (str == "repeat") {
                repeatX = true;
                repeatY = true;
            } else if (str == "repeat-x") {
                repeatX = true;
                repeatY = false;
            } else if (str == "repeat-y") {
                repeatX = false;
                repeatY = true;
            } else {
                //error
                printf("unknown repeat mode: %s\n", str.c_str());
            }

            return;
        }
    }
};

/**
 * Polygon factory.
 */
class AminoPolygonFactory : public AminoJSObjectFactory {
public:
    AminoPolygonFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * AminoPolygon node class.
 */
class AminoPolygon : public AminoNode {
public:
    //fill
    FloatProperty *propFillR;
    FloatProperty *propFillG;
    FloatProperty *propFillB;

    //dimension
    UInt32Property *propDimension;
    BooleanProperty *propFilled;

    //points
    FloatArrayProperty *propGeometry;

    AminoPolygon(): AminoNode(getFactory()->name, POLY) {
        //empty
    }

    ~AminoPolygon() {
    }

    void setup() override {
        AminoNode::setup();

        //register native properties
        propFillR = createFloatProperty("fillR");
        propFillG = createFloatProperty("fillG");
        propFillB = createFloatProperty("fillB");

        propDimension = createUInt32Property("dimension");
        propFilled = createBooleanProperty("filled");

        propGeometry = createFloatArrayProperty("geometry");
    }

    //creation

    /**
     * Get polygon factory.
     */
    static AminoPolygonFactory* getFactory() {
        static AminoPolygonFactory *polygonFactory = NULL;

        if (!polygonFactory) {
            polygonFactory = new AminoPolygonFactory(New);
        }

        return polygonFactory;
    }

    /**
     * Initialize Group template.
     */
    static v8::Local<v8::FunctionTemplate> GetInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

        //no methods

        //template function
        return tpl;
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }
};

/**
 * Model factory.
 */
class AminoModelFactory : public AminoJSObjectFactory {
public:
    AminoModelFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * AminoModel node class.
 */
class AminoModel : public AminoNode {
public:
    //fill
    FloatProperty *propFillR;
    FloatProperty *propFillG;
    FloatProperty *propFillB;

    //arrays
    FloatArrayProperty *propVertices;
    FloatArrayProperty *propNormals;
    FloatArrayProperty *propUVs;
    UShortArrayProperty *propIndices;

    //texture
    ObjectProperty *propTexture;

    //VBO
    GLuint vboVertex = INVALID_BUFFER;
    GLuint vboNormal = INVALID_BUFFER;
    GLuint vboUV = INVALID_BUFFER;
    GLuint vboIndex = INVALID_BUFFER;

    bool vboVertexModified = true;
    bool vboNormalModified = true;
    bool vboUVModified = true;
    bool vboIndexModified = true;

    AminoModel(): AminoNode(getFactory()->name, MODEL) {
        //empty
    }

    ~AminoModel() {
        if (!destroyed) {
            destroyAminoModel();
        }
    }

    /**
     * Free all resources.
     */
    void destroy() override {
        if (destroyed) {
            return;
        }

        //instance
        destroyAminoModel();

        //base class
        AminoNode::destroy();
    }

    /**
     * Free instance resources.
     */
    void destroyAminoModel() {
        //free buffers
        if (eventHandler) {
            if (vboVertex != INVALID_BUFFER) {
                (static_cast<AminoGfx *>(eventHandler))->deleteBufferAsync(vboVertex);
                vboVertex = INVALID_BUFFER;
            }

            if (vboNormal != INVALID_BUFFER) {
                (static_cast<AminoGfx *>(eventHandler))->deleteBufferAsync(vboNormal);
                vboNormal = INVALID_BUFFER;
            }

            if (vboIndex != INVALID_BUFFER) {
                (static_cast<AminoGfx *>(eventHandler))->deleteBufferAsync(vboIndex);
                vboIndex = INVALID_BUFFER;
            }
        }
    }

    /**
     * Setup properties.
     */
    void setup() override {
        AminoNode::setup();

        //register native properties
        propW = createFloatProperty("w");
        propH = createFloatProperty("h");

        propOriginX = createFloatProperty("originX");
        propOriginY = createFloatProperty("originY");

        propFillR = createFloatProperty("fillR");
        propFillG = createFloatProperty("fillG");
        propFillB = createFloatProperty("fillB");

        propVertices = createFloatArrayProperty("vertices");
        propNormals = createFloatArrayProperty("normals");
        propUVs = createFloatArrayProperty("uvs");
        propIndices = createUShortArrayProperty("indices");

        propTexture = createObjectProperty("texture");
    }

    //creation

    /**
     * Get polygon factory.
     */
    static AminoModelFactory* getFactory() {
        static AminoModelFactory *modelFactory = NULL;

        if (!modelFactory) {
            modelFactory = new AminoModelFactory(New);
        }

        return modelFactory;
    }

    /**
     * Initialize Group template.
     */
    static v8::Local<v8::FunctionTemplate> GetInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

        //no methods

        //template function
        return tpl;
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }

    /*
     * Handle async property updates.
     */
    void handleAsyncUpdate(AsyncPropertyUpdate *update) override {
        //default: set value
        AminoJSObject::handleAsyncUpdate(update);

        //check array updates
        AnyProperty *property = update->property;

        assert(property);

        if (property == propVertices) {
            vboVertexModified = true;
        } else if (property == propNormals) {
            vboNormalModified = true;
        } else if (property == propUVs) {
            vboUVModified = true;
        } else if (property == propIndices) {
            vboIndexModified = true;
        }
    }
};

/**
 * Group factory.
 */
class AminoGroupFactory : public AminoJSObjectFactory {
public:
    AminoGroupFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

//group insert

typedef struct {
    AminoNode *child;
    size_t pos;
} group_insert_t;

/**
 * Group node.
 *
 * Special: supports clipping
 */
class AminoGroup : public AminoNode {
public:
    //internal
    std::vector<AminoNode *> children;

    //properties
    BooleanProperty *propClipRect;
    BooleanProperty *propDepth;

    AminoGroup(): AminoNode(getFactory()->name, GROUP) {
        //empty
    }

    ~AminoGroup() {
        if (!destroyed) {
            destroyAminoGroup();
        }
    }

    /**
     * Free all resources.
     */
    void destroy() override {
        if (destroyed) {
            return;
        }

        //instance
        destroyAminoGroup();

        //base
        AminoNode::destroy();
    }

    /**
     * Free children.
     */
    void destroyAminoGroup() {
        //reset children
        children.clear();
    }

    void setup() override {
        AminoNode::setup();

        //register native properties
        propW = createFloatProperty("w");
        propH = createFloatProperty("h");

        propOriginX = createFloatProperty("originX");
        propOriginY = createFloatProperty("originY");

        propClipRect = createBooleanProperty("clipRect");
        propDepth = createBooleanProperty("depth");
    }

    //creation

    /**
     * Get group factory.
     */
    static AminoGroupFactory* getFactory() {
        static AminoGroupFactory *groupFactory = NULL;

        if (!groupFactory) {
            groupFactory = new AminoGroupFactory(New);
        }

        return groupFactory;
    }

    /**
     * Initialize Group template.
     */
    static v8::Local<v8::FunctionTemplate> GetInitFunction() {
        v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

        //prototype methods
        Nan::SetPrototypeMethod(tpl, "_add", Add);
        Nan::SetPrototypeMethod(tpl, "_insert", Insert);
        Nan::SetPrototypeMethod(tpl, "_remove", Remove);

        //template function
        return tpl;
    }

private:
    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }

    /**
     * Add a child node.
     */
    static NAN_METHOD(Add) {
        assert(info.Length() == 1);

        AminoGroup *group = Nan::ObjectWrap::Unwrap<AminoGroup>(info.This());
        AminoNode *child = Nan::ObjectWrap::Unwrap<AminoNode>(Nan::To<v8::Object>(info[0]).ToLocalChecked());

        assert(group);
        assert(child);

        if (!child->checkRenderer(group)) {
            return;
        }

        //handle async
        group->enqueueValueUpdate(child, static_cast<asyncValueCallback>(&AminoGroup::addChild));
    }

    /**
     * Add a child node.
     */
    void addChild(AsyncValueUpdate *update, int state) {
        if (state != AsyncValueUpdate::STATE_APPLY) {
            return;
        }

        AminoNode *node = static_cast<AminoNode *>(update->valueObj);

        //Note: reference kept on JS side

        if (DEBUG_BASE) {
            printf("-> addChild()\n");
        }

        children.push_back(node);

        //debug (provoke crash to get stack trace)
        if (DEBUG_CRASH) {
            int *foo = (int *)1;

            *foo = 78; // trigger a SIGSEGV
            assert(false);
        }
    }

    /**
     * Insert a child node.
     */
    static NAN_METHOD(Insert) {
        assert(info.Length() == 2);

        AminoGroup *group = Nan::ObjectWrap::Unwrap<AminoGroup>(info.This());
        v8::Local<v8::Value> childValue = info[0];
        AminoNode *child = Nan::ObjectWrap::Unwrap<AminoNode>(Nan::To<v8::Object>(childValue).ToLocalChecked());
        int32_t pos = Nan::To<v8::Integer>(info[1]).ToLocalChecked()->Value();

        assert(group);
        assert(child);
        assert(pos >= 0);

        if (!child->checkRenderer(group)) {
            return;
        }

        //Note: reference kept on JS side

        //handle async
        group_insert_t *data = new group_insert_t();

        data->child = child;
        data->pos = pos;

        group->enqueueValueUpdate(childValue, data, static_cast<asyncValueCallback>(&AminoGroup::insertChild));
    }

    /**
     * Add a child node.
     */
    void insertChild(AsyncValueUpdate *update, int state) {
        if (state == AsyncValueUpdate::STATE_APPLY) {
            group_insert_t *data = (group_insert_t *)update->data;

            assert(data);

            if (DEBUG_BASE) {
                printf("-> insertChild()\n");
            }

            children.insert(children.begin() + data->pos, data->child);
        } else if (state == AsyncValueUpdate::STATE_DELETE) {
            //on main thread
            group_insert_t *data = (group_insert_t *)update->data;

            assert(data);

            //free
            delete data;
            update->data = NULL;
        }
    }

    /**
     * Remove a child node.
     */
    static NAN_METHOD(Remove) {
        assert(info.Length() == 1);

        AminoGroup *group = Nan::ObjectWrap::Unwrap<AminoGroup>(info.This());
        AminoNode *child = Nan::ObjectWrap::Unwrap<AminoNode>(Nan::To<v8::Object>(info[0]).ToLocalChecked());

        assert(group);
        assert(child);

        //handle async
        group->enqueueValueUpdate(child, static_cast<asyncValueCallback>(&AminoGroup::removeChild));
    }

    /**
     * Remove a child node.
     */
    void removeChild(AsyncValueUpdate *update, int state) {
        if (state != AsyncValueUpdate::STATE_APPLY) {
            return;
        }

        if (DEBUG_BASE) {
            printf("-> removeChild()\n");
        }

        AminoNode *node = static_cast<AminoNode *>(update->valueObj);

        //remove pointer
        std::vector<AminoNode *>::iterator pos = std::find(children.begin(), children.end(), node);

        assert(pos != children.end());

        children.erase(pos);
    }
};

//font shader

typedef struct {
    float x, y, z;    // position
    float s, t;       // texture pos
} vertex_t;

#endif
