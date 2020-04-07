#include "images.h"
#include "base.h"

#include <uv.h>

extern "C" {
    // #include "libavutil/imgutils.h"
    // #include "libswscale/swscale.h"

    // #include <jpeglib.h>

    #define PNG_SKIP_SETJMP_CHECK
    #include <png.h>
}

#define DEBUG_IMAGES false
#define DEBUG_IMAGES_CONSOLE false

//
// libjpeg error handler
//
// See https://github.com/ellzey/libjpeg/blob/master/example.c
//

// struct myjpeg_error_mgr {
//     struct jpeg_error_mgr pub;	/* "public" fields */

//     jmp_buf setjmp_buffer;	/* for return to caller */
// };

// typedef struct myjpeg_error_mgr *myjpeg_error_ptr;

// METHODDEF(void) myjpeg_error_exit(j_common_ptr cinfo) {
//     /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
//     myjpeg_error_ptr myerr = (myjpeg_error_ptr) cinfo->err;

//     /* Always display the message. */
//     /* We could postpone this until after returning, if we chose. */
//     (*cinfo->err->output_message)(cinfo);

//     /* Return control to the setjmp point */
//     longjmp(myerr->setjmp_buffer, 1);
// }

//
// libpng handlers
//

typedef struct {
    png_byte *data;
    png_size_t size;
} DataHandle;

typedef struct {
    DataHandle data;
    png_size_t offset;
    int extra;
} ReadDataHandle;

static void read_png_data_callback(png_structp png_ptr, png_byte *raw_data, png_size_t read_length) {
    ReadDataHandle *handle = (ReadDataHandle *)png_get_io_ptr(png_ptr);
    const png_byte *png_src = handle->data.data + handle->offset;

    memcpy(raw_data, png_src, read_length);
    handle->offset += read_length;
}

//
// AsyncImageWorker
//

/**
 * Asynchronous image loader.
 */
class AsyncImageWorker : public Nan::AsyncWorker {
private:
    //input buffer
    char *buffer;
    size_t bufferLen;
    int32_t maxWH;

    //image
    char *imgData = NULL;
    int32_t imgDataLen = 0;
    int32_t imgW;
    int32_t imgH;
    bool imgAlpha;
    int32_t imgBPP;

public:
    AsyncImageWorker(Nan::Callback *callback, v8::Local<v8::Object> &obj, v8::Local<v8::Value> &bufferObj, int32_t maxWH) : AsyncWorker(callback) {
        SaveToPersistent("object", obj);

        //process buffer
        SaveToPersistent("buffer", bufferObj);

        buffer = node::Buffer::Data(bufferObj);
        bufferLen = node::Buffer::Length(bufferObj);
        this->maxWH = maxWH;

        //debug
        //this->maxWH = 10;
    }

    /**
     * Async running code.
     */
    void Execute() {
        if (DEBUG_THREADS) {
            uv_thread_t threadId = uv_thread_self();

            printf("async image loading: start (thread=%lu)\n", (unsigned long)threadId);
        }

        if (DEBUG_IMAGES) {
            printf("-> async image loading started\n");
        }

        //check image type

        // 1) PNG (header)
        bool isPng = bufferLen > 8 &&
            buffer[0] == (char)137 &&
            buffer[1] == (char)80 &&
            buffer[2] == (char)78 &&
            buffer[3] == (char)71 &&
            buffer[4] == (char)13 &&
            buffer[5] == (char)10 &&
            buffer[6] == (char)26 &&
            buffer[7] == (char)10;

        //decode image
        bool res;
        if (isPng) {
            char sum = 0;
            for (int i=0; i<bufferLen; i++)
                sum+=buffer[i];
                printf("sum: %i, sizs %i\n", sum, bufferLen);
            res = decodePng();
        }
        else {
            printf("%i %i %i %i %i %i %i %i %i\n", bufferLen, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
            assert(isPng);
        }
        // } else {
        //     res = decodeJpeg();
        // }

        //resize
        if (res) {
            resizeImage();
        }

        if (DEBUG_THREADS) {
            printf("async image loading: done\n");
        }
    }

    /**
     * Decode PNG image (using libpng).
     *
     * See http://www.learnopengles.com/loading-a-png-into-memory-and-displaying-it-as-a-texture-with-opengl-es-2-using-almost-the-same-code-on-ios-android-and-emscripten/.
     */
    bool decodePng() {
        //Note: header already checked

        //init
        png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

        if (!png_ptr) {
            SetErrorMessage("could not init PNG decoder");
            return false;
        }

        png_infop info_ptr = png_create_info_struct(png_ptr);

        if (!info_ptr) {
            SetErrorMessage("could not init PNG decoder");

            png_destroy_read_struct(&png_ptr, NULL, NULL);

            return false;
        }

        //data handler
        // ReadDataHandle png_data_handle = (ReadDataHandle){{ (unsigned char *)buffer, bufferLen }, 0};
        ReadDataHandle png_data_handle;
        png_data_handle.data.data = (unsigned char *)buffer;
        png_data_handle.data.size = bufferLen;
        png_data_handle.offset = 0;

        png_set_read_fn(png_ptr, &png_data_handle, read_png_data_callback);

        //error handler
        if (setjmp(png_jmpbuf(png_ptr))) {
            SetErrorMessage("could not decode PNG");

            png_read_end(png_ptr, info_ptr);
            png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

            if (imgData) {
                free(imgData);
                imgData = NULL;
            }

            return false;
        }

        //PNG header
        png_uint_32 width, height;
        int bit_depth, color_type;

        png_read_info(png_ptr, info_ptr);
        png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

        if (DEBUG_IMAGES) {
            printf("-> got an image %dx%d (depth=%i, type=%i)\n", (int)width, (int)height, (int)bit_depth, (int)color_type);
            printf("-> data size = %d\n", imgDataLen);
        }

        //convert transparency to full alpha
        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
            png_set_tRNS_to_alpha(png_ptr);
        }

        //convert grayscale, if needed.
        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
            png_set_expand_gray_1_2_4_to_8(png_ptr);
        }

        //convert paletted images, if needed.
        if (color_type == PNG_COLOR_TYPE_PALETTE) {
            png_set_palette_to_rgb(png_ptr);
        }

        //add alpha channel, if there is none.
        //rationale: GL_RGBA is faster than GL_RGB on many GPUs)
        /*
        if (color_type == PNG_COLOR_TYPE_PALETTE || color_type == PNG_COLOR_TYPE_RGB) {
            png_set_add_alpha(png_ptr, 0xFF, PNG_FILLER_AFTER);
        }
        */

        //ensure 8-bit packing
        if (bit_depth < 8) {
            png_set_packing(png_ptr);
        } else if (bit_depth == 16) {
            //Note: not part of libjpeg-turbo
#ifdef MAC
            png_set_scale_16(png_ptr);
#endif
        }

        //get final info
        png_read_update_info(png_ptr, info_ptr);

        const png_byte colorType = png_get_color_type(png_ptr, info_ptr);
        const png_size_t rowSize = png_get_rowbytes(png_ptr, info_ptr);
        const png_byte bitDepth = png_get_bit_depth(png_ptr, info_ptr);

        assert(rowSize > 0);

        imgW = width;
        imgH = height;
        imgAlpha = (colorType == PNG_COLOR_TYPE_RGB_ALPHA) || (colorType == PNG_COLOR_TYPE_GRAY_ALPHA);

        switch (colorType) {
            case PNG_COLOR_TYPE_GRAY:
                assert(bitDepth == 8);
                imgBPP = 1;
                break;

            case PNG_COLOR_TYPE_RGB:
                assert(bitDepth == 8);
                imgBPP = 3;
                break;

            case PNG_COLOR_TYPE_RGB_ALPHA:
                assert(bitDepth == 8);
                imgBPP = 4;
                break;

            case PNG_COLOR_TYPE_GRAY_ALPHA:
                assert(bitDepth == 8);
                imgBPP = 2;
                break;

            default:
                //fallback
                printf("unknown color type: %i\n", (int)colorType);

                imgBPP = rowSize / width;
                break;
        }

        if (DEBUG_IMAGES) {
            printf("-> output image %dx%d (bpp=%i, alpha=%i, type=%i)\n", (int)width, (int)height, (int)imgBPP, (int)imgAlpha, (int)colorType);
        }

        //decode

        //printf("-> row=%i calc=%i\n", (int)rowSize, width * imgBPP);
        //printf("-> size=%ix%i, alpha=%i, bpp=%i\n", imgW, imgH, imgAlpha ? 1:0, imgBPP);

        imgDataLen = rowSize * height;
        imgData = (char *)malloc(imgDataLen);

        assert(imgData != NULL);

        png_byte** row_ptrs = new png_byte*[height];

        for (png_uint_32 i = 0; i < height; i++) {
            row_ptrs[i] = (unsigned char *)imgData + i * rowSize;
        }

        png_read_image(png_ptr, &row_ptrs[0]);

        //done
        png_read_end(png_ptr, info_ptr);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

        if (DEBUG_IMAGES) {
            printf("-> size=%ix%i, alpha=%i, bpp=%i\n", imgW, imgH, imgAlpha ? 1:0, imgBPP);
        }

        delete[] row_ptrs;

        return true;
    }

    /**
     * Decode JPEG image (using libjpeg).
     *
     * Note: NOT thread-safe! Has to be called in single queue.
     */
    bool decodeJpeg() {
        assert(false);
#if 0
        if (DEBUG_IMAGES) {
            printf("decodeJpeg()\n");
        }

        //check thread
        /*
        uv_thread_t threadId = uv_thread_self();

        printf("JPEG thread: %lu\n", threadId);
        */

        struct jpeg_decompress_struct cinfo;

        //error handler
        struct myjpeg_error_mgr jerr;

        cinfo.err = jpeg_std_error(&jerr.pub);
        jerr.pub.error_exit = myjpeg_error_exit;

        if (setjmp(jerr.setjmp_buffer)) {
            SetErrorMessage("error decoding JPEG file");

            jpeg_destroy_decompress(&cinfo);

            if (imgData) {
                free(imgData);
                imgData = NULL;
            }

            return false;
        }

        //init
        jpeg_create_decompress(&cinfo);

        //input data
        jpeg_mem_src(&cinfo, (unsigned char *)buffer, bufferLen);

        //decompress
        int rc = jpeg_read_header(&cinfo, TRUE);

        if (rc != 1) {
            SetErrorMessage("error not a JPEG file");

            jpeg_destroy_decompress(&cinfo);
            return false;
        }

        jpeg_start_decompress(&cinfo);

        //get JPEG data
        imgW = cinfo.output_width;
	    imgH = cinfo.output_height;
        imgAlpha = false;
        imgBPP = cinfo.output_components;

        //read pixels
        imgDataLen = imgW * imgH * imgBPP;
	    imgData = (char *)malloc(imgDataLen); //gets transferred to buffer

        assert(imgData != NULL);

        if (DEBUG_IMAGES) {
            printf("-> got an image %dx%d\n", imgW, imgH);
            printf("-> data size = %d\n", imgDataLen);
        }

        int rowStride = cinfo.output_width * imgBPP;

        while (cinfo.output_scanline < cinfo.output_height) {
            unsigned char *bufferArray[1];

		    bufferArray[0] = (unsigned char *)imgData + cinfo.output_scanline * rowStride;

    		jpeg_read_scanlines(&cinfo, bufferArray, 1);
        }

        //done
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);

        if (DEBUG_IMAGES) {
            printf("-> size=%ix%i, alpha=%i, bpp=%i\n", imgW, imgH, imgAlpha ? 1:0, imgBPP);
        }

        return true;
#endif
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    /**
     * Resize image.
     */
    void resizeImage() {
        if (!imgW || !imgH || maxWH <= 0) {
            return;
        }

        if (imgW < maxWH && imgH < maxWH) {
            return;
        }

        assert(imgData != NULL);
        assert(false);
#if 0

        //new size
        int newW = maxWH;
        int newH = imgH * maxWH /imgW;

        if (newH > maxWH) {
            float fac = maxWH / newH;

            newW *= fac;
            newH *= fac;
        }

        //initialize SWS context for software scaling
        AVPixelFormat format;

        switch (imgBPP) {
            case 1:
                format = AV_PIX_FMT_GRAY8;
                break;

            case 2:
                format = AV_PIX_FMT_YA8; //AV_PIX_FMT_GRAY8A
                break;

            case 3:
                format = AV_PIX_FMT_RGB24;
                break;

            case 4:
                format = AV_PIX_FMT_RGBA;
                break;

            default:
                return;
        }

        //debug
        //printf("new size: %ix%i\n", newW, newH);

        //Note: FFmpeg version
        /*
        const int dstAlign = 32;
        int dstDataLen = av_image_get_buffer_size(format, newW, newH, dstAlign);
        struct SwsContext *sws_ctx = sws_getContext(imgW, imgH, format, newW, newH, format, SWS_BILINEAR, NULL, NULL, NULL);
        uint8_t *srcSlice[4], *dstSlice[4];
        int srcSlide[4], dstSlide[4];

        av_image_alloc(srcSlice, srcSlide, imgW, imgH, format, 1);
        av_image_fill_pointers(srcSlice, format, imgH, (uint8_t *)imgData, srcSlide);
        av_image_alloc(dstSlice, dstSlide, newW, newH, format, dstAlign);
        sws_scale(sws_ctx, srcSlice, srcSlide, 0, imgH, dstSlice, dstSlide);

        int dataLen = newW * newH * imgBPP;
        char *data = (char *)malloc(dataLen);

        av_image_copy_to_buffer((uint8_t *)data, dataLen, dstSlice, dstSlide, format, newW, newH, 1);

        av_freep(&srcSlice[0]);
        av_freep(&dstSlice[0]);
        */

        /*
         * Note: using libav compatible code (deprecated on FFmpeg)
         *
         * Seeing some alignment warnings on macOS.
         */
        int dataLen = avpicture_get_size(format, newW, newH);
        char *data = (char *)malloc(dataLen);
        AVPicture srcPic, dstPic;
        struct SwsContext *sws_ctx = sws_getContext(imgW, imgH, format, newW, newH, format, SWS_BILINEAR, NULL, NULL, NULL);

        assert(data != NULL);

        avpicture_fill(&srcPic, (uint8_t *)imgData, format, imgW, imgH);
        avpicture_alloc(&dstPic, format, newW, newH);
        sws_scale(sws_ctx, srcPic.data, srcPic.linesize, 0, imgH, dstPic.data, dstPic.linesize);
        avpicture_layout(&dstPic, format, newW, newH, (unsigned char *)data, dataLen);
        avpicture_free(&dstPic);

        sws_freeContext(sws_ctx);
        free(imgData);

        imgW = newW;
        imgH = newH;
        imgData = data;
        imgDataLen = dataLen;
#endif
    }

#pragma GCC diagnostic pop

    /**
     * Back in main thread with JS access.
     */
    void HandleOKCallback() {
        if (DEBUG_IMAGES) {
            printf("-> async image loading done\n");
        }

        //Note: scope already created (but needed in HandleErrorCallback())

        //result
        v8::Local<v8::Object> obj = Nan::To<v8::Object>(GetFromPersistent("object")).ToLocalChecked();
        v8::Local<v8::Object> buff;

        //transfer ownership
        buff = Nan::NewBuffer(imgData, imgDataLen).ToLocalChecked();

        //create object
        Nan::Set(obj, Nan::New("w").ToLocalChecked(),      Nan::New(imgW));
        Nan::Set(obj, Nan::New("h").ToLocalChecked(),      Nan::New(imgH));
        Nan::Set(obj, Nan::New("alpha").ToLocalChecked(),  Nan::New(imgAlpha));
        Nan::Set(obj, Nan::New("bpp").ToLocalChecked(),    Nan::New(imgBPP));
        Nan::Set(obj, Nan::New("buffer").ToLocalChecked(), buff);

        //store local values
        AminoImage *img = Nan::ObjectWrap::Unwrap<AminoImage>(obj);

        assert(img);

        img->imageLoaded(buff, imgW, imgH, imgAlpha, imgBPP);

        //call callback
        v8::Local<v8::Value> argv[] = { Nan::Null(), obj };

        Nan::Call(*callback, 2, argv);
    }
};

//
// AminoImage
//

/**
 * Constructor.
 */
AminoImage::AminoImage(): AminoJSObject(getFactory()->name) {
    //empty
}

/**
 * Destructor.
 */
AminoImage::~AminoImage()  {
    if (!destroyed) {
        destroyAminoImage();
    }
}

/**
 * Check if image is available.
 */
bool AminoImage::hasImage() {
    return w > 0;
}

/**
 * Free all resources.
 */
void AminoImage::destroy() {
    if (destroyed) {
        return;
    }

    //instance
    destroyAminoImage();

    //base class
    AminoJSObject::destroy();
}

/**
 * Free all resources.
 */
void AminoImage::destroyAminoImage() {
    buffer.Reset();
    bufferData = NULL;
    bufferLength = 0;
}

/**
 * Create texture.
 *
 * Note: only call from async handler!
 */
GLuint AminoImage::createTexture(GLuint textureId) {
    if (!hasImage()) {
        return INVALID_TEXTURE;
    }

    //debug
    if (DEBUG_IMAGES) {
        printf("createTexture(): buffer=%d, size=%ix%i, bpp=%i\n", (int)bufferLength, w, h, bpp);
    }

    return createTexture(textureId, bufferData, bufferLength, w, h, bpp);
}

/**
 * Create texture.
 *
 * Note: only call from async handler (rendering thread)!
 */
GLuint AminoImage::createTexture(GLuint textureId, char *bufferData, size_t bufferLength, int w, int h, int bpp) {
    assert(w * h * bpp == (int)bufferLength);

    GLuint texture;

    if (textureId != INVALID_TEXTURE) {
        //use existing texture
	    texture = textureId;
    } else {
        //create new texture
        texture = INVALID_TEXTURE;
	    glGenTextures(1, &texture);
        printf("glGenTexture\n");

        assert(texture != INVALID_TEXTURE);
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    //Note: glTexSubImage2D() would probably be faster for updates

    if (bpp == 3) {
        //RGB (24-bit)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, bufferData);
    } else if (bpp == 4) {
        //RGBA (32-bit)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, bufferData);
    } else if (bpp == 1) {
        //grayscale (8-bit)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, bufferData);
    } else if (bpp == 2) {
        //grayscale & alpha (16-bit)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, w, h, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, bufferData);
    } else {
        //unsupported
        printf("unsupported texture format: bpp=%d\n", bpp);
    }

    //linear scaling
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    /*
     * clamp to border
     *
     * Note: GL_CLAMP_TO_BORDER not supported by OpenGL ES 2.0
     *
     * - https://lists.cairographics.org/archives/cairo/2011-February/021715.html
     * - https://wiki.linaro.org/WorkingGroups/Middleware/Graphics/GLES2PortingTips
     *
     * Workaround: use GL_CLAMP_TO_EDGE and clip pixels outside in shader
     */

    /*
    //code below works on macOS
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    float color[] = { 0.0f, 0.0f, 0.0f, 0.0f };

    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);
    */

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return texture;
}

/**
 * Get factory instance.
 */
AminoImageFactory* AminoImage::getFactory() {
    static AminoImageFactory *aminoImageFactory = NULL;

    if (!aminoImageFactory) {
        aminoImageFactory = new AminoImageFactory(New);
    }

    return aminoImageFactory;
}

/**
 * Add class template to module exports.
 */
NAN_MODULE_INIT(AminoImage::Init) {
    if (DEBUG_IMAGES) {
        printf("AminoImage init\n");
    }

    AminoImageFactory *factory = getFactory();
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(factory);

    //prototype methods
    Nan::SetPrototypeMethod(tpl, "loadImage", loadImage);

    //global template instance
    Nan::Set(target, Nan::New(factory->name).ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoImage::New) {
    AminoJSObject::createInstance(info, getFactory());
}

/**
 * Load image asynchronously.
 */
NAN_METHOD(AminoImage::loadImage) {
    int params = info.Length();

    assert(params >= 2);

    v8::Local<v8::Value> bufferObj = info[0];
    Nan::Callback *callback = new Nan::Callback(info[1].As<v8::Function>());
    int32_t maxWH = params >= 3 ? Nan::To<v8::Int32>(info[2]).ToLocalChecked()->Value():0;
    v8::Local<v8::Object> obj = info.This();

    //async loading
    AsyncQueueWorker(new AsyncImageWorker(callback, obj, bufferObj, maxWH));
}

/**
 * Create local copy of JS values.
 */
void AminoImage::imageLoaded(v8::Local<v8::Object> &buffer, int w, int h, bool alpha, int bpp) {
    this->buffer.Reset(buffer);
    this->w = w;
    this->h = h;
    this->alpha = alpha;
    this->bpp = bpp;

    //get buffer data for OpenGL thread
    bufferData = node::Buffer::Data(buffer);
    bufferLength = node::Buffer::Length(buffer);
}

//
//  AminoImageFactory
//

/**
 * Create AminoImage factory.
 */
AminoImageFactory::AminoImageFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoImage", callback) {
    //empty
}

/**
 * Create AminoImage instance.
 */
AminoJSObject* AminoImageFactory::create() {
    return new AminoImage();
}

//
// AminoTexture
//

/**
 * Constructor.
 */
AminoTexture::AminoTexture(): AminoJSObject(getFactory()->name) {
    //empty
}

/**
 * Destructor.
 */
AminoTexture::~AminoTexture()  {
    if (!destroyed) {
        destroyAminoTexture(true);
    }

    //lock
    if (videoLockUsed) {
        uv_mutex_destroy(&videoLock);
    }
}

/**
 * Free resources.
 */
void AminoTexture::destroy() {
    if (destroyed) {
        return;
    }

    destroyAminoTexture(false);

    //Note: frees eventHandler
    AminoJSObject::destroy();
}

/**
 * Free resources (on main thread).
 */
void AminoTexture::destroyAminoTexture(bool destructorCall) {
    if (callback) {
        delete callback;
        callback = NULL;
    }

    if (videoPlayer) {
        //acquire lock (rendering thread could access the videoPlayer right now)
        uv_mutex_lock(&videoLock);
        delete videoPlayer;
        videoPlayer = NULL;
        uv_mutex_unlock(&videoLock);
    }

    //free texture
    if (textureCount > 0) {
        //Note: we are on the main thread

        if (eventHandler && ownTexture) {
            AminoGfx *gfx = static_cast<AminoGfx *>(eventHandler);

            for (int i = 0; i < textureCount; i++) {
                gfx->deleteTextureAsync(textureIds[i]);
            }
        }

        activeTexture = -1;
        delete[] textureIds;
        textureIds = NULL;
        textureCount = 0;
        ownTexture = false;

        w = 0;
        h = 0;

        if (!destructorCall) {
            //Note: we have an active scope
            v8::Local<v8::Object> obj = handle();

            Nan::Set(obj, Nan::New("w").ToLocalChecked(), Nan::Undefined());
            Nan::Set(obj, Nan::New("h").ToLocalChecked(), Nan::Undefined());
        }
    }
}

/**
 * Get factory instance.
 */
AminoTextureFactory* AminoTexture::getFactory() {
    static AminoTextureFactory *aminoTextureFactory = NULL;

    if (!aminoTextureFactory) {
        aminoTextureFactory = new AminoTextureFactory(New);
    }

    return aminoTextureFactory;
}

/**
 * Initialize Texture template.
 */
v8::Local<v8::FunctionTemplate> AminoTexture::GetInitFunction() {
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(getFactory());

    //methods
    Nan::SetPrototypeMethod(tpl, "loadTextureFromImage", LoadTextureFromImage);
    Nan::SetPrototypeMethod(tpl, "loadTextureFromVideo", LoadTextureFromVideo);
    Nan::SetPrototypeMethod(tpl, "loadTextureFromBuffer", LoadTextureFromBuffer);
    Nan::SetPrototypeMethod(tpl, "loadTextureFromFont", LoadTextureFromFont);

    Nan::SetPrototypeMethod(tpl, "destroy", Destroy);

    // playback
    Nan::SetPrototypeMethod(tpl, "getMediaTime", GetMediaTime);
    Nan::SetPrototypeMethod(tpl, "getDuration", GetDuration);
    Nan::SetPrototypeMethod(tpl, "getState", GetState);
    Nan::SetPrototypeMethod(tpl, "stop", StopPlayback);
    Nan::SetPrototypeMethod(tpl, "pause", PausePlayback);
    Nan::SetPrototypeMethod(tpl, "play", ResumePlayback);

    //template function
    return tpl;
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoTexture::New) {
    AminoJSObject::createInstance(info, getFactory());
}

/**
 * Init amino binding.
 */
void AminoTexture::preInit(Nan::NAN_METHOD_ARGS_TYPE info) {
    if (DEBUG_IMAGES) {
        printf("-> preInit()\n");
    }

    assert(info.Length() == 1);

    //set amino instance
    v8::Local<v8::Object> jsObj = Nan::To<v8::Object>(info[0]).ToLocalChecked();
    AminoJSEventObject *obj = Nan::ObjectWrap::Unwrap<AminoJSEventObject>(jsObj);

    assert(obj);

    //bind to queue
    setEventHandler(obj);
    Nan::Set(handle(), Nan::New("amino").ToLocalChecked(), jsObj);
}

/**
 * Get the current active texture.
 */
GLuint AminoTexture::getTexture() {
    if (activeTexture >= 0) {
        assert(textureIds);

        return textureIds[activeTexture];
    }

    return INVALID_TEXTURE;
}

/**
 * Load texture asynchronously.
 *
 * loadTextureFromImage(img, callback)
 */
NAN_METHOD(AminoTexture::LoadTextureFromImage) {
    if (DEBUG_IMAGES) {
        printf("-> loadTextureFromImage()\n");
    }

    assert(info.Length() == 2);

    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());
    v8::Local<v8::Function> callback = info[1].As<v8::Function>();

    assert(obj);

    // if (obj->callback || obj->textureCount > 0) {
    //     //already set
    //     int argc = 1;
    //     v8::Local<v8::Value> argv[1] = { Nan::Error("already loading") };

    //     callback->Call(info.This(), argc, argv);
    //     return;
    // }

    //image
    AminoImage *img = Nan::ObjectWrap::Unwrap<AminoImage>(Nan::To<v8::Object>(info[0]).ToLocalChecked());

    assert(img);

    if (!img->hasImage()) {
        //missing image
        int argc = 1;
        v8::Local<v8::Value> argv[1] = { Nan::Error("image not loaded") };

        callback->Call(info.This(), argc, argv);
        return;
    }

    if (DEBUG_BASE) {
        printf("enqueue: create texture\n");
    }

    //async loading
    obj->callback = new Nan::Callback(callback);
    obj->enqueueValueUpdate(img, static_cast<asyncValueCallback>(&AminoTexture::createTexture));
}

/**
 * Create texture from image.
 */
void AminoTexture::createTexture(AsyncValueUpdate *update, int state) {
    if (state == AsyncValueUpdate::STATE_APPLY) {
        //create texture on OpenGL thread

        if (DEBUG_IMAGES) {
            printf("-> createTexture()\n");
        }

        AminoImage *img = static_cast<AminoImage *>(update->valueObj);

        assert(img);

        bool newTexture = textureCount == 0;
        GLuint textureId = img->createTexture(getTexture());

        //debug
        //printf("-> createTexture() new=%i id=%i\n", (int)newTexture, (int)textureId);

        if (textureId != INVALID_TEXTURE) {
            //set values
            if (newTexture) {
                textureIds = new GLuint[1];
                textureIds[0] = textureId;
                textureCount = 1;
                activeTexture = 0;
                ownTexture = true;
            } else {
                //re-used texture
                assert(getTexture() == textureId);
            }

            w = img->w;
            h = img->h;

            if (newTexture) {
               (static_cast<AminoGfx *>(eventHandler))->notifyTextureCreated(1);
            }
        } else {
            activeTexture = -1;
        }
    } else if (state == AsyncValueUpdate::STATE_DELETE) {
        //on main thread

        v8::Local<v8::Object> obj = handle();

        if (activeTexture < 0) {
            //failed

            if (callback) {
                int argc = 1;
                v8::Local<v8::Value> argv[1] = { Nan::Error("could not create texture") };

                Nan::Call(*callback, obj, argc, argv);
                delete callback;
                callback = NULL;
            }

            return;
        }

        Nan::Set(obj, Nan::New("w").ToLocalChecked(), Nan::New(w));
        Nan::Set(obj, Nan::New("h").ToLocalChecked(), Nan::New(h));

        //callback
        if (callback) {
            int argc = 2;
            v8::Local<v8::Value> argv[2] = { Nan::Null(), obj };

            Nan::Call(*callback, obj, argc, argv);
            delete callback;
            callback = NULL;
        }
    }
}

/**
 * Load texture asynchronously.
 *
 * loadTextureFromVideo(video, callback)
 */
NAN_METHOD(AminoTexture::LoadTextureFromVideo) {
    if (DEBUG_IMAGES) {
        printf("-> loadTextureFromVideo()\n");
    }

    assert(info.Length() == 2);

    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());
    v8::Local<v8::Function> callback = info[1].As<v8::Function>();

    assert(obj);

    if (obj->callback || obj->textureCount > 0) {
        //already set
        int argc = 1;
        v8::Local<v8::Value> argv[1] = { Nan::Error("already loading") };

        callback->Call(info.This(), argc, argv);
        return;
    }

    //video
    AminoVideo *video = Nan::ObjectWrap::Unwrap<AminoVideo>(Nan::To<v8::Object>(info[0]).ToLocalChecked());

    assert(video);

    if (video->getPlaybackSource().empty()) {
        //missing video
        int argc = 1;
        v8::Local<v8::Value> argv[1] = { Nan::Error("missing video data") };

        callback->Call(info.This(), argc, argv);
        return;
    }

    if (obj->videoPlayer) {
        //acquire lock
        uv_mutex_lock(&obj->videoLock);

        if (obj->videoPlayer) {
            delete obj->videoPlayer;
            obj->videoPlayer = NULL;
        }

        uv_mutex_unlock(&obj->videoLock);
    }

    //create lock
    if (!obj->videoLockUsed) {
        uv_mutex_init(&obj->videoLock);
        obj->videoLockUsed = true;
    }

    //create player
    if (DEBUG_VIDEOS) {
        printf("creating video player\n");
    }

    obj->videoPlayer = (static_cast<AminoGfx *>(obj->eventHandler))->createVideoPlayer(obj, video);

    assert(obj->videoPlayer);

    if (!obj->videoPlayer->initStream()) {
        int argc = 1;
        v8::Local<v8::Value> argv[1] = { Nan::Error(obj->videoPlayer->getLastError().c_str()) };

        callback->Call(info.This(), argc, argv);

        uv_mutex_lock(&obj->videoLock);

        if (obj->videoPlayer) {
            delete obj->videoPlayer;
            obj->videoPlayer = NULL;
        }

        uv_mutex_unlock(&obj->videoLock);

        return;
    }

    if (DEBUG_BASE) {
        printf("enqueue: create video texture\n");
    }

    //async loading (OpenGL thread)
    obj->callback = new Nan::Callback(callback);

    obj->enqueueValueUpdate(video, static_cast<asyncValueCallback>(&AminoTexture::createVideoTexture));
}

/**
 * Create video texture.
 *
 * Note: on OpenGL thread.
 */
void AminoTexture::createVideoTexture(AsyncValueUpdate *update, int state) {
    if (state == AsyncValueUpdate::STATE_APPLY) {
        //create texture on OpenGL thread
        if (DEBUG_IMAGES) {
            printf("-> createVideoTexture()\n");
        }

        AminoVideo *video = static_cast<AminoVideo *>(update->valueObj);

        assert(video);

        //get amount of textures
        uv_mutex_lock(&videoLock);

        int count = 0;

        if (videoPlayer) {
            //FIXME creating too many texture on RPi (non-H264 playback case)
            count = videoPlayer->getNeededTextures();

            assert(count > 0);
        }

        uv_mutex_unlock(&videoLock);

        //create own textures
        if (!ownTexture || count > textureCount) {
            //free first
            if (ownTexture) {
                glDeleteTextures(textureCount, textureIds);
                delete[] textureIds;
                textureIds = NULL;
                textureCount = 0;
                activeTexture = -1;
            }

            //create
            textureIds = new GLuint[count];

            for (int i = 0; i < count; i++) {
                textureIds[i] = INVALID_TEXTURE;
            }

            glGenTextures(count, textureIds);

            for (int i = 0; i < count; i++) {
                assert(textureIds[i] != INVALID_TEXTURE);
            }

            textureCount = count;
            activeTexture = 0;
            ownTexture = true;

            //stats
            (static_cast<AminoGfx *>(eventHandler))->notifyTextureCreated(count);
        } else {
            //re-use textures
        }

        //debug
        if (DEBUG_VIDEOS) {
            printf("-> createVideoTexture() count=%i id=%i\n", count, textureIds[0]);
        }

        //initialize
        uv_mutex_lock(&videoLock);
        if (videoPlayer) {
            if (DEBUG_VIDEOS) {
                printf("-> init video player\n");
            }

            videoPlayer->init();
        }
        uv_mutex_unlock(&videoLock);
    } else if (state == AsyncValueUpdate::STATE_DELETE) {
        //on main thread

        if (textureCount == 0) {
            //failed

            if (callback) {
                int argc = 1;
                v8::Local<v8::Value> argv[1] = { Nan::Error("could not create texture") };

                Nan::Call(*callback, handle(), argc, argv);
                delete callback;
                callback = NULL;
            }
        }
    }
}

/**
 * Prepare OpenGL callback.
 */
void AminoTexture::initVideoTexture() {
    //enqueue
    enqueueValueUpdate(0, this, static_cast<asyncValueCallback>(&AminoTexture::initVideoTextureHandler));
}

/**
 * Initialize texture on OpenGL thread.
 */
void AminoTexture::initVideoTextureHandler(AsyncValueUpdate *update, int state) {
    if (state != AsyncValueUpdate::STATE_APPLY) {
        return;
    }

    uv_mutex_lock(&videoLock);

    if (videoPlayer) {
        videoPlayer->initVideoTexture();
    }

    uv_mutex_unlock(&videoLock);
}

/**
 * Video player init failed
 */
void AminoTexture::videoPlayerInitDone() {
    if (DEBUG_VIDEOS) {
        printf("videoPlayerInitDone()\n");
    }

    //switch to main thread
    enqueueJSCallbackUpdate(static_cast<jsUpdateCallback>(&AminoTexture::handleVideoPlayerInitDone), NULL, NULL);
}

/**
 * Inform JS callback about video state.
 */
void AminoTexture::handleVideoPlayerInitDone(JSCallbackUpdate *update) {
    if (DEBUG_VIDEOS) {
        printf("handleVideoPlayerInitDone()\n");
    }

    if (!videoPlayer) {
        return;
    }

    //create scope
    Nan::HandleScope scope;

    //check state
    if (videoPlayer->isReady()) {
        videoPlayer->getVideoDimension(w, h);

        if (DEBUG_VIDEOS) {
            printf("-> ready: %ix%i\n", w, h);
        }

        v8::Local<v8::Object> obj = handle();

        Nan::Set(obj, Nan::New("w").ToLocalChecked(), Nan::New(w));
        Nan::Set(obj, Nan::New("h").ToLocalChecked(), Nan::New(h));

        //callback
        if (callback) {
            int argc = 2;
            v8::Local<v8::Value> argv[2] = { Nan::Null(), obj };

            Nan::Call(*callback, obj, argc, argv);
            delete callback;
            callback = NULL;
        }
    } else {
        //failed
        const char *error = videoPlayer->getLastError().c_str();

        if (DEBUG_VIDEOS) {
            printf("-> error: %s\n", error);
        }

        if (callback) {
            int argc = 1;
            v8::Local<v8::Value> argv[1] = { Nan::Error(error) };

            Nan::Call(*callback, handle(), argc, argv);
            delete callback;
            callback = NULL;
        }
    }
}

/**
 * Prepare the texture (on rendering thread).
 *
 * Note: texture is valid.
 */
void AminoTexture::prepareTexture(GLContext *ctx) {
    if (!videoLockUsed) {
        return;
    }

    uv_mutex_lock(&videoLock);

    if (videoPlayer) {
        videoPlayer->updateVideoTexture(ctx);
    }

    uv_mutex_unlock(&videoLock);
}

/**
 * Fire video event.
 */
void AminoTexture::fireVideoEvent(std::string event) {
    //switch to main thread
    std::string *param = new std::string(event);

    enqueueJSCallbackUpdate(static_cast<jsUpdateCallback>(&AminoTexture::handleFireVideoEvent), NULL, param);
}

/**
 * Fire video event (on main thread).
 */
void AminoTexture::handleFireVideoEvent(JSCallbackUpdate *update) {
    std::string *event = static_cast<std::string *>(update->data);

    assert(event);

    if (DEBUG_VIDEOS) {
        printf("handleFireVideoEvent() %s\n", event->c_str());
    }

    //create scope
    Nan::HandleScope scope;

    //call
    v8::Local<v8::Function> fireEventFunc = Nan::Get(handle(), Nan::New<v8::String>("fireEvent").ToLocalChecked()).ToLocalChecked().As<v8::Function>();
    int argc = 1;
    v8::Local<v8::Value> argv[] = { Nan::New<v8::String>(event->c_str()).ToLocalChecked() };

    fireEventFunc->Call(handle(), argc, argv);

    delete event;
}

typedef struct {
    char *bufferData;
    size_t bufferLen;
    int32_t w;
    int32_t h;
    int32_t bpp;
    Nan::Callback *callback;
} amino_texture_t;

/**
 * Load texture from pixel buffer.
 */
NAN_METHOD(AminoTexture::LoadTextureFromBuffer) {
    if (DEBUG_IMAGES) {
        printf("-> loadTextureFromBuffer()\n");
    }

    assert(info.Length() == 2);

    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());

    assert(obj);
    assert(obj->ownTexture);

    //data
    v8::Local<v8::Value> data = info[0];
    v8::Local<v8::Object> dataObj = Nan::To<v8::Object>(data).ToLocalChecked();
    v8::Local<v8::Object> bufferObj = Nan::To<v8::Object>(Nan::Get(dataObj, Nan::New<v8::String>("buffer").ToLocalChecked()).ToLocalChecked()).ToLocalChecked();
    amino_texture_t *textureData = new amino_texture_t();

    textureData->bufferData = node::Buffer::Data(bufferObj);
    textureData->bufferLen = node::Buffer::Length(bufferObj);

    textureData->w = Nan::To<v8::Int32>(Nan::Get(dataObj, Nan::New<v8::String>("w").ToLocalChecked()).ToLocalChecked()).ToLocalChecked()->Value();
    textureData->h = Nan::To<v8::Int32>(Nan::Get(dataObj, Nan::New<v8::String>("h").ToLocalChecked()).ToLocalChecked()).ToLocalChecked()->Value();
    textureData->bpp = Nan::To<v8::Int32>(Nan::Get(dataObj, Nan::New<v8::String>("bpp").ToLocalChecked()).ToLocalChecked()).ToLocalChecked()->Value();

    //callback
    v8::Local<v8::Function> callback = info[1].As<v8::Function>();

    textureData->callback = new Nan::Callback(callback);

    if (DEBUG_BASE) {
        printf("enqueue: create texture from buffer\n");
    }

    //async loading
    obj->enqueueValueUpdate(data, textureData, static_cast<asyncValueCallback>(&AminoTexture::createTextureFromBuffer));
}

/**
 * Create texture.
 */
void AminoTexture::createTextureFromBuffer(AsyncValueUpdate *update, int state) {
    if (state == AsyncValueUpdate::STATE_APPLY) {
        //create texture on OpenGL thread

        if (DEBUG_IMAGES) {
            printf("-> createTextureFromBuffer()\n");
        }

        amino_texture_t *textureData = (amino_texture_t *)update->data;

        assert(textureData);

        bool newTexture = textureCount == 0;
        GLuint textureId = AminoImage::createTexture(getTexture(), textureData->bufferData, textureData->bufferLen, textureData->w, textureData->h, textureData->bpp);

        if (textureId != INVALID_TEXTURE) {
            //set values
            if (newTexture) {
                textureIds = new GLuint[1];
                textureIds[0] = textureId;
                textureCount = 1;
                activeTexture = 0;
                ownTexture = true;
            } else {
                //re-used texture
                assert(getTexture() == textureId);
            }

            w = textureData->w;
            h = textureData->h;

            if (newTexture) {
                (static_cast<AminoGfx *>(eventHandler))->notifyTextureCreated(1);
            }
        } else {
            activeTexture = -1;
        }
    } else if (state == AsyncValueUpdate::STATE_DELETE) {
        //on main thread
        amino_texture_t *textureData = (amino_texture_t *)update->data;

        assert(textureData);

        if (activeTexture < 0) {
            //failed

            if (textureData->callback) {
                int argc = 1;
                v8::Local<v8::Value> argv[1] = { Nan::Error("could not create texture") };

                Nan::Call(*textureData->callback, handle(), argc, argv);
                delete textureData->callback;
            }

            return;
        }

        v8::Local<v8::Object> obj = handle();

        Nan::Set(obj, Nan::New("w").ToLocalChecked(), Nan::New(w));
        Nan::Set(obj, Nan::New("h").ToLocalChecked(), Nan::New(h));

        //callback
        if (textureData->callback) {
            int argc = 2;
            v8::Local<v8::Value> argv[2] = { Nan::Null(), obj };

            Nan::Call(*textureData->callback, obj, argc, argv);
            delete textureData->callback;
        }

        //free
        delete textureData;
        update->data = NULL;
    }
}

/**
 * Load texture from font.
 */
NAN_METHOD(AminoTexture::LoadTextureFromFont) {
    if (DEBUG_IMAGES) {
        printf("-> loadTextureFromBuffer()\n");
    }

    assert(info.Length() == 2);

    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());

    assert(obj);

    //callback
    v8::Local<v8::Function> callback = info[1].As<v8::Function>();

    if (obj->callback || obj->textureCount > 0) {
        //already set
        int argc = 1;
        v8::Local<v8::Value> argv[1] = { Nan::Error("already loading") };

        callback->Call(info.This(), argc, argv);
        return;
    }

    //font
    AminoFontSize *fontSize = Nan::ObjectWrap::Unwrap<AminoFontSize>(Nan::To<v8::Object>(info[0]).ToLocalChecked());

    if (DEBUG_BASE) {
        printf("enqueue: create texture from font\n");
    }

    //async loading
    obj->callback = new Nan::Callback(callback);
    obj->enqueueValueUpdate(fontSize, static_cast<asyncValueCallback>(&AminoTexture::createTextureFromFont));
}

/**
 * Create texture.
 */
void AminoTexture::createTextureFromFont(AsyncValueUpdate *update, int state) {
    if (state == AsyncValueUpdate::STATE_APPLY) {
        //create texture on OpenGL thread

        if (DEBUG_IMAGES) {
            printf("-> createTextureFromFont()\n");
        }

        AminoFontSize *fontSize = static_cast<AminoFontSize *>(update->valueObj);

        assert(fontSize);
        assert(eventHandler);

        //use current font texture
        texture_atlas_t *atlas = fontSize->fontTexture->atlas;
        bool newTexture;
        GLuint textureId = (static_cast<AminoGfx *>(eventHandler))->getAtlasTexture(atlas, true, newTexture).textureId;

        if (textureId != INVALID_TEXTURE) {
            //set values
            assert(textureIds == NULL);

            textureIds = new GLuint[1];
            textureIds[0] = textureId;
            textureCount = 1;
            activeTexture = 0;
            ownTexture = false;

            w = atlas->width;
            h = atlas->height;
        } else {
            activeTexture = -1;
        }
    } else if (state == AsyncValueUpdate::STATE_DELETE) {
        //on main thread
        AminoFontSize *fontSize = static_cast<AminoFontSize *>(update->valueObj);

        assert(fontSize);

        if (activeTexture < 0) {
            //failed

            if (callback) {
                int argc = 1;
                v8::Local<v8::Value> argv[1] = { Nan::Error("could not create texture") };

                Nan::Call(*callback, handle(), argc, argv);
                delete callback;
            }

            return;
        }

        v8::Local<v8::Object> obj = handle();

        Nan::Set(obj, Nan::New("w").ToLocalChecked(), Nan::New(w));
        Nan::Set(obj, Nan::New("h").ToLocalChecked(), Nan::New(h));

        //callback
        if (callback) {
            int argc = 2;
            v8::Local<v8::Value> argv[2] = { Nan::Null(), obj };

            Nan::Call(*callback, obj, argc, argv);
            delete callback;
        }
    }
}

/**
 * Free texture.
 */
NAN_METHOD(AminoTexture::Destroy) {
    //debug
    //printf("-> destroy()\n");

    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());

    assert(obj);

    //destroy instance
    obj->destroy();
}

/**
 * Get the media time (video playback).
 */
NAN_METHOD(AminoTexture::GetMediaTime) {
    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());

    assert(obj);

    double mediaTime = -1;

    if (obj->videoPlayer) {
        mediaTime = obj->videoPlayer->getMediaTime();
    }

    info.GetReturnValue().Set(Nan::New(mediaTime));
}

/**
 * Get the duration (video playback).
 */
NAN_METHOD(AminoTexture::GetDuration) {
    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());

    assert(obj);

    double duration = -1;

    if (obj->videoPlayer) {
        duration = obj->videoPlayer->getDuration();
    }

    info.GetReturnValue().Set(Nan::New(duration));
}

/**
 * Get player state.
 */
NAN_METHOD(AminoTexture::GetState) {
    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());

    assert(obj);

    std::string state = "";

    if (obj->videoPlayer) {
        state = obj->videoPlayer->getState();
    }

    info.GetReturnValue().Set(Nan::New(state.c_str()).ToLocalChecked());
}

/**
 * Stop playback.
 */
NAN_METHOD(AminoTexture::StopPlayback) {
    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());

    assert(obj);

    if (obj->videoPlayer) {
        obj->videoPlayer->stopPlayback();
    }
}

/**
 * Pause playback.
 */
NAN_METHOD(AminoTexture::PausePlayback) {
    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());

    assert(obj);

    if (obj->videoPlayer) {
        obj->videoPlayer->pausePlayback();
    }
}

/**
 * Resume playback.
 */
NAN_METHOD(AminoTexture::ResumePlayback) {
    AminoTexture *obj = Nan::ObjectWrap::Unwrap<AminoTexture>(info.This());

    assert(obj);

    if (obj->videoPlayer) {
        obj->videoPlayer->resumePlayback();
    }
}

//
//  AminoTextureFactory
//

/**
 * Create AminoTexture factory.
 */
AminoTextureFactory::AminoTextureFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoTexture", callback) {
    //empty
}

/**
 * Create AminoTexture instance.
 */
AminoJSObject* AminoTextureFactory::create() {
    return new AminoTexture();
}
