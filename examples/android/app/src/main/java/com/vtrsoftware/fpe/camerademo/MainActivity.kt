package com.vtrsoftware.fpe.camerademo

import android.Manifest
import android.app.Activity
import android.content.pm.PackageManager
import android.graphics.Typeface
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.TextView

/**
 * Minimal FrameProcessorEngine demo: the back camera renders into FPE's AImageReader; two tasks
 * run per frame (mean luma, motion energy); the screen shows fps, latency and drops.
 * No preview on purpose: everything the camera produces goes to the engine.
 */
class MainActivity : Activity() {
    private lateinit var text: TextView
    private val handler = Handler(Looper.getMainLooper())
    private var running = false

    private val refresh = object : Runnable {
        override fun run() {
            if (running) {
                text.text = NativeDemo.stats()
                handler.postDelayed(this, 250)
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        text = TextView(this).apply {
            typeface = Typeface.MONOSPACE
            textSize = 15f
            setPadding(48, 96, 48, 48)
            text = "FrameProcessorEngine camera demo"
        }
        setContentView(text)
    }

    override fun onResume() {
        super.onResume()
        if (checkSelfPermission(Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
            startEngine()
        } else {
            requestPermissions(arrayOf(Manifest.permission.CAMERA), REQUEST_CAMERA)
        }
    }

    override fun onPause() {
        stopEngine()
        super.onPause()
    }

    override fun onRequestPermissionsResult(code: Int, permissions: Array<out String>, results: IntArray) {
        super.onRequestPermissionsResult(code, permissions, results)
        if (code == REQUEST_CAMERA && results.firstOrNull() == PackageManager.PERMISSION_GRANTED) {
            startEngine()
        } else {
            text.text = "Camera permission is needed for this demo."
        }
    }

    private fun startEngine() {
        if (running) return
        val error = NativeDemo.start()
        if (error != null) {
            text.text = "Could not start: $error"
            return
        }
        running = true
        handler.post(refresh)
    }

    private fun stopEngine() {
        if (!running) return
        running = false
        handler.removeCallbacks(refresh)
        NativeDemo.stop()
    }

    private companion object {
        const val REQUEST_CAMERA = 1
    }
}
