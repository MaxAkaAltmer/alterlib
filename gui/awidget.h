#ifndef AWIDGET_H
#define AWIDGET_H

#include "../amath_vec.h"
#include "../avariant.h"
#include "../aimage.h"
#include "../acolor.h"
#include "../at_priority_cache.h"
#include "agl_texture.h"
#include "agl_primitive.h"
#include "agl_font.h"
#include "../aobject.h"
#include "../adelegate.h"

namespace alt
{
namespace ui
{

    class AWidgetTextures
    {
    public:
        AWidgetTextures()
        {
            textures_used_at_draw_maximum = 0;
        }
        ~AWidgetTextures()
        {
            clear();
        }

        void init(delegate<image,string> loader)
        {
            loadImage = loader;
        }

        void begin()
        {
            textures_used_at_draw.clear();
        }
        void end()
        {
            if(textures_used_at_draw.size()>textures_used_at_draw_maximum)
                textures_used_at_draw_maximum=textures_used_at_draw.size();

            if(texcache.Size()>(textures_used_at_draw_maximum<<1))
            {
                int torem=texcache.Size()-(textures_used_at_draw_maximum<<1);
                while(torem--)
                {
                    string tex=texcache.ForRemove();
                    texcache.Delete(texcache.ForRemoveIndex());
                    delete textures[tex].left();
                    textures.remove(tex);
                }
            }
        }

        void clear()
        {
            texcache.Clear();
            for(int i=0;i<textures.size();i++)
            {
                delete textures.value(i).left();
            }
            textures.clear();
        }

        AGLTexture* operator()(string id)
        {
            textures_used_at_draw.insert(id);
            if(!textures.contains(id))
            {
                image img=loadImage(id);
                AGLTexture *tmp = new AGLTexture(img(),img.width(),img.height(),0x8888);
                textures.insert(id,pair<AGLTexture*,int>(tmp,texcache.Push(id)));
            }
            pair<AGLTexture*,int> tex=textures[id];
            texcache.Update(tex.right());
            return tex.left();
        }

    private:
        //процедура загрузки из ресурсов
        delegate<image,string> loadImage;

        //кеширование текстур
        set<string>  textures_used_at_draw;
        int textures_used_at_draw_maximum;
        cache<string> texcache;
        hash<string,pair<AGLTexture*,int> > textures;
    };

    class widget: public delegateBase
    {
    public:
        widget(string name, widget *parent);
        virtual ~widget();

        //извлечение указателей на дочерние элементы
        widget* operator[](string child);
        widget* operator[](int child)
        {
            if(child>chilren_list.size() || child<0)return NULL;
            return chilren_list[child];
        }
        string Name(){return wdname;}
        int childCount(){return chilren_list.size();}
        string PathToMe();

        //клавишное управление интерфейсом
        enum VKeyCodes
        {
            VKNone,
            VKUp,
            VKDown,
            VKLeft,
            VKRight,
            VKEnter,
            VKExit
        };

        virtual widget *onKey(int key);
        //события от нажатий на экран, p - экранные координаты, btn - номер кнопки/пальца
        virtual widget *onPointDown(vec2d<real32> p, int btn);
        virtual widget *onPointUp(vec2d<real32> scr_pos, int btn);
        virtual widget *onPointMove(vec2d<real32> scr_pos, int btn);
        virtual widget *onWheelMove(vec2d<real32> p, int value);
        //событие отрисовки, в нем же происходит зачистка уничтоженных виджетов
        virtual void render(rect<real32> relativ, rect<real32> screen);
        virtual real32 evalHeight(real32 forWidth);
        virtual real32 evalOptimalWidth();

        //настройки элемента

        enum Alignment
        {
            ALIGN_CENTER,
            ALIGN_JUSTIFY,
            ALIGN_LEFT,
            ALIGN_RIGHT
        };

        virtual widget& setRect(alt::rect<real32> rc){rectangle=rc; return *this;}
        alt::rect<real32> rect(){return rectangle;}

        widget& setMargin(real32 m)
        {
            margin=m;
            return *this;
        }
        widget& setEnabled(bool en)
        {
            is_disabled = !en;
            return *this;
        }

        widget& setProperty(string name, variant val)
        {
            user_property[name]=val;
            return *this;
        }
        bool hasProperty(string name){return user_property.contains(name);}
        variant property(string name)
        {
            if(user_property.contains(name))return user_property[name];
            return variant();
        }

        //управление фокусом
        virtual array<widget*> focusChild();
        virtual void setFocus(bool en, int key = VKNone);
        bool focus()
        {
            return focus_status>0?true:false;
        }
        void setFocusable(bool en)
        {
            focus_status=en?0:-1;
        }

        //общие переменные
        static AGLFont font;
        static AWidgetTextures textures;
        static vec2d<real32> window_size;

        static real32 std_font_size_title, std_font_size_text;
        static real32 std_font_size_comment;
        static colorRGBA std_font_color_main, std_font_color_header, std_font_color_subheader;
        static colorRGBA std_font_color_warning;
        static colorRGBA std_color_background;
        static colorRGBA std_color_bgdecor;
        static colorRGBA std_color_button_off;
        static colorRGBA std_color_button_on;
        static vec2d<real32> std_size_icon;
        static real32 std_margin;

        static real32 global_zoom;

        //хранение и загрузка состояния
        virtual void saveState(object *obj);
        virtual void loadState(object *obj);

        void setParent(widget *parent);

    protected:

        vec2d<real32> screen2local(vec2d<real32> p);
        vec2d<real32> local2screen(vec2d<real32> p);

        virtual widget *onKeyParentFilter(widget *child, int key);
        virtual widget *onPressParentFilter(widget *child);
        virtual widget *onFocusChanged(widget *child); //todo: передавать ARect

        virtual void onScrollDeltaFilter(vec2d<real32> delta);

        //базовые атрибуты
        int focus_status;
        bool is_disabled;

        //обязательные данные
        string wdname;
        widget *parent_point;
        array<widget*> chilren_list;
        alt::rect<real32> rectangle;
        hash<string,variant> user_property;
        real32 margin;

    };

}}

#endif // AWIDGET_H
