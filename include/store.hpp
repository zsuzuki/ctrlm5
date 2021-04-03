///
/// Copyright Y.Suzuki 2021
/// wave.suzuki.z@gmail.com
///
#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <vector>

namespace Store
{

    ///
    using Header = char[4];
    class Data
    {
        struct Index
        {
            int pos;
            int size;
        };
        std::vector<Index> indices;
        int writePoint;

    public:
        void init(const Header h, size_t sz)
        {
            EEPROM.begin(sz);
            bool init = false;
            size_t hsz = sizeof(Header);
            for (int i = 0; i < hsz; i++)
            {
                auto ch = EEPROM.readChar(i);
                if (h[i] != ch)
                {
                    init = true;
                    break;
                }
            }
            indices.reserve(10);
            if (init)
            {
                // 初回使用→初期化
                EEPROM.writeBytes(0, h, hsz);
                EEPROM.writeByte(hsz, 0);
                indices.resize(0);
                Serial.println("initialize EEPROM");
                writePoint = hsz + 1;
            }
            else
            {
                char buff[24];
                auto n = EEPROM.readByte(hsz);
                indices.resize(n);
                snprintf(buff, sizeof(buff), "setup EEPROM: num=%d", n);
                Serial.println(buff);
                int p = hsz + 1;
                for (int i = 0; i < n; i++)
                {
                    auto &idx = indices[i];
                    idx.size = EEPROM.readByte(p++);
                    idx.pos = p;
                    p += idx.size;
                    snprintf(buff, sizeof(buff), "data: %d(size=%d)", idx.pos, idx.size);
                    Serial.println(buff);
                }
                writePoint = p;
            }
        }
        void clearIndex()
        {
            indices.resize(0);
            writePoint = sizeof(Header);
            EEPROM.writeByte(writePoint++, 0);
            Serial.println("clear EEPROM index");
        }
        int storeString(const char *str)
        {
            if (indices.size() < indices.capacity())
            {
                Index idx;
                int id = indices.size();
                idx.pos = writePoint;
                idx.size = strlen(str);
                EEPROM.writeByte(writePoint++, idx.size);
                EEPROM.writeBytes(writePoint, str, idx.size);
                writePoint += idx.size;
                indices.push_back(idx);
                EEPROM.writeByte(sizeof(Header), indices.size());
                EEPROM.commit();
                Serial.println(str);
                char buff[48];
                snprintf(buff, sizeof(buff), "store: id=%d,pos=%d,size=%d", id, idx.pos, idx.size);
                Serial.println(buff);
                return id;
            }
            Serial.println("store: failed");
            return -1; // full
        }
        bool loadString(int i, char *buff, size_t buffsize) const
        {
            if (i < indices.size())
            {
                auto &idx = indices[i];
                memset(buff, '\0', buffsize);
                size_t rsz = buffsize - 1;
                size_t sz = idx.size < rsz ? idx.size : rsz;
                EEPROM.readBytes(idx.pos, buff, sz);
                char buff[48];
                snprintf(buff, sizeof(buff), "load: pos=%d,size=%d", idx.pos, idx.size);
                Serial.println(buff);
                return true;
            }
            Serial.println("load: no data");
            return false;
        }
    };
}
