#pragma once

#include <cstdint>
#include <span>

// Helpers for redirecting E8 rel32 call instructions in the game's code to plugin functions.

// The game module's .text section, or an empty span if it can't be found.
std::span<const uint8_t> TextSection(uint8_t* module);

// Points the call at replacement through a jump stub allocated within rel32 reach, saving the
// call's original displacement. Returns false (leaving the call untouched) if no memory near
// the call could be allocated.
bool RedirectCall(uint8_t* call, void* replacement, int32_t& originalDisplacement);

// Restores a call redirected by RedirectCall.
void RestoreCall(uint8_t* call, int32_t originalDisplacement);
