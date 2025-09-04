#ifndef ATEXTURE_H
#define ATEXTURE_H

#include "../amath_int.h"
#include "../amath_vec.h"
#include "../acolor.h"

namespace alt {

class texture
{
public:
    texture() { hand = nullptr; }
    texture(const texture& val)
    {
        hand=val.hand;
        if(hand)hand->refcount++;
    }

    virtual ~texture()
    {
        if(!hand) return;
        hand->refcount--;

        if(!hand->refcount)
            delete hand;
    }

    virtual void resize(int w, int h, uint32 format, int filter = 0) = 0;
    virtual void update(int x, int y, const void *buff, int w, int h, uint32 format) = 0;
    virtual void free() = 0;
    virtual void draw(vec3d<real32> *vertex, vec2d<real32> *coord, int count, bool strip, colorRGBA col=colorRGBA()) const = 0;

    virtual bool isValid() const { return hand; }

    virtual void* mapData() = 0;
    virtual void unmapData() = 0;

    virtual uintz getID() const = 0;

    texture& operator=(const texture &val)
    {
        free();
        hand=val.hand;
        if(hand)hand->refcount++;
        return *this;
    }

    void drawParticle(real32 x, real32 y, real32 z, real32 zoom, colorRGBA col=colorRGBA()) const
    {
        if(!hand)return;

        vec3d<real32> vertexes[4];
        vec2d<real32> coords[4];

        real32 dx=originWidth()*0.5*zoom;
        real32 dy=originHeight()*0.5*zoom;

        vertexes[0].x=x-dx; vertexes[0].y=y-dy; vertexes[0].z=z;
        vertexes[1].x=x+dx; vertexes[1].y=y-dy; vertexes[1].z=z;
        vertexes[2].x=x-dx; vertexes[2].y=y+dy; vertexes[2].z=z;
        vertexes[3].x=x+dx; vertexes[3].y=y+dy; vertexes[3].z=z;

        coords[0].x=0.0; coords[0].y=0.0;
        coords[1].x=normalWidth(); coords[1].y=0.0;
        coords[2].x=0.0; coords[2].y=normalHeight();
        coords[3].x=normalWidth(); coords[3].y=normalHeight();

        draw(vertexes,coords,4,true,col);
    }

    void drawRect(real32 x, real32 y, real32 w, real32 h, colorRGBA col=colorRGBA()) const
    {
        if(!hand)return;
        vec3d<real32> vertexes[4];
        vec2d<real32> coords[4];

        vertexes[0].x=x; vertexes[0].y=y; vertexes[0].z=0.0;
        vertexes[1].x=x+w; vertexes[1].y=y; vertexes[1].z=0.0;
        vertexes[2].x=x; vertexes[2].y=y+h; vertexes[2].z=0.0;
        vertexes[3].x=x+w; vertexes[3].y=y+h; vertexes[3].z=0.0;

        coords[0].x=0.0; coords[0].y=0.0;
        coords[1].x=normalWidth(); coords[1].y=0.0;
        coords[2].x=0.0; coords[2].y=normalHeight();
        coords[3].x=normalWidth(); coords[3].y=normalHeight();

        draw(vertexes,coords,4,true,col);
    }
    void drawRectPart(alt::rect<real32> scr_part, alt::rect<real32> tex_part, colorRGBA col=colorRGBA()) const
    {
        if(!hand)return;
        vec3d<real32> vertexes[4];
        vec2d<real32> coords[4];

        vertexes[0].x=scr_part.tl().x; vertexes[0].y=scr_part.tl().y; vertexes[0].z=0.0;
        vertexes[1].x=scr_part.dr().x; vertexes[1].y=scr_part.tl().y; vertexes[1].z=0.0;
        vertexes[2].x=scr_part.tl().x; vertexes[2].y=scr_part.dr().y; vertexes[2].z=0.0;
        vertexes[3].x=scr_part.dr().x; vertexes[3].y=scr_part.dr().y; vertexes[3].z=0.0;

        coords[0].x=convX(tex_part.tl().x); coords[0].y=convY(tex_part.tl().y);
        coords[1].x=convX(tex_part.dr().x); coords[1].y=convY(tex_part.tl().y);
        coords[2].x=convX(tex_part.tl().x); coords[2].y=convY(tex_part.dr().y);
        coords[3].x=convX(tex_part.dr().x); coords[3].y=convY(tex_part.dr().y);

        draw(vertexes,coords,4,true,col);
    }
    void drawInside(int w, int h, colorRGBA col= colorRGBA()) //вписывает в экран (w,h) текстуру
    {
        if(!hand)return;

        real32 asp=(real32)w/(real32)hand->width;
        real32 aspy=(real32)h/(real32)hand->height;
        if(aspy>asp)asp=aspy;

        real32 dx=(w-asp*hand->width)/2.0;
        real32 dy=(h-asp*hand->height)/2.0;

        vec3d<real32> vertexes[4];
        vec2d<real32> coords[4];

        vertexes[0].x=dx; vertexes[0].y=dy; vertexes[0].z=0.0;
        vertexes[1].x=-dx+w; vertexes[1].y=dy; vertexes[1].z=0.0;
        vertexes[2].x=dx; vertexes[2].y=-dy+h; vertexes[2].z=0.0;
        vertexes[3].x=-dx+w; vertexes[3].y=-dy+h; vertexes[3].z=0.0;

        coords[0].x=0.0; coords[0].y=0.0;
        coords[1].x=normalWidth(); coords[1].y=0.0;
        coords[2].x=0.0; coords[2].y=normalHeight();
        coords[3].x=normalWidth(); coords[3].y=normalHeight();

        draw(vertexes,coords,4,true,col);
    }
    void drawMirrorBox(alt::rect<real32> box, real32 border, colorRGBA col=colorRGBA())
    {
        if(!hand)return;
        if(border>((box.width-1.0)/2.0)) border=(box.width-1.0)/2.0;
        if(border>((box.height-1.0)/2.0)) border=(box.height-1.0)/2.0;

        vec3d<real32> vertexes[9*6];
        vec2d<real32> coords[9*6];

        alt::rect<real32> trc=normalRect();

        makeTriangles(vertexes,coords,
                          alt::rect<real32>(box.x,box.y,border,border),
                          alt::rect<real32>(trc.x,trc.y,trc.width,trc.height));

        makeTriangles(vertexes+6,coords+6,
                          alt::rect<real32>(box.x+border,box.y,box.width-2*border,border),
                          alt::rect<real32>(trc.width-convX(1),trc.y,convX(1),trc.height));

        makeTriangles(vertexes+2*6,coords+2*6,
                          alt::rect<real32>(box.x+box.width-border,box.y,border,border),
                          alt::rect<real32>(trc.x,trc.y,trc.width,trc.height),true);

        makeTriangles(vertexes+3*6,coords+3*6,
                          alt::rect<real32>(box.x,box.y+border,border,box.height-2*border),
                          alt::rect<real32>(trc.x,trc.height-convY(1),trc.width,convY(1)));

        makeTriangles(vertexes+4*6,coords+4*6,
                          alt::rect<real32>(box.x+border,box.y+border,box.width-2*border,box.height-2*border),
                          alt::rect<real32>(trc.width-convX(1),trc.height-convY(1),convX(1),convY(1)));

        makeTriangles(vertexes+5*6,coords+5*6,
                          alt::rect<real32>(box.x+box.width-border,box.y+border,border,box.height-2*border),
                          alt::rect<real32>(trc.x,trc.height-convY(1),trc.width,convY(1)),true);

        makeTriangles(vertexes+6*6,coords+6*6,
                          alt::rect<real32>(box.x,box.y+box.height-border,border,border),
                          alt::rect<real32>(trc.x,trc.y,trc.width,trc.height),false,true);

        makeTriangles(vertexes+7*6,coords+7*6,
                          alt::rect<real32>(box.x+border,box.y+box.height-border,box.width-2*border,border),
                          alt::rect<real32>(trc.width-convX(1),trc.y,convX(1),trc.height),false,true);

        makeTriangles(vertexes+8*6,coords+8*6,
                          alt::rect<real32>(box.x+box.width-border,box.y+box.height-border,border,border),
                          alt::rect<real32>(trc.x,trc.y,trc.width,trc.height),true,true);

        draw(vertexes,coords,9*6,false,col);
    }

    void drawRectPart(alt::rect<real32> scr_part, alt::rect<int32> tex_part, colorRGBA col=colorRGBA()) const
    {
        if(!hand)return;
        alt::rect<real32> rc(convX(tex_part.x),convY(tex_part.y),convX(tex_part.width),convY(tex_part.height));
        drawRectPart(scr_part,rc,col);
    }

    real32 normalWidth() const
    {
        if(!hand) return 0.0;
        return (real32)originWidth()/(real32)hand->alwidth;
    }
    real32 normalHeight() const
    {
        if(!hand) return 0.0;
        return (real32)originHeight()/(real32)hand->alheight;
    }
    int alignedWidth() const
    {
        if(!hand) return 0;
        return hand->alwidth;
    }
    int alignedHeight() const
    {
        if(!hand) return 0;
        return hand->alheight;
    }
    int originWidth() const
    {
        if(!hand) return 0;
        return hand->width;
    }
    int originHeight() const
    {
        if(!hand) return 0;
        return hand->height;
    }
    real32 convX(int32 x) const
    {
        if(!hand) return 0.0;
        return (real32)x/(real32)hand->alwidth;
    }
    real32 convY(int32 y) const
    {
        if(!hand) return 0.0;
        return (real32)y/(real32)hand->alheight;
    }

    alt::rect<int> originRect()
    {
        if(!hand) return alt::rect<int>();
        return alt::rect<int>(0,0,hand->width,hand->height);
    }
    alt::rect<real32> normalRect()
    {
        if(!hand) return alt::rect<real32>();
        return alt::rect<int>(0,0,normalWidth(),normalHeight());
    }

    static int pixelSize(uint32 format)
    {
        if((format&0xf) == 0xf)
        { //float pixel format
            return (format>>4)*4;
        }
        int rv=(format>>16)&0xff;
        if(!rv)rv=(format&0xf)+(((format>>4)&0xf))
                +(((format>>8)&0xf))+(((format>>12)&0xf));
        return (rv+7)>>3;
    }
    static bool hasAlpha(uint32 format)
    {
        if(format == 0x4f) return true;
        if(((format>>16)&0xff)==32)return true;
        if(format&0xf000)return true;
        return false;
    }

protected:

    struct Internal
    {
        uint refcount;

        int width, height;
        int alwidth, alheight;
        uint32 pixelFormat;

        void *api_related = nullptr;
    };

    Internal *hand = nullptr;

    void makeTriangles(vec3d<real32> *vertexes, vec2d<real32> *coords, alt::rect<real32> scr, alt::rect<real32> tex,
                           bool flipH=false, bool flipV=false)
    {
        vertexes[0].x=scr.tl().x; vertexes[0].y=scr.tl().y; vertexes[0].z=0.0;
        vertexes[1].x=scr.tr().x; vertexes[1].y=scr.tr().y; vertexes[1].z=0.0;
        vertexes[2].x=scr.dl().x; vertexes[2].y=scr.dl().y; vertexes[2].z=0.0;

        vertexes[3].x=scr.tr().x; vertexes[3].y=scr.tr().y; vertexes[3].z=0.0;
        vertexes[4].x=scr.dr().x; vertexes[4].y=scr.dr().y; vertexes[4].z=0.0;
        vertexes[5].x=scr.dl().x; vertexes[5].y=scr.dl().y; vertexes[5].z=0.0;

        coords[0].x=flipH?tex.tr().x:tex.tl().x; coords[0].y=flipV?tex.dl().y:tex.tl().y;
        coords[1].x=flipH?tex.tl().x:tex.tr().x; coords[1].y=flipV?tex.dr().y:tex.tr().y;
        coords[2].x=flipH?tex.dr().x:tex.dl().x; coords[2].y=flipV?tex.tl().y:tex.dl().y;

        coords[3].x=flipH?tex.tl().x:tex.tr().x; coords[3].y=flipV?tex.dr().y:tex.tr().y;
        coords[4].x=flipH?tex.dl().x:tex.dr().x; coords[4].y=flipV?tex.tr().y:tex.dr().y;
        coords[5].x=flipH?tex.dr().x:tex.dl().x; coords[5].y=flipV?tex.tl().y:tex.dl().y;
    }

};

} //namespace alt

#endif // ATEXTURE_H
