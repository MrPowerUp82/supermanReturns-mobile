#include <cstdint>
// Standalone packet replay tests do not create a guest display or a gamma ramp.
namespace superman_returns::native {
bool GuestGammaRamp256(uint32_t*) {return false;}
}
