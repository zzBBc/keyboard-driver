package io.keymapper

import android.app.Activity
import android.content.Intent
import android.graphics.Color
import android.provider.Settings
import android.view.Gravity
import android.view.ViewGroup
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

/** The GUI: the program's own web page, served on 127.0.0.1, once the accessibility service is on. */
class MainActivity : Activity() {
    private var web: WebView? = null

    override fun onResume() {
        super.onResume()
        if (KeymapperService.instance == null) showSetup() else showGui()
    }

    private fun showSetup() {
        web = null
        val message = TextView(this).apply {
            text = "Turn on the Keymapper accessibility service to remap the keys of a hardware keyboard."
            textSize = 18f
            gravity = Gravity.CENTER
        }
        val button = Button(this).apply {
            text = "Open accessibility settings"
            setOnClickListener { startActivity(Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS)) }
        }
        setContentView(LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(48, 48, 48, 48)
            addView(message)
            addView(button)
        })
    }

    private fun showGui() {
        if (web != null) return
        web = WebView(this).apply {
            setBackgroundColor(Color.WHITE)
            settings.javaScriptEnabled = true
            webViewClient = WebViewClient()
            layoutParams = ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)
            loadUrl("http://127.0.0.1:8765")
        }
        setContentView(web)
    }
}
