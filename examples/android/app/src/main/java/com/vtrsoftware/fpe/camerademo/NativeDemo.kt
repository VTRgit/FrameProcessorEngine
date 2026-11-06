package com.vtrsoftware.fpe.camerademo

/** JNI bridge to demo.cpp: back camera → FrameProcessorEngine, stats as text. */
object NativeDemo {
    init {
        System.loadLibrary("camerademo")
    }

    /** Opens the back camera and starts processing. Returns an error message or null. */
    external fun start(): String?

    external fun stop()

    /** One-line human-readable stats (fps, latency, drops, task outputs). */
    external fun stats(): String
}
