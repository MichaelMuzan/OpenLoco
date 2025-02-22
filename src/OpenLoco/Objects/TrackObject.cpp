#include "TrackObject.h"
#include "../Graphics/Colour.h"
#include "../Graphics/Gfx.h"
#include "../Interop/Interop.hpp"

namespace OpenLoco
{
    // 0x004A6CBA
    void TrackObject::drawPreviewImage(Gfx::RenderTarget& rt, const int16_t x, const int16_t y) const
    {
        auto colourImage = Gfx::recolour(image, Colour::mutedDarkRed);

        Gfx::drawImage(&rt, x, y, colourImage + 18);
        Gfx::drawImage(&rt, x, y, colourImage + 20);
        Gfx::drawImage(&rt, x, y, colourImage + 22);
    }

    // 0x004A6C6C
    bool TrackObject::validate() const
    {
        if (var_06 >= 3)
        {
            return false;
        }

        // vanilla missed this check
        if (costIndex > 32)
        {
            return false;
        }

        if (-sellCostFactor > buildCostFactor)
        {
            return false;
        }
        if (buildCostFactor <= 0)
        {
            return false;
        }
        if (tunnelCostFactor <= 0)
        {
            return false;
        }
        if ((trackPieces & ((1 << 0) | (1 << 1))) && (trackPieces & ((1 << 7) | (1 << 4))))
        {
            return false;
        }
        if (numBridges > 7)
        {
            return false;
        }
        return numStations <= 7;
    }

    // 0x004A6A5F
    void TrackObject::load(const LoadedObjectHandle& handle, stdx::span<std::byte> data)
    {
        Interop::registers regs;
        regs.esi = Interop::X86Pointer(this);
        regs.ebx = handle.id;
        regs.ecx = enumValue(handle.type);
        Interop::call(0x004A6A5F, regs);
    }

    // 0x004A6C2D
    void TrackObject::unload()
    {
        name = 0;
        var_10 = 0;
        std::fill(std::begin(mods), std::end(mods), 0);
        var_0E = 0;
        var_1B = 0;
        image = 0;
        std::fill(std::begin(bridges), std::end(bridges), 0);
        std::fill(std::begin(stations), std::end(stations), 0);
    }
}
