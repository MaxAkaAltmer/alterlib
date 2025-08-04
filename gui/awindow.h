#ifndef AWINDOW_H
#define AWINDOW_H

#include "../aobject.h"
#include "../adelegate.h"
#include "awidget.h"

namespace alt
{
namespace ui
{

    class window: public delegateBase
    {
    public:
        //конструкторы и деструкторы
        window(string name, int w, int h, window *parent = nullptr);
        virtual ~window();

        //проверка на выход из системы виджетов
        bool isTerminated(){return !layerStack.size();}

        //управление состоянием интерфейса
        void saveState(object state);
        void loadState(object state);

        //общий ресайз
        virtual void resize(int w, int h);
        //очистка ресурсов в случае разрушения контекста
        virtual void resetResources();

        //событие отрисовки, в нем же происходит зачистка уничтоженных виджетов
        bool render();
        //клавишное управление интерфейсом
        void onKey(int key, bool pressed);
        //события от нажатий на экран, p - экранные координаты, btn - номер кнопки/пальца
        void onPointDown(vec2d<int> p, int btn);
        void onPointUp(vec2d<int> p, int btn);
        void onPointMove(vec2d<int> p, int btn);
        void onWheelMove(vec2d<int> p, int value);

        //к предыдущему слою, вернет false, если текущий слой последний
        void popLayer(){layerStack.pop();}
        //переходим с текущего слоя к указанному
        void setLayer(string name)
        {
            if(!layers.contains(name))return;
            if(layerStack.size())layerStack.last()=name;
            else layerStack.append(name);
        }
        //помещаем текущий слой в стэк и отображаем указанный
        void pushLayer(string name)
        {
            if(!layers.contains(name))return;
            layerStack.append(name);
        }

    protected:

        //текущие параметры окна
        real32 window_w, window_h;

        //функции которые должен реализовать бэкэнд
        virtual image loadImage(string id) = 0;
        //todo: аналогично сделать для загрузки шрифтов

        //событие нажатия
        virtual void eventPress(widget *wd) = 0;

        //работа со слоями
        void addLayer(widget *lw)
        {
            layers.insert(lw->Name(),lw);
            if(!layerStack.size())layerStack.append(lw->Name());
        }

    private:

        window *m_parent = nullptr;

        void *internal = nullptr;

        string window_name;
        hash<string,widget*> layers;
        array<string> layerStack;
        image loadImageCallback(string id);

        hash<int,widget*> wd_on_btn;

    };

}}

#endif // AWINDOW_H
