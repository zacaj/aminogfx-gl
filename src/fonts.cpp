#include "fonts.h"

#include "fonts/utf8-utils.h"
#include "fonts/shader.h"
#include "base.h"

#include <cmath>

#define DEBUG_FONTS false

//
// AminoFonts
//

AminoFonts::AminoFonts(): AminoJSObject(getFactory()->name) {
    //empty
}

AminoFonts::~AminoFonts() {
    //empty
}

/**
 * Get factory instance.
 */
AminoFontsFactory* AminoFonts::getFactory() {
    static AminoFontsFactory *aminoFontsFactory = NULL;

    if (!aminoFontsFactory) {
        aminoFontsFactory = new AminoFontsFactory(New);
    }

    return aminoFontsFactory;
}

/**
 * Add class template to module exports.
 */
NAN_MODULE_INIT(AminoFonts::Init) {
    AminoFontsFactory *factory = getFactory();
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(factory);

    //prototype methods

    // -> no native methods
    Nan::SetTemplate(tpl, "Font", AminoFont::GetInitFunction());
    Nan::SetTemplate(tpl, "FontSize", AminoFontSize::GetInitFunction());

    //global template instance
    Nan::Set(target, Nan::New(factory->name).ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoFonts::New) {
    AminoJSObject::createInstance(info, getFactory());
}

//
//  AminoFontsFactory
//

/**
 * Create AminoFonts factory.
 */
AminoFontsFactory::AminoFontsFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoFonts", callback) {
    //empty
}

/**
 * Create AminoFonts instance.
 */
AminoJSObject* AminoFontsFactory::create() {
    return new AminoFonts();
}

//
// AminoFont
//

AminoFont::AminoFont(): AminoJSObject(getFactory()->name) {
    //empty
}

AminoFont::~AminoFont() {
    if (!destroyed) {
        destroyAminoFont();
    }
}

/**
 * Destroy font data.
 */
void AminoFont::destroy() {
    if (destroyed) {
        return;
    }

    //instance
    destroyAminoFont();

    //base class
    AminoJSObject::destroy();
}

/**
 * Destroy font data.
 */
void AminoFont::destroyAminoFont() {
    //font sizes
    for (std::map<uint32_t, texture_font_t *>::iterator it = fontSizes.begin(); it != fontSizes.end(); it++) {
        texture_font_delete(it->second);
    }

    fontSizes.clear();

    //atlas
    if (atlas) {
        texture_atlas_delete(atlas);
        atlas = NULL;
    }

    //font data
    fontData.Reset();
}

/**
 * Get factory instance.
 */
AminoFontFactory* AminoFont::getFactory() {
    static AminoFontFactory *aminoFontFactory = NULL;

    if (!aminoFontFactory) {
        aminoFontFactory = new AminoFontFactory(New);
    }

    return aminoFontFactory;
}

/**
 * Initialize Group template.
 */
v8::Local<v8::FunctionTemplate> AminoFont::GetInitFunction() {
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

    //no methods

    //template function
    return tpl;
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoFont::New) {
    AminoJSObject::createInstance(info, getFactory());
}

/**
 * Initialize fonts instance.
 */
void AminoFont::preInit(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.Length() == 2);

    AminoFonts *fonts = Nan::ObjectWrap::Unwrap<AminoFonts>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
    v8::Local<v8::Object> fontData = Nan::To<v8::Object>(info[1]).ToLocalChecked();

    assert(fonts);

    this->fonts = fonts;

    //store font (in memory)
    v8::Local<v8::Object> bufferObj = Nan::To<v8::Object>(Nan::Get(fontData, Nan::New<v8::String>("data").ToLocalChecked()).ToLocalChecked()).ToLocalChecked();

    this->fontData.Reset(bufferObj);

    //create atlas
    atlas = texture_atlas_new(512, 512, 1); //depth must be 1

    if (!atlas) {
        Nan::ThrowTypeError("could not create atlas");
        return;
    }

    //metadata
    v8::Local<v8::Value> nameValue = Nan::Get(fontData, Nan::New<v8::String>("name").ToLocalChecked()).ToLocalChecked();
    v8::Local<v8::Value> styleValue = Nan::Get(fontData, Nan::New<v8::String>("style").ToLocalChecked()).ToLocalChecked();

    fontName = AminoJSObject::toString(nameValue);
    fontWeight = Nan::To<v8::Integer>(Nan::Get(fontData, Nan::New<v8::String>("weight").ToLocalChecked()).ToLocalChecked()).ToLocalChecked()->Value();
    fontStyle = AminoJSObject::toString(styleValue);

    if (DEBUG_FONTS) {
        printf("-> new font: name=%s, style=%s, weight=%i\n", fontName.c_str(), fontStyle.c_str(), fontWeight);
    }
}

/**
 * Load font size.
 *
 * Note: has to be called in v8 thread.
 */
texture_font_t *AminoFont::getFontWithSize(uint32_t size) {
    //check cache
    std::map<uint32_t, texture_font_t *>::iterator it = fontSizes.find(size);
    texture_font_t *fontSize;

    if (it == fontSizes.end()) {
        //add new size
        v8::Local<v8::Object> bufferObj = Nan::New(fontData);
        char *buffer = node::Buffer::Data(bufferObj);
        size_t bufferLen = node::Buffer::Length(bufferObj);

        //Note: has texture id but we use our own handling
        fontSize = texture_font_new_from_memory(atlas, size, buffer, bufferLen, library);

        if (fontSize) {
            fontSizes[size] = fontSize;

            //use single FreeType instance
            library = fontSize->library;
        }

        if (DEBUG_FONTS) {
            std::string info = getFontInfo();

            printf("-> new font size: %i (%s)\n", size, info.c_str());
        }
    } else {
        fontSize = it->second;
    }

    return fontSize;
}

/**
 * Get Unique font info string.
 */
std::string AminoFont::getFontInfo() {
    return fontName + "/" + fontStyle + "/" + std::to_string(fontWeight);
}

FT_Library AminoFont::library = NULL;

//
//  AminoFontFactory
//

/**
 * Create AminoFont factory.
 */
AminoFontFactory::AminoFontFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoFont", callback) {
    //empty
}

/**
 * Create AminoFonts instance.
 */
AminoJSObject* AminoFontFactory::create() {
    return new AminoFont();
}

//
// AminoFontSize
//

AminoFontSize::AminoFontSize(): AminoJSObject(getFactory()->name) {
    //empty
}

AminoFontSize::~AminoFontSize() {
    //empty
}

/**
 * Get factory instance.
 */
AminoFontSizeFactory* AminoFontSize::getFactory() {
    static AminoFontSizeFactory *aminoFontSizeFactory = NULL;

    if (!aminoFontSizeFactory) {
        aminoFontSizeFactory = new AminoFontSizeFactory(New);
    }

    return aminoFontSizeFactory;
}

/**
 * Initialize AminoFontSize template.
 */
v8::Local<v8::FunctionTemplate> AminoFontSize::GetInitFunction() {
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

    //methods
    Nan::SetPrototypeMethod(tpl, "_calcTextWidth", CalcTextWidth);
    Nan::SetPrototypeMethod(tpl, "getFontMetrics", GetFontMetrics);

    //template function
    return tpl;
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoFontSize::New) {
    AminoJSObject::createInstance(info, getFactory());
}

/**
 * Initialize constructor values.
 */
void AminoFontSize::preInit(Nan::NAN_METHOD_ARGS_TYPE info) {
    assert(info.Length() == 2);

    AminoFont *font = Nan::ObjectWrap::Unwrap<AminoFont>(Nan::To<v8::Object>(info[0]).ToLocalChecked());
    uint32_t size = Nan::To<v8::Uint32>(info[1]).ToLocalChecked()->Value();

    assert(font);

    this->font = font;
    fontTexture = font->getFontWithSize(size);

    if (!fontTexture) {
        Nan::ThrowTypeError("could not create font size");
    }

    //font properties
    v8::Local<v8::Object> obj = handle();

    Nan::Set(obj, Nan::New("name").ToLocalChecked(), Nan::New<v8::String>(font->fontName).ToLocalChecked());
    Nan::Set(obj, Nan::New("size").ToLocalChecked(), Nan::New<v8::Number>(size));
    Nan::Set(obj, Nan::New("weight").ToLocalChecked(), Nan::New<v8::Number>(font->fontWeight));
    Nan::Set(obj, Nan::New("style").ToLocalChecked(), Nan::New<v8::String>(font->fontStyle).ToLocalChecked());
}

/**
 * Calculate text width.
 */
NAN_METHOD(AminoFontSize::CalcTextWidth) {
    assert(info.Length() == 1);

    AminoFontSize *obj = Nan::ObjectWrap::Unwrap<AminoFontSize>(info.This());
    Nan::Utf8String str(info[0]);

    assert(obj);

    info.GetReturnValue().Set(obj->getTextWidth(*str));
}

/**
 * Calculate text width.
 */
float AminoFontSize::getTextWidth(const char *text) {
    size_t len = utf8_strlen(text);
    char *textPos = (char *)text;
    char *lastTextPos = NULL;
    float w = 0;

    AminoText::initFreeTypeMutex();
    uv_mutex_lock(&AminoText::freeTypeMutex);

    size_t lastGlyphCount = fontTexture->glyphs->size;

    for (std::size_t i = 0; i < len; i++) {
        texture_glyph_t *glyph = texture_font_get_glyph(fontTexture, textPos);

        if (!glyph) {
            printf("Error: got empty glyph from texture_font_get_glyph()\n");
            continue;
        }

        //kerning
        if (lastTextPos) {
            w += texture_glyph_get_kerning(glyph, lastTextPos);
        }

        //char width
        w += glyph->advance_x;

        //next
        size_t charLen = utf8_surrogate_len(textPos);

        lastTextPos = textPos;
        textPos += charLen;
    }

    bool glyphsChanged = lastGlyphCount != fontTexture->glyphs->size;

    uv_mutex_unlock(&AminoText::freeTypeMutex);

    if (glyphsChanged) {
        texture_atlas_t *atlas = fontTexture->atlas;

        //update all instances
        AminoGfx::updateAtlasTextures(atlas);
    }

    return w;
}

/**
 * Get font metrics (height, ascender, descender).
 */
NAN_METHOD(AminoFontSize::GetFontMetrics) {
    AminoFontSize *obj = Nan::ObjectWrap::Unwrap<AminoFontSize>(info.This());

    assert(obj);

    //metrics
    v8::Local<v8::Object> metricsObj = Nan::New<v8::Object>();

    Nan::Set(metricsObj, Nan::New("height").ToLocalChecked(), Nan::New<v8::Number>(obj->fontTexture->ascender - obj->fontTexture->descender));
    Nan::Set(metricsObj, Nan::New("ascender").ToLocalChecked(), Nan::New<v8::Number>(obj->fontTexture->ascender));
    Nan::Set(metricsObj, Nan::New("descender").ToLocalChecked(), Nan::New<v8::Number>(obj->fontTexture->descender));

    info.GetReturnValue().Set(metricsObj);
}

//
//  AminoFontSizeFactory
//

/**
 * Create AminoFontSize factory.
 */
AminoFontSizeFactory::AminoFontSizeFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoFontSize", callback) {
    //empty
}

/**
 * Create AminoFonts instance.
 */
AminoJSObject* AminoFontSizeFactory::create() {
    return new AminoFontSize();
}

//
// AminoFontShader
//

AminoFontShader::AminoFontShader() : TextureShader() {
    //shader

    //Note: using unmodified vertex shader
    fragmentShader = R"(
        #ifdef GL_ES
            precision mediump float;
        #endif

        uniform float opacity;
        uniform vec3 color;
        uniform sampler2D tex;

        varying vec2 uv;

        void main() {
            float a = texture2D(tex, uv).a;

            gl_FragColor = vec4(color, opacity * a);
        }
    )";
}

/**
 * Initialize the font shader.
 */
void AminoFontShader::initShader() {
    TextureShader::initShader();

    //uniforms
    uColor = getUniformLocation("color");
}

/**
 * Set color.
 */
void AminoFontShader::setColor(GLfloat color[3]) {
    glUniform3f(uColor, color[0], color[1], color[2]);
}

/**
 * Get texture for atlas.
 *
 * Note: has to be called on OpenGL thread.
 */
amino_atlas_t AminoFontShader::getAtlasTexture(texture_atlas_t *atlas, bool createIfMissing, bool &newTexture) {
    std::map<texture_atlas_t *, amino_atlas_t>::iterator it = atlasTextures.find(atlas);

    newTexture = false;

    if (it == atlasTextures.end()) {
        if (!createIfMissing) {
            amino_atlas_t item = { INVALID_TEXTURE };

            return item;
        }

        //create new one
        GLuint id = INVALID_TEXTURE;

        //see https://webcache.googleusercontent.com/search?q=cache:EZ3HLutV3zwJ:https://github.com/rougier/freetype-gl/blob/master/texture-atlas.c+&cd=1&hl=de&ct=clnk&gl=ch
        glGenTextures(1, &id);
        newTexture = true;

        assert(id != INVALID_TEXTURE);

        glBindTexture(GL_TEXTURE_2D, id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        //best quality
        //Note: padding adjustment in freetype-gl needed to solve vertical thin line issue at boundaries (see https://github.com/rougier/freetype-gl/issues/123)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        //much worse quality
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        amino_atlas_t item;

        item.textureId = id;

        atlasTextures[atlas] = item;

        //debug
        //printf("create new atlas texture: %i (total: %i)\n", id, (int)atlasTextures.size());

        return item;
    }

    return it->second;
}
