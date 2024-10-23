#pragma once

namespace FPE {

enum class Response { noError };
}

extern "C" FPE::Response startFPE();
extern "C" FPE::Response stopFPE();
