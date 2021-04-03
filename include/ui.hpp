///
/// Copyright Y.Suzuki 2021
/// wave.suzuki.z@gmail.com
///
#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <vector>
#include <array>

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
        std::vector<char> clipboard;

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
        void copy(std::vector<char> &src)
        {
            if (clipboard.capacity() == 0)
                clipboard.reserve(48);
            clipboard = src;
        }
        void paste(std::vector<char> &dst, std::vector<char>::iterator &it)
        {
            for (auto c : clipboard)
            {
                if (dst.size() < dst.capacity())
                    dst.insert(it++, c);
                else
                    break;
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

        std::vector<String> strList;
        size_t selected = -1;
        SelectFunction selectFunc = nullptr;
        portMUX_TYPE listMux = portMUX_INITIALIZER_UNLOCKED;

        void draw() override
        {
            gfx->drawRect(x, y, w, h, TFT_WHITE);
            int dy = y;
            portENTER_CRITICAL(&listMux);
            for (size_t i = 0; i < strList.size(); i++)
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
            portENTER_CRITICAL(&listMux);
            if (sel < strList.size())
            {
                if (sel != selected)
                {
                    update();
                    selected = sel;
                }
                else if (selectFunc)
                    selectFunc(selected, strList[selected].c_str());
            }
            portEXIT_CRITICAL(&listMux);
        }

    public:
        ~ListBox() = default;

        //
        void setSelectFunction(SelectFunction sf)
        {
            selectFunc = sf;
        }

        //
        void init(size_t n, int width = 0, int height = 0)
        {
            portENTER_CRITICAL(&listMux);
            if (height == 0)
                height = n * (context->fontHeight + mY) + mY;

            if (n > strList.capacity())
            {
                strList.reserve(n);
                strList.resize(0);
                w = 0;
            }
            if (width != 0)
            {
                w = width;
            }
            h = height;
            portEXIT_CRITICAL(&listMux);
        }
        void clear()
        {
            portENTER_CRITICAL(&listMux);
            strList.resize(0);
            update();
            portEXIT_CRITICAL(&listMux);
        }
        bool append(const char *s)
        {
            bool ret = false;
            portENTER_CRITICAL(&listMux);
            if (strList.size() < strList.capacity())
            {
                strList.push_back(s);
                int width = utf8len(s) * context->fontWidth;
                if (w < width)
                    w = width;
                ret = true;
                update();
            }
            portEXIT_CRITICAL(&listMux);
            return ret;
        }
        size_t size() const { return strList.size(); }
        const char *operator[](size_t idx) const
        {
            if (idx < strList.size())
                return strList[idx].c_str();
            return nullptr;
        }
        void erase(size_t idx)
        {
            portENTER_CRITICAL(&listMux);
            if (idx < strList.size())
            {
                strList.erase(strList.begin() + idx);
                update();
            }
            portEXIT_CRITICAL(&listMux);
        }
    };

    //
    // UI:キーボード
    //
    class Keyboard : public Widget
    {
        static constexpr int mX = 5; // margin X
        static constexpr int mY = 5; // margin Y

        enum class Type : uint8_t
        {
            Char,
            Enter,
            Space,
            BackSpace,
            Delete,
            Left,
            Right,
            Clear,
            Copy,
            Paste,
            Home,
            End,
            PWMode,
            Layer1,
            Layer2,
            Layer3,
        };
        struct CharInfo
        {
            Type type = Type::Char;
            const char *dispChar = "a";
            int code = 'a';
            int size = 1;
            CharInfo() = default;
            CharInfo(int c, const char *d) : dispChar(d), code(c) {}
            CharInfo(Type t, int s, const char *d) : type(t), dispChar(d), code('\0'), size(s) {}
        };
        using CharLine = std::vector<CharInfo>;
        using CharLayer = std::array<CharLine, 5>;

        String title{};
        String placeHolder = "Place Holder";
        std::vector<char> body;
        decltype(body)::iterator editIdx;
        int layer = 0;
        const CharInfo *current = nullptr;
        bool passwordMode = false;

        const CharLayer &getLayer() const
        {
            static const CharLayer defaultLayer = {
                {{
                     {'1', "１"},
                     {'2', "２"},
                     {'3', "３"},
                     {'4', "４"},
                     {'5', "５"},
                     {'6', "６"},
                     {'7', "７"},
                     {'8', "８"},
                     {'9', "９"},
                     {'0', "０"},
                 },
                 {{'q', "ｑ"}, {'w', "ｗ"}, {'e', "ｅ"}, {'r', "ｒ"}, {'t', "ｔ"}, {'y', "ｙ"}, {'u', "ｕ"}, {'i', "ｉ"}, {'o', "ｏ"}, {'p', "ｐ"}},
                 {{'a', "ａ"}, {'s', "ｓ"}, {'d', "ｄ"}, {'f', "ｆ"}, {'g', "ｇ"}, {'h', "ｈ"}, {'j', "ｊ"}, {'k', "ｋ"}, {'l', "ｌ"}, {'.', "."}},
                 {{'z', "ｚ"},
                  {'x', "ｘ"},
                  {'c', "ｃ"},
                  {'v', "ｖ"},
                  {'b', "ｂ"},
                  {'n', "ｎ"},
                  {'m', "ｍ"},
                  {'@', "＠"},
                  {Type::BackSpace, 2, "BS"}},
                 {{Type::Layer2, 2, "ABC"}, {Type::Space, 2, "SPC"}, {Type::Layer3, 2, "+="}, {Type::Left, 2, "←"}, {Type::Right, 2, "→"}}}};
            if (layer == 0)
                return defaultLayer;
            static const CharLayer shiftLayer = {
                {{
                     {'!', "！"},
                     {'"', "”"},
                     {'#', "＃"},
                     {'$', "＄"},
                     {'%', "％"},
                     {'&', "＆"},
                     {'\'', "’"},
                     {'(', "（"},
                     {')', "）"},
                     {'^', "＾"},
                 },
                 {{'Q', "Ｑ"}, {'W', "Ｗ"}, {'E', "Ｅ"}, {'R', "Ｒ"}, {'T', "Ｔ"}, {'Y', "Ｙ"}, {'U', "Ｕ"}, {'I', "Ｉ"}, {'O', "Ｏ"}, {'P', "Ｐ"}},
                 {{'A', "Ａ"}, {'S', "Ｓ"}, {'D', "Ｄ"}, {'F', "Ｆ"}, {'G', "Ｇ"}, {'H', "Ｈ"}, {'J', "Ｊ"}, {'K', "Ｋ"}, {'L', "Ｌ"}, {';', "；"}},
                 {
                     {'Z', "Ｚ"},
                     {'X', "Ｘ"},
                     {'C', "Ｃ"},
                     {'V', "Ｖ"},
                     {'B', "Ｂ"},
                     {'N', "Ｎ"},
                     {'M', "Ｍ"},
                     {'=', "＝"},
                     {Type::BackSpace, 2, "BS"},
                 },
                 {{Type::Layer1, 2, "abc"}, {Type::Space, 2, "SPC"}, {Type::Layer3, 2, "+="}, {Type::Left, 2, "←"}, {Type::Right, 2, "→"}}}};
            if (layer == 1)
                return shiftLayer;
            static const CharLayer symbolLayer = {
                {{
                     {'+', "＋"},
                     {'-', "ー"},
                     {'/', "／"},
                     {'*', "＊"},
                     {'=', "＝"},
                     {':', "："},
                     {'[', "［"},
                     {']', "］"},
                     {'<', "＜"},
                     {'>', "＞"},
                 },
                 {
                     {'{', "｛"},
                     {'}', "｝"},
                     {'?', "？"},
                     {'_', "＿"},
                     {'|', "｜"},
                     {'~', "〜"},
                     {'\\', "￥"},
                     {',', "，"},
                     {'`', "｀"},
                     {'@', "＠"},
                 },
                 {
                     {'!', "！"},
                     {'"', "”"},
                     {'#', "＃"},
                     {'$', "＄"},
                     {'%', "％"},
                     {'&', "＆"},
                     {'\'', "’"},
                     {'(', "（"},
                     {')', "）"},
                     {'^', "＾"},
                 },
                 {
                     {'.', "．"},
                     {';', "；"},
                     {Type::Copy, 2, "写"},
                     {Type::Paste, 2, "貼"},
                     {Type::Clear, 2, "Clr"},
                     {Type::BackSpace, 2, "BS"},
                 },
                 {{Type::Layer1, 2, "abc"}, {Type::Space, 2, "SPC"}, {Type::Layer2, 2, "ABC"}, {Type::Left, 2, "←"}, {Type::Right, 2, "→"}}}};
            return symbolLayer;
        }

        void draw() override
        {
            const auto &nl = getLayer();
            int dy = y + mY;
            gfx->fillRect(x, y, w, context->fontHeight + mY, TFT_BLACK);
            gfx->drawRect(x, y, w, context->fontHeight + mY, TFT_WHITE);
            int fontW = context->fontWidth;
            {
                // カーソル表示
                int curIdx = std::distance(body.begin(), editIdx);
                int cy = dy + context->fontHeight - 2;
                int bx = x + curIdx * fontW + mX;
                gfx->fillRect(bx, cy, fontW, 2, TFT_GREEN);
            }
            if (body.empty())
            {
                // 未入力状態のプレースホルダー表示
                gfx->setTextColor(TFT_DARKGRAY);
                gfx->drawString(placeHolder, x + mX, dy);
            }
            else
            {
                // 編集文字列
                gfx->setTextColor(passwordMode ? TFT_RED : TFT_WHITE);
                int dx = x + mX;
                for (auto c : body)
                {
                    if (passwordMode)
                        c = '*';
                    gfx->drawChar(c, dx, dy + context->fontHeight - mY);
                    dx += context->fontWidth;
                }
            }
            //
            dy += context->fontHeight + mY;
            int w1 = context->fontWidth * 2 + mX;
            for (const auto &l : nl)
            {
                int dx = x;
                for (const auto &c : l)
                {
                    int dw = w1 * c.size - mX;
                    int dh = context->fontHeight;
                    int len = utf8len(c.dispChar);
                    int ofs = max(0, (w1 * c.size - fontW * len) / 2 - mX);
                    bool sel = current == &c;
                    gfx->fillRect(dx, dy, dw, dh, sel ? TFT_SKYBLUE : TFT_WHITE);
                    gfx->setTextColor(TFT_BLACK);
                    gfx->drawString(c.dispChar, dx + ofs, dy);
                    dx += dw + mX;
                }
                dy += context->fontHeight + mY;
            }
        }

        void onPressed(int ofsx, int ofsy) override
        {
            int rh = context->fontHeight + mY;
            int sy = (ofsy - rh) / rh;
            int sx = ofsx / (context->fontWidth * 2 + mX);
            auto insert = [&](int ch) {
                if (body.size() < body.capacity())
                    body.insert(editIdx++, ch);
            };
            auto chgLayer = [&](int l) {
                layer = l;
                gfx->fillRect(x, y + rh, w, h - rh, TFT_BLACK);
            };

            const auto &ly = getLayer();
            current = nullptr;
            // 選択する文字を検索
            for (const auto &l : ly)
            {
                int ssx = sx;
                for (const auto &c : l)
                {
                    if (ssx >= 0 && ssx < c.size && sy == 0)
                    {
                        current = &c;
                        break;
                    }
                    ssx -= c.size;
                }
                if (current)
                    break;
                sy -= 1;
            }
            //
            if (current)
            {
                if (current->type != Type::Char)
                {
                    switch (current->type)
                    {
                    case Type::Space:
                        insert(' ');
                        break;
                    case Type::Layer1:
                        chgLayer(0);
                        break;
                    case Type::Layer2:
                        chgLayer(1);
                        break;
                    case Type::Layer3:
                        chgLayer(2);
                        break;
                    case Type::Left:
                        if (editIdx != body.begin())
                            --editIdx;
                        break;
                    case Type::Right:
                        if (editIdx < body.end())
                            editIdx++;
                        break;
                    case Type::BackSpace:
                        if (editIdx != body.begin())
                            body.erase(--editIdx);
                        break;
                    case Type::Delete:
                        if (body.empty() == false && editIdx != body.end())
                            body.erase(editIdx);
                        break;
                    case Type::Home:
                        editIdx = body.begin();
                        break;
                    case Type::End:
                        editIdx = body.end();
                        break;
                    case Type::Clear:
                        body.resize(0);
                        editIdx = body.begin();
                        break;
                    case Type::Copy:
                        context->copy(body);
                        break;
                    case Type::Paste:
                        context->paste(body, editIdx);
                        break;
                    case Type::PWMode:
                        passwordMode = !passwordMode;
                        break;
                    default:
                        break;
                    }
                }
                else
                    insert(current->code);
            }
            update();
        }

    public:
        ~Keyboard() = default;

        void init(size_t cap = 16)
        {
            if (cap > 32)
                cap = 32;
            w = (context->fontWidth * 2 + mX) * 10;
            h = (context->fontHeight + mY) * 6;
            body.reserve(cap);
            body.resize(0);
            editIdx = body.begin();
        }

        void setPlaceHolder(const char *ph)
        {
            placeHolder = ph;
        }

        void setString(const char *buff)
        {
            int len = strlen(buff);
            if (len > body.capacity())
                len = body.capacity();
            body.resize(len);
            editIdx = body.end();
            memcpy(body.data(), buff, len);
            // Serial.println(buff);
        }

        void getString(char *buff, size_t buffsize) const
        {
            auto sz = min(body.size(), buffsize);
            memset(buff, '\0', buffsize);
            memcpy(buff, body.data(), sz);
        }

        void setPasswordMode(bool md)
        {
            passwordMode = md;
        }
    };
}
