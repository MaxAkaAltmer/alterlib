#include "awidget.h"

using namespace alt;
using namespace ui;

AGLFont widget::font;
AWidgetTextures widget::textures;
vec2d<real32> widget::window_size;

real32 widget::std_font_size_title = 24.0;
real32 widget::std_font_size_text = 18.0;
real32 widget::std_font_size_comment = 12.0;
colorRGBA widget::std_font_color_main = 0xffdddddd;
colorRGBA widget::std_font_color_header = 0xffff7070;
colorRGBA widget::std_font_color_subheader = 0xffddb0b0;
colorRGBA widget::std_font_color_warning = 0xff7070ff;
colorRGBA widget::std_color_bgdecor = colorRGBA(0xff302F2F).scale(0.8);
colorRGBA widget::std_color_background = 0xff302F2F;
colorRGBA widget::std_color_button_off = 0xff4A4949;
colorRGBA widget::std_color_button_on = colorRGBA(0xff4A4949).scale(1.5);
vec2d<real32> widget::std_size_icon = vec2d<real32>(24,24);
real32 widget::std_margin = 5.0;

real32 widget::global_zoom = 1.0;

widget::widget(string name, widget *parent)
    : wdname(name),
      parent_point(parent)
{
    focus_status = -1;
    margin = std_margin;
    is_disabled = false;

    if(parent)
    {
        parent->chilren_list.append(this);
    }
}

void widget::setParent(widget *parent)
{
    if(parent_point!=parent)
    {
        if(parent_point) parent_point->chilren_list.removeValue(this);
        parent_point=parent;
        parent_point->chilren_list.append(this);
    }
}

widget::~widget()
{
    for(int i=0; i<chilren_list.size(); i++)
    {
        delete chilren_list[i];
    }
}

real32 widget::evalHeight(real32 forWidth)
{
    return (std_size_icon.y+std_margin*2.0)*global_zoom;
}

real32 widget::evalOptimalWidth()
{
    return (std_size_icon.y+std_margin*2.0)*global_zoom;
}

widget* widget::operator[](string child)
{
    string next,curr;

    if(child.isEmpty())return this;

    int ind=child.indexOf('/',0);
    if(ind<0)
    {
        curr=child;
    }
    else
    {
        curr=child.left(ind);
        next=child.right(ind+1);
    }

    for(int i=0; i<chilren_list.size(); i++)
    {
        if(chilren_list[i]->wdname==curr)
            return (*chilren_list[i])[next];
    }

    return 0;
}

array<widget *> widget::focusChild()
{
    array<widget*> rv;
    if(is_disabled)return rv;
    for(int i=0;i<chilren_list.size();i++)
        rv.append(chilren_list[i]->focusChild());
    if(focus_status>=0)rv.append(this);
    return rv;
}

void widget::setFocus(bool en, int key)
{
    if(focus_status>=0)focus_status=en?1:0;
    if(focus_status>0) onFocusChanged(this);
}

string widget::PathToMe()
{
    string ppath;
    if(parent_point) ppath=parent_point->PathToMe();
    else return string();

    if(ppath.isEmpty())return wdname;
    return ppath+"/"+wdname;
}

widget* widget::onKey(int key)
{
    if(is_disabled)return NULL;
    return onKeyParentFilter(this,key);
}

widget* widget::onKeyParentFilter(widget *child, int key)
{
    if(parent_point)return parent_point->onKeyParentFilter(child,key);
    return NULL;
}

widget* widget::onPressParentFilter(widget *child)
{
    if(parent_point)return parent_point->onPressParentFilter(child);
    return NULL;
}

widget* widget::onFocusChanged(widget *child)
{
    if(parent_point)return parent_point->onFocusChanged(this);
    return NULL;
}

void widget::onScrollDeltaFilter(vec2d<real32> delta)
{
    if(parent_point)return parent_point->onScrollDeltaFilter(delta);
}

widget* widget::onPointDown(vec2d<real32> p, int btn)
{
    if(is_disabled)return NULL;
    for(int i=0;i<chilren_list.size();i++)
    {
        if(!chilren_list[i]->is_disabled && chilren_list[i]->rect().test(p))
        {
            return chilren_list[i]->onPointDown(p-chilren_list[i]->rect().tl(),btn);
        }
    }
    return NULL;
}

widget* widget::onPointUp(vec2d<real32> scr_pos, int btn)
{    
    if(is_disabled)return NULL;
    if(parent_point) return parent_point->onPointUp(scr_pos,btn);
    return NULL;
}

widget* widget::onPointMove(vec2d<real32> scr_pos, int btn)
{
    if(is_disabled)return NULL;
    if(parent_point) return parent_point->onPointMove(scr_pos,btn);
    return NULL;
}

widget* widget::onWheelMove(vec2d<real32> p, int value)
{
    if(is_disabled)return NULL;
    for(int i=0;i<chilren_list.size();i++)
    {
        if(!chilren_list[i]->is_disabled && chilren_list[i]->rect().test(p))
        {
            return chilren_list[i]->onWheelMove(p-chilren_list[i]->rect().tl(),value);
        }
    }
    return NULL;
}

vec2d<real32> widget::screen2local(vec2d<real32> p)
{
    if(parent_point) p = parent_point->screen2local(p);
    return p-rectangle.tl();
}

vec2d<real32> widget::local2screen(vec2d<real32> p)
{
    p+=rectangle.tl();
    if(parent_point) p = parent_point->local2screen(p);
    return p;
}

void widget::render(alt::rect<real32> relativ, alt::rect<real32> screen)
{
    if(!relativ.test(rectangle))return;
    alt::rect<real32> rlseg=relativ.crossResult(rectangle);
    alt::rect<real32> scseg=screen.affine(relativ,rlseg);

    for(int i=0; i<chilren_list.size(); i++)
    {
        glScissor(scseg.x,window_size.y-scseg.y-scseg.height,scseg.width,scseg.height);
        chilren_list[i]->render(rlseg-rectangle.tl(),scseg);
    }
}

void widget::saveState(object *obj)
{
    for(int i=0;i<user_property.size();i++)
    {
        obj->setAttr("_"+user_property.key(i),user_property.value(i));
    }
    for(int i=0; i<chilren_list.size(); i++)
    {
        chilren_list[i]->saveState(obj->item(chilren_list[i]->Name()));
    }
}

void widget::loadState(object *obj)
{
    array<string> list = obj->listAttributes();
    for(int i=0;i<list.size();i++)
    {
        if(list[i].indexOf("_")==0)
            user_property.insert(list[i].right(1),obj->attr(list[i]));
    }
    for(int i=0; i<chilren_list.size(); i++)
    {
        object *chobj = obj->existedItem(chilren_list[i]->Name());
        if(chobj) chilren_list[i]->loadState(chobj);
    }
}
