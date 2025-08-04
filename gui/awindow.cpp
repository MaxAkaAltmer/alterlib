#include "awindow.h"

#ifdef RENDER_WITH_VULKAN
    #define GLFW_INCLUDE_NONE
    #define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#ifdef RENDER_WITH_VULKAN
    #include <imgui/backends/imgui_impl_vulkan.h>
    #ifdef IMGUI_IMPL_VULKAN_USE_VOLK
        #define VOLK_IMPLEMENTATION
        #include <volk.h>
    #endif
#else
    #include <imgui/backends/imgui_impl_opengl3.h>
#endif
#include <imgui/imgui_internal.h>

using namespace alt;
using namespace ui;

struct internalWnd
{
    GLFWwindow* hand = nullptr;
};

static int api_init_count = 0;
static bool initAPI()
{
    if(!api_init_count)
    {
        if (!glfwInit())
        {
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }

    api_init_count ++;
    return true;
}

static bool destroyAPI()
{
    if(!api_init_count)
    {
        return false;
    }
    api_init_count--;

    if(!api_init_count)
    {
        glfwTerminate();
    }

    return true;
}

window::window(string name, int w, int h, window *parent)
    : window_w(w)
    , window_h(h)
    , m_parent(parent)
    , window_name(name)
{
    if(!parent)
    {
        initAPI();
    }

    internal = new internalWnd;
    internalWnd *wnd = (internalWnd*)internal;
    GLFWwindow *owner =  parent ? ((internalWnd*)parent->internal)->hand : nullptr;

    wnd->hand = glfwCreateWindow(window_w,window_h,window_name(),nullptr,owner);

    widget::textures.init(delegate<image,string>(this,&window::loadImageCallback));
}

window::~window()
{
    resetResources();
    for(int i=0;i<layers.size();i++)
        delete layers.value(i);

    internalWnd *wnd = (internalWnd*)internal;
    glfwDestroyWindow(wnd->hand);
    delete wnd;

    if(!m_parent)
        destroyAPI();
}

void window::saveState(object state)
{
    state.setName(window_name);
    for(int i=0; i<layers.size(); i++)
    {
        layers.value(i)->saveState(state.item(layers.key(i)));
    }
}

void window::loadState(object state)
{
    for(int i=0; i<layers.size(); i++)
    {
        object *lstate = state.existedItem(layers.key(i));
        if(lstate) layers.value(i)->loadState(lstate);
    }
}

void window::resize(int w, int h)
{
    window_w = w;
    window_h = h;
    widget::window_size=vec2d<real32>(window_w,window_h);

    for(int i=0;i<layers.size();i++)
    {
        layers.value(i)->setRect(rect<real32>(0,0,w,h));
    }
}

bool window::render()
{
    internalWnd *wnd = (internalWnd*)internal;

    if(!layerStack.size()
            || !glfwWindowShouldClose(wnd->hand))return false;

    glClear(GL_COLOR_BUFFER_BIT);

    widget::textures.begin();

    glEnable(GL_SCISSOR_TEST);
    glScissor(0,0,window_w,window_h);
    layers[layerStack.last()]->render(rect<real32>(0.0,0.0,window_w,window_h),
                                       rect<real32>(0.0,0.0,window_w,window_h));
    glDisable(GL_SCISSOR_TEST);

    widget::textures.end();

    return true;
}

void window::onKey(int key, bool pressed)
{
    if(!layerStack.size())return;
    if(!pressed)return;

    array<widget*> list = layers[layerStack.last()]->focusChild();
    int ind=-1;
    for(int i=0;i<list.size();i++)
    {
        if(list[i]->focus())
        {
            ind=i;
            break;
        }
    }

    if(ind>=0) //если обрабатывает виджет - уходим
    {
        if(list[ind]->onKey(key))
        {
            if(key == widget::VKEnter) eventPress(list[ind]);
            //qDebug() << list[ind]->Name() << "onKey done";
            return;
        }
    }

    //if(ind>=0) qDebug() << list[ind]->Name() << "onKey next";

    switch(key)
    {
    case widget::VKExit:
        popLayer();
        break;
    case widget::VKEnter:
        if(ind>=0) eventPress(list[ind]);
        break;
    case widget::VKUp:
    case widget::VKLeft:
        if(ind>=0)
        {
            list[ind]->setFocus(false,key);
            ind--;
            if(ind>=0)list[ind]->setFocus(true,key);
        }
        else if(list.size())
        {
            list.last()->setFocus(true,key);
        }
        break;
    case widget::VKDown:
    case widget::VKRight:
        if(ind>=0)
        {
            list[ind]->setFocus(false,key);
            ind++;
            if(ind<list.size()) list[ind]->setFocus(true,key);
        }
        else if(list.size())
        {
            list[0]->setFocus(true,key);
        }
        break;
    };
}

void window::onPointDown(vec2d<int> p, int btn)
{
    if(!layerStack.size())return;
    array<widget*> list = layers[layerStack.last()]->focusChild();
    for(int i=0;i<list.size();i++)
    {
        if(list[i]->focus())
        {
            list[i]->setFocus(false);
            break;
        }
    }
    if(wd_on_btn.contains(btn) && wd_on_btn[btn])
    {
        widget *w = wd_on_btn[btn]->onPointUp(p.conv<real32>(),btn);
        if(w) eventPress(w);
    }
    wd_on_btn[btn] = layers[layerStack.last()]->onPointDown(p.conv<real32>(),btn);
}

void window::onWheelMove(vec2d<int> p, int value)
{
    if(!layerStack.size())return;
    layers[layerStack.last()]->onWheelMove(p.conv<real32>(),value);
}

void window::onPointUp(vec2d<int> p, int btn)
{
    if(!layerStack.size())return;
    if(wd_on_btn.contains(btn) && wd_on_btn[btn])
    {
        widget *w = wd_on_btn[btn]->onPointUp(p.conv<real32>(),btn);
        if(w) eventPress(w);
        wd_on_btn[btn]=NULL;
    }
}

void window::onPointMove(vec2d<int> p, int btn)
{
    if(!layerStack.size())return;
    if(wd_on_btn.contains(btn) && wd_on_btn[btn])
    {
        wd_on_btn[btn]->onPointMove(p.conv<real32>(),btn);
    }
}

void window::resetResources()
{
    widget::font.clear();
    widget::textures.clear();
}

image window::loadImageCallback(string id)
{
    return loadImage(id);
}
