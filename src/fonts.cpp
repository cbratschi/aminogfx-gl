#include "fonts.h"

#include "fonts/utf8-utils.h"
#include "fonts/shader.h"

#include <cmath>

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
    static AminoFontsFactory *aminoFontsFactory;

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
    //empty
}

void AminoFont::destroy() {
    AminoJSObject::destroy();

    //font sizes
    for (std::map<int, texture_font_t *>::iterator it = fontSizes.begin(); it != fontSizes.end(); it++) {
        texture_font_delete(it->second);
    }

    fontSizes.clear();

    //atlas
    if (atlas) {
        texture_atlas_delete(atlas);
    }

    //font data
    fontData.Reset();
}

/**
 * Get factory instance.
 */
AminoFontFactory* AminoFont::getFactory() {
    static AminoFontFactory *aminoFontFactory;

    if (!aminoFontFactory) {
        aminoFontFactory = new AminoFontFactory(New);
    }

    return aminoFontFactory;
}

/**
 * Initialize Group template.
 */
v8::Local<v8::Function> AminoFont::GetInitFunction() {
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

    //no methods

    //template function
    return Nan::GetFunction(tpl).ToLocalChecked();
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoFont::New) {
    AminoJSObject::createInstance(info, getFactory());
}

void AminoFont::preInit(Nan::NAN_METHOD_ARGS_TYPE info) {
    AminoFonts *parent = Nan::ObjectWrap::Unwrap<AminoFonts>(info[0]->ToObject());
    v8::Local<v8::Object> fontData = info[1]->ToObject();

    this->parent = parent;

    //store font (in memory)
    v8::Local<v8::Object> bufferObj =  Nan::Get(fontData, Nan::New<v8::String>("data").ToLocalChecked()).ToLocalChecked()->ToObject();

    this->fontData.Reset(bufferObj);

    //create atlas
    atlas = texture_atlas_new(512, 512, 1);

    if (!atlas) {
        Nan::ThrowTypeError("could not create atlas");
        return;
    }

    //metadata
    v8::Local<v8::Value> nameValue = Nan::Get(fontData, Nan::New<v8::String>("name").ToLocalChecked()).ToLocalChecked();
    v8::Local<v8::Value> styleValue = Nan::Get(fontData, Nan::New<v8::String>("style").ToLocalChecked()).ToLocalChecked();

    name = AminoJSObject::toString(nameValue);
    weight = Nan::Get(fontData, Nan::New<v8::String>("weight").ToLocalChecked()).ToLocalChecked()->NumberValue();
    style = AminoJSObject::toString(styleValue);
}

/**
 * Load font size.
 *
 * Note: has to be called in v8 thread.
 */
texture_font_t *AminoFont::getFontWithSize(int size) {
    //check cache
    std::map<int, texture_font_t *>::iterator it = fontSizes.find(size);
    texture_font_t *fontSize;

    if (it == fontSizes.end()) {
        //add new size
        v8::Local<v8::Object> bufferObj = Nan::New(fontData);
        char *buffer = node::Buffer::Data(bufferObj);
        size_t bufferLen = node::Buffer::Length(bufferObj);

        //Note: has texture id but we use our own handling
        fontSize = texture_font_new_from_memory(atlas, size, buffer, bufferLen);

        if (fontSize) {
            fontSizes[size] = fontSize;
        }
    } else {
        fontSize = it->second;
    }

    return fontSize;
}

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
    static AminoFontSizeFactory *aminoFontSizeFactory;

    if (!aminoFontSizeFactory) {
        aminoFontSizeFactory = new AminoFontSizeFactory(New);
    }

    return aminoFontSizeFactory;
}

/**
 * Initialize Group template.
 */
v8::Local<v8::Function> AminoFontSize::GetInitFunction() {
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

    //methods
    Nan::SetPrototypeMethod(tpl, "_calcTextWidth", CalcTextWidth);
    Nan::SetPrototypeMethod(tpl, "getFontMetrics", GetFontMetrics);

    //template function
    return Nan::GetFunction(tpl).ToLocalChecked();
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoFontSize::New) {
    AminoJSObject::createInstance(info, getFactory());
}

void AminoFontSize::preInit(Nan::NAN_METHOD_ARGS_TYPE info) {
    AminoFont *parent = Nan::ObjectWrap::Unwrap<AminoFont>(info[0]->ToObject());
    int size = (int)round(info[1]->NumberValue());

    this->parent = parent;
    fontTexture = parent->getFontWithSize(size);

    if (!fontTexture) {
        Nan::ThrowTypeError("could not create font size");
    }

    //font properties
    v8::Local<v8::Object> obj = handle();

    Nan::Set(obj, Nan::New("name").ToLocalChecked(), Nan::New<v8::String>(parent->name).ToLocalChecked());
    Nan::Set(obj, Nan::New("size").ToLocalChecked(), Nan::New<v8::Number>(size));
    Nan::Set(obj, Nan::New("weight").ToLocalChecked(), Nan::New<v8::Number>(parent->weight));
    Nan::Set(obj, Nan::New("style").ToLocalChecked(), Nan::New<v8::String>(parent->style).ToLocalChecked());
}

NAN_METHOD(AminoFontSize::CalcTextWidth) {
    AminoFontSize *obj = Nan::ObjectWrap::Unwrap<AminoFontSize>(info.This());
    v8::String::Utf8Value str(info[0]);

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

    for (std::size_t i = 0; i < len; i++) {
        texture_glyph_t *glyph = texture_font_get_glyph(fontTexture, textPos);

        if (!glyph) {
            printf("Error: got empty glyph from texture_font_get_glyph\n");
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

    return w;
}

NAN_METHOD(AminoFontSize::GetFontMetrics) {
    AminoFontSize *obj = Nan::ObjectWrap::Unwrap<AminoFontSize>(info.This());

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

AminoFontShader::AminoFontShader(std::string shaderPath) {
    loadShader(shaderPath);
}

AminoFontShader::~AminoFontShader() {
    glDeleteProgram(shader);
}

void AminoFontShader::loadShader(std::string shaderPath) {
    std::string vert = shaderPath + "/v3f-t2f.vert";
    std::string frag = shaderPath + "/v3f-t2f.frag";

    //printf("shader: vertex=%s fragment=%s\n", vert.c_str(), frag.c_str());

    shader = shader_load(vert.c_str(), frag.c_str());

    //get uniforms
    texUni   = glGetUniformLocation(shader, "texture");
    mvpUni   = glGetUniformLocation(shader, "mvp");
    transUni = glGetUniformLocation(shader, "trans");
    colorUni = glGetUniformLocation(shader, "color");
}

GLuint AminoFontShader::getAtlasTexture(texture_atlas_t *atlas) {
    std::map<texture_atlas_t *, GLuint>::iterator it = atlasTextures.find(atlas);

    if (it == atlasTextures.end()) {
        //create new one
        GLuint id;

        //see https://webcache.googleusercontent.com/search?q=cache:EZ3HLutV3zwJ:https://github.com/rougier/freetype-gl/blob/master/texture-atlas.c+&cd=1&hl=de&ct=clnk&gl=ch
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        //subpixel error on Mac retina display at edges
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        //worse on retina but no pixel errors
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        atlasTextures[atlas] = id;

        return id;
    }

    return it->second;
}