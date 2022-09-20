#include "../Graphics/Gfx.h"

#include <cstdint>
#include <string>

namespace OpenLoco::Ui::TextInput
{
    constexpr int16_t textboxPadding = 4;

    struct InputSession
    {
        std::string buffer;    // 0x011369A0
        size_t cursorPosition; // 0x01136FA2
        int16_t xOffset;       // 0x01136FA4
        uint8_t cursorFrame;   // 0x011370A9
        uint8_t maxAmountOfCharacters;

        InputSession() = default;
        InputSession(const std::string initialString)
        {
            buffer = initialString;
            cursorPosition = buffer.length();
            cursorFrame = 0;
            xOffset = 0;
            maxAmountOfCharacters = 199;
        };

        bool handleInput(uint32_t charCode, uint32_t keyCode);
        bool needsReoffsetting(int16_t containerWidth);
        void calculateTextOffset(int16_t containerWidth);
        void sanitizeInput();
        uint8_t getCharactersLeft();
    };
}
