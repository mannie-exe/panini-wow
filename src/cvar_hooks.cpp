#include "panini.h"

// WotLK CVar functions at the addresses in wow_offsets.h are __cdecl wrappers
// that load the CVarSystem singleton (0x00CA19FC) internally. No hook or
// singleton capture is needed; the DLL calls them directly.
