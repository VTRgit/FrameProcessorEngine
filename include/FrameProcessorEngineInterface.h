#pragma once

namespace fpe {

enum class Response { noError };
}

extern "C" fpe::Response startFPE();
extern "C" fpe::Response stopFPE();
