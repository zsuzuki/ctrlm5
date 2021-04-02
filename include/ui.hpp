///
/// Copyright Y.Suzuki 2021
/// wave.suzuki.z@gmail.com
///
#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>

namespace UI
{
    ///
    /// 文字列
    ///
    int utf8bc(char ch)
    {
        if (0 <= uint8_t(ch) && uint8_t(ch) < 0x80)
            return 1;
        if (0xc2 <= uint8_t(ch) && uint8_t(ch) < 0xe0)
            return 2;
        if (0xe0 <= uint8_t(ch) && uint8_t(ch) < 0xf0)
            return 3;
        if (0xf0 <= uint8_t(ch) && uint8_t(ch) < 0xf8)
            return 4;
        return 0;
    }
    int utf8len(const char *str)
    {
        int len = 0;
        while (char ch = *str)
        {
            int cnt = utf8bc(ch);
            str += cnt;
            len += cnt > 1 ? 2 : 1;
        }
        return len;
    }

    ///
    /// コンテキスト
    ///
    class Context
    {
        int bbLeft = 320;
        int bbRight = 0;
        int bbTop = 240;
        int bbBottom = 0;
        bool bbEnabled = false;

        void resetBB()
        {
            bbLeft = 320;
            bbRight = 0;
            bbTop = 240;
            bbBottom = 0;
            bbEnabled = false;
        }

    public:
        bool drawRequest = false;
        int fontWidth = 12;
        int fontHeight = 24;

        void setBoundingBox(int x, int y, int w, int h)
        {
            if (x < bbLeft)
                bbLeft = x;
            if (x + w > bbRight)
                bbRight = x + w;
            if (y < bbTop)
                bbTop = y;
            if (y + h > bbBottom)
                bbBottom = y + h;
            bbEnabled = true;
        }
        void clear(LGFX *gfx)
        {
            if (bbEnabled)
            {
                gfx->fillRect(bbLeft, bbTop, bbRight - bbLeft, bbBottom - bbTop, TFT_BLACK);
                resetBB();
            }
        }
    };

    //
    // UI:ウィジェット基礎
    //
    class Widget
    {
    private:
        friend class Control;
        friend class Layer;
        bool needUpdate = false;
        bool focused = false;
        Widget *focusNext = nullptr;
        Widget *focusPrev = nullptr;

    protected:
        Context *context = nullptr;
        LGFX *gfx = nullptr;

        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;

        void initialize(Context *ctx, LGFX *g)
        {
            context = ctx;
            gfx = g;
        }

        void update(bool nu = true)
        {
            if (nu)
            {
                needUpdate = true;
                context->drawRequest = true;
            }
        }
        bool checkUpdate()
        {
            bool nu = needUpdate;
            needUpdate = false;
            return nu;
        }
        virtual bool hitCheck(int tx, int ty)
        {
            return x < tx && tx < (x + w) && y < ty && ty < (y + h);
        }

        //
        virtual void draw() {}
        void drawBase(bool forceDraw)
        {
            if (checkUpdate() || forceDraw)
            {
                draw();
                context->setBoundingBox(x, y, w, h);
            }
        }
        //
        virtual void onPressed(int, int) {}

        //
        void linkNext(Widget *w)
        {
            focusNext = w;
            if (w)
                w->focusPrev = this;
        }
        void linkPrev(Widget *w)
        {
            focusPrev = w;
            if (w)
                w->focusNext = this;
        }

    public:
        virtual ~Widget() = default;

        //
        virtual void setCaption(String c) {}

        //
        int getWidth() const { return w; }
        int getHeight() const { return h; }

        //
        void setGeometory(int gx, int gy, int gw = 0, int gh = 0)
        {
            x = gx;
            y = gy;
            if (gw != 0)
                w = gw;
            if (gh != 0)
                h = gh;
            update();
        }

        //
        bool isFocused() const { return focused; }
    };

    ///
    /// レイヤー
    ///
    struct Layer
    {
        Widget *currentFocus = nullptr;
        Widget *drawSet = nullptr;
        Widget *lastWidget = nullptr;
        //
        void appendWidget(Widget *w)
        {
            if (!currentFocus)
                setFocus(w);
            if (lastWidget)
                lastWidget->linkNext(w);
            lastWidget = w;
            if (!drawSet)
                drawSet = w;
        }
        ///
        void setFocus(Widget *w, bool f = true)
        {
            if (f && currentFocus == w)
                return;

            w->update(w->focused != f);
            if (f)
            {
                if (currentFocus)
                {
                    currentFocus->focused = false;
                    currentFocus->update();
                }
                currentFocus = w;
            }
            else if (currentFocus == w)
                currentFocus = nullptr;
            w->focused = f;
        }
        void nextFocus(Widget *w)
        {
            if (!w->focused || w->focusNext == nullptr)
                return;
            w->focused = false;
            setFocus(w->focusNext, true);
            w->update();
        }
        void prevFocus(Widget *w)
        {
            if (!w->focused || w->focusPrev == nullptr)
                return;
            w->focused = false;
            setFocus(w->focusPrev, true);
            w->update();
        }
    };

    ///
    /// コントローラ
    ///
    class Control
    {
        Context context;
        LGFX *gfx = nullptr;
        Layer layerPool[10]{};
        int layerIndex = 0;
        Layer *layer = nullptr;
        bool requestLayer = false;

    public:
        void init(LGFX *g)
        {
            gfx = g;
            setLayer(0);
        }
        ///
        void setLayer(int idx)
        {
            requestLayer = true;
            layerIndex = idx;
            layer = &layerPool[layerIndex];
        }
        int getLayer() const { return layerIndex; }
        ///
        bool needDraw() const { return context.drawRequest; }
        ///
        Widget *getCurrentFocus()
        {
            return layer->currentFocus;
        }
        ///
        void appendWidget(Widget *w)
        {
            w->initialize(&context, gfx);
            layer->appendWidget(w);
        }
        ///
        void setFocus(Widget *w, bool f = true)
        {
            layer->setFocus(w, f);
        }
        void nextFocus(Widget *w)
        {
            layer->nextFocus(w);
        }
        void prevFocus(Widget *w)
        {
            layer->prevFocus(w);
        }
        ///
        void drawWidgets()
        {
            if (requestLayer)
                context.clear(gfx);

            auto *dset = layer->drawSet;
            auto *next = dset;
            for (auto *w = next; w; w = next)
            {
                w->drawBase(requestLayer);
                next = w->focusNext;
                if (next == dset)
                    break;
            }
            context.drawRequest = false;
            requestLayer = false;
        }
        ///
        void touchCheck(int tx, int ty, bool first)
        {
            auto *dset = layer->drawSet;
            auto *next = dset;
            ty -= 20;
            for (auto *w = next; w; w = next)
            {
                if (w->hitCheck(tx, ty))
                {
                    if (first && w->isFocused())
                        w->onPressed(tx - w->x, ty - w->y);
                    else
                        setFocus(w);
                    break;
                }
                next = w->focusNext;
                if (next == dset)
                    break;
            }
        }
    };

    //
    // UIパーツ実装
    //

    using PressFunction = void (*)(Widget *);
    using SelectFunction = void (*)(int, const char *);

    //
    // UI:テキストボタン
    //
    class TextButton : public Widget
    {
        static constexpr int mX = 12; // margin X
        static constexpr int mY = 10; // margin Y
        static constexpr int rd = 8;  // round size

        String caption{};
        int len = 0;

        PressFunction pressFunc = nullptr;

        //
        void
        draw() override
        {
            if (isFocused())
            {
                gfx->setTextColor(TFT_WHITE);
                gfx->fillRoundRect(x, y, w, h, rd, TFT_BLUE);
                gfx->drawString(caption, x + mX, y + mY);
            }
            else
            {
                gfx->setTextColor(TFT_BLACK);
                gfx->fillRoundRect(x, y, w, h, rd, TFT_WHITE);
                gfx->drawString(caption, x + mX, y + mY);
                gfx->drawRoundRect(x, y, w, h, rd, TFT_BLUE);
            }
        }
        //
        void onPressed(int, int) override
        {
            if (pressFunc)
                pressFunc(this);
        }

    public:
        ~TextButton() = default;

        //
        void setCaption(String c) override
        {
            caption = c;
            len = utf8len(c.c_str());

            int width = len * context->fontWidth + mX * 2;
            int height = context->fontHeight + mY * 2;
            if (width > w)
                w = width;
            if (height > h)
                h = height;

            update();
        }

        //
        void setPressFunction(PressFunction pf)
        {
            pressFunc = pf;
        }
    };

    //
    // UI:チェックボックス
    //
    class CheckBox : public Widget
    {
        static constexpr int mX = 10; // margin X
        static constexpr int mY = 10; // margin Y
        static constexpr int bS = 18; // box size
        static constexpr int rd = 8;  // round size
        static constexpr int mB = 10; // box margin(X)

        String caption{};
        int len = 0;

        PressFunction updateFunc = nullptr;

        bool checked = false;

        //
        void draw() override
        {
            bool isF = isFocused();
            constexpr auto boxOfs = 1;
            constexpr auto checkOfs = 2;
            constexpr auto baseFill = bS + boxOfs * 2;
            constexpr auto checkSize = bS - checkOfs * 2;
            constexpr auto textX = bS + mX + mB;
            const auto ofsY = (h - baseFill) / 2;
            gfx->fillRoundRect(x, y, w, h, rd, isF ? TFT_BLUE : TFT_WHITE);
            gfx->fillRect(x + mX, y + ofsY, baseFill, baseFill, TFT_WHITE);      // base
            gfx->drawRect(x + mX + boxOfs, y + ofsY + boxOfs, bS, bS, TFT_BLUE); // box
            if (checked)
            {
                auto ofs = checkOfs + boxOfs;
                gfx->fillRect(x + mX + ofs, y + ofsY + ofs, checkSize, checkSize, TFT_BLUE);
            }
            gfx->setTextColor(isF ? TFT_WHITE : TFT_BLACK);
            gfx->drawString(caption, x + textX, y + mY);
        }
        //
        void onPressed(int, int) override
        {
            checked = !checked;
            if (updateFunc)
                updateFunc(this);
            update();
        }

    public:
        ~CheckBox() = default;

        //
        void setCaption(String c) override
        {
            caption = c;
            len = utf8len(c.c_str());

            int width = len * context->fontWidth + mX * 2 + bS + mB;
            int height = context->fontHeight + mY * 2;
            if (width > w)
                w = width;
            if (height > h)
                h = height;

            update();
        }

        //
        void setUpdateFunction(PressFunction pf)
        {
            updateFunc = pf;
        }
    };

    //
    // UI:リストボックス
    //
    class ListBox : public Widget
    {
        static constexpr int mX = 5; // margin X
        static constexpr int mY = 5; // margin Y

        const char **strList = nullptr;
        size_t nbList = 0;
        size_t capacity = 0;
        size_t selected = -1;
        SelectFunction selectFunc = nullptr;
        portMUX_TYPE listMux = portMUX_INITIALIZER_UNLOCKED;

        void draw() override
        {
            gfx->drawRect(x, y, w, h, TFT_WHITE);
            int dy = y;
            portENTER_CRITICAL(&listMux);
            for (size_t i = 0; i < nbList; i++)
            {
                int fg = i == selected ? TFT_BLACK : TFT_WHITE;
                int bg = i == selected ? TFT_ORANGE : TFT_BLACK;
                gfx->setTextColor(fg);
                gfx->fillRect(x + mX, dy + mY, w - mX * 2, context->fontHeight, bg);
                gfx->drawString(strList[i], x + mX, dy + mY);
                dy += context->fontHeight + mY;
            }
            portEXIT_CRITICAL(&listMux);
        }
        void onPressed(int, int ofsy) override
        {
            size_t sel = ofsy / (context->fontHeight + mY);
            if (sel < nbList)
            {
                if (sel != selected)
                {
                    update();
                    selected = sel;
                }
                else if (selectFunc)
                    selectFunc(selected, strList[selected]);
            }
        }

    public:
        ~ListBox() = default;

        //
        void setSelectFunction(SelectFunction sf)
        {
            selectFunc = sf;
        }

        //
        void init(size_t n, int width = 0)
        {
            if (n != nbList)
            {
                if (strList)
                    free(strList);
                strList = (const char **)malloc(sizeof(const char *) * n);
                nbList = 0;
                capacity = n;
                for (int i = 0; i < n; i++)
                    strList[n] = nullptr;
                h = (context->fontHeight + mY) * n + mY;
                w = 0;
            }
            if (width != 0)
            {
                w = width;
            }
        }
        void clear()
        {
            nbList = 0;
            capacity = 0;
            strList = nullptr;
        }
        bool append(const char *s)
        {
            if (strList && nbList < capacity)
            {
                portENTER_CRITICAL(&listMux);
                strList[nbList++] = s;
                int width = utf8len(s) * context->fontWidth;
                if (w < width)
                    w = width;
                portEXIT_CRITICAL(&listMux);
                return true;
            }
            return false;
        }
        size_t size() const { return nbList; }
        const char *operator[](size_t idx) const
        {
            if (idx < nbList)
                return strList[idx];
            return nullptr;
        }
        void erase(size_t idx)
        {
            if (idx < nbList)
            {
                portENTER_CRITICAL(&listMux);
                for (int i = idx; i < nbList - 1; i++)
                    strList[i] = strList[i + 1];
                nbList -= 1;
                portEXIT_CRITICAL(&listMux);
            }
        }
    };
}
