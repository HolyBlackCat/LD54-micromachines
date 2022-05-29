#pragma once

#include <algorithm>
#include <array>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

#include "audio/complete.h"
#include "gameutils/adaptive_viewport.h"
#include "gameutils/fps_counter.h"
#include "gameutils/render.h"
#include "gameutils/state.h"
#include "graphics/complete.h"
#include "input/complete.h"
#include "interface/gui.h"
#include "interface/window.h"
#include "macros/adjust.h"
#include "macros/enum_flag_operators.h"
#include "macros/finally.h"
#include "macros/qualifiers.h"
#include "macros/named_loops.h"
#include "meta/common.h"
#include "program/compiler.h"
#include "program/entry_point.h"
#include "program/errors.h"
#include "program/exe_path.h"
#include "program/exit.h"
#include "program/main_loop.h"
#include "program/platform.h"
#include "reflection/full_with_poly.h"
#include "reflection/short_macros.h"
#include "strings/common.h"
#include "strings/format.h"
#include "strings/lexical_cast.h"
#include "utils/clock.h"
#include "utils/hash.h"
#include "utils/mat.h"
#include "utils/metronome.h"
#include "utils/multiarray.h"
#include "utils/poly_storage.h"
#include "utils/random.h"
#include "utils/simple_iterator.h"
