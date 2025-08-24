#ifndef AVKTEXTURE_H
#define AVKTEXTURE_H

#include "avk_context.h"

#include "atexture.h"

namespace alt {

class VKTexture: public alt::texture
{
public:
    VKTexture(): alt::texture() {}
    VKTexture(const texture& val): alt::texture(val) {}
    VKTexture(const void *buff, int w, int h, uint32 format, int filter = 0)
        : alt::texture()
    {
        resize(w,h,format,filter);
        update(0,0,buff, w, h, format);
    }
    VKTexture(int w, int h, uint32 format, int filter = 0)
        : alt::texture()
    {
        resize(w,h,format,filter);
    }
#ifdef QT_WIDGETS_LIB
    VKTexture(const QImage& img)
        : alt::texture()
    {
        resize(img.width(),img.height(),img.depth()<<16);
        update(0,0,img.bits(),img.height(),img.depth()<<16);
    }
#endif

    ~VKTexture() { VKTexture::free(); }

    void resize(int w, int h, uint32 format, int filter = 0) final;
    void update(int x, int y, const void *buff, int w, int h, uint32 format) final;
    void free() final;
    void draw(vec3d<real32> *vertex, vec2d<real32> *coord, int count, bool strip, alt::colorRGBA col=alt::colorRGBA()) const final;

    uintz getID() const final
    {
        if(!hand || !hand->api_related) return 0;
        return ptr2int(((VKApiResInternal*)(hand->api_related))->m_DescriptorSet);
    }

    bool isValid() const final
    {
        if(!hand || !hand->api_related) return false;
        return true;
    }

    static void initCtx(VKContext *ctx_ptr)
    {
        ctx = ctx_ptr;
    }

    void* mapData() final;
    void unmapData() final;

private:

    static thread_local VKContext *ctx;

};

} //namespace alt

#endif // AVKTEXTURE_H
